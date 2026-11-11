// DOR2Prepare - headless GameCube disc extractor for DOR2 Static Recomp.
//
// Extracts a retail GameCube ISO into the layout the ModernGekko runner
// expects:
//
//   <output>/sys/boot.bin
//   <output>/sys/bi2.bin
//   <output>/sys/apploader.img
//   <output>/sys/fst.bin
//   <output>/sys/main.dol
//   <output>/files/...            (FST file tree)
//
// It prints RecompLauncher-compatible "progress <0..1> <text>" lines, verifies
// the disc ID and the retail DOL SHA-256, and never writes inside the disc
// image.
//
// This tool contains no game data. It only reads a disc the user supplies.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{

constexpr std::uint32_t kGcMagic = 0xC2339F3Du;
constexpr double kVersion = 0.1;

// Retail WWE Day of Reckoning 2 (USA).
constexpr std::string_view kExpectedDiscId = "GW2E78";
constexpr std::string_view kExpectedDolSha256 =
    "e47abf73e2c7162098c01de34b90ab66031c3f161fd72b36dde0173ea4af3610";

void Progress(double fraction, std::string_view text)
{
  fraction = std::clamp(fraction, 0.0, 1.0);
  std::cout << "progress " << std::fixed << std::setprecision(3) << fraction
            << ' ' << text << std::endl;
}

int Fail(std::string_view message, int code = 1)
{
  std::cerr << "error: " << message << '\n';
  return code;
}

std::string ToHex(const std::uint8_t* data, std::size_t size)
{
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(size * 2);
  for (std::size_t i = 0; i < size; ++i)
  {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

std::string ToLower(std::string value)
{
  for (char& c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

std::uint32_t ReadBE32(const std::uint8_t* p)
{
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}

std::uint16_t ReadBE16(const std::uint8_t* p)
{
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), small standalone implementation.
// ---------------------------------------------------------------------------
class Sha256
{
public:
  Sha256() { Reset(); }

  void Reset()
  {
    h_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    length_ = 0;
    buffer_.clear();
    buffer_.reserve(64);
  }

  void Update(const void* data, std::size_t size)
  {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    length_ += size;
    while (size > 0)
    {
      const std::size_t take = std::min<std::size_t>(64 - buffer_.size(), size);
      buffer_.insert(buffer_.end(), bytes, bytes + take);
      bytes += take;
      size -= take;
      if (buffer_.size() == 64)
      {
        Transform(buffer_.data());
        buffer_.clear();
      }
    }
  }

  std::array<std::uint8_t, 32> Finish()
  {
    const std::uint64_t bit_length = length_ * 8;
    buffer_.push_back(0x80);
    while (buffer_.size() != 56)
      buffer_.push_back(0);
    for (int i = 7; i >= 0; --i)
      buffer_.push_back(static_cast<std::uint8_t>((bit_length >> (i * 8)) & 0xFF));
    Transform(buffer_.data());

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t i = 0; i < 8; ++i)
    {
      digest[i * 4 + 0] = static_cast<std::uint8_t>(h_[i] >> 24);
      digest[i * 4 + 1] = static_cast<std::uint8_t>(h_[i] >> 16);
      digest[i * 4 + 2] = static_cast<std::uint8_t>(h_[i] >> 8);
      digest[i * 4 + 3] = static_cast<std::uint8_t>(h_[i]);
    }
    return digest;
  }

private:
  static std::uint32_t Rotr(std::uint32_t x, int n)
  {
    return (x >> n) | (x << (32 - n));
  }

  void Transform(const std::uint8_t* block)
  {
    static constexpr std::uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
        0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
        0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
        0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
        0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
        0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
        0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
        0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = ReadBE32(block + i * 4);
    for (int i = 16; i < 64; ++i)
    {
      const std::uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
    std::uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];

    for (int i = 0; i < 64; ++i)
    {
      const std::uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
      const std::uint32_t ch = (e & f) ^ (~e & g);
      const std::uint32_t temp1 = h + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = s0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
    h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
  }

  std::array<std::uint32_t, 8> h_{};
  std::uint64_t length_ = 0;
  std::vector<std::uint8_t> buffer_;
};

std::uint32_t Crc32Update(std::uint32_t crc, const std::uint8_t* data, std::size_t size)
{
  static const auto table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i)
    {
      std::uint32_t c = i;
      for (int bit = 0; bit < 8; ++bit)
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      t[i] = c;
    }
    return t;
  }();

  crc = ~crc;
  for (std::size_t i = 0; i < size; ++i)
    crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

// ---------------------------------------------------------------------------
// Shift-JIS -> UTF-8 for disc file names.
// ---------------------------------------------------------------------------
std::string SjisToUtf8(const std::string& input)
{
#if defined(_WIN32)
  if (input.empty())
    return {};
  const int wide_length =
      MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, input.data(),
                          static_cast<int>(input.size()), nullptr, 0);
  if (wide_length <= 0)
    return input;
  std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
  MultiByteToWideChar(932, 0, input.data(), static_cast<int>(input.size()),
                      wide.data(), wide_length);
  const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                              wide_length, nullptr, 0, nullptr, nullptr);
  if (utf8_length <= 0)
    return input;
  std::string utf8(static_cast<std::size_t>(utf8_length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_length, utf8.data(),
                      utf8_length, nullptr, nullptr);
  return utf8;
#else
  return input;
#endif
}

std::string SanitizeComponent(const std::string& name)
{
  std::string out = name;
  for (char& c : out)
  {
    switch (c)
    {
    case '/': case '\\': case ':': case '*': case '?':
    case '"': case '<': case '>': case '|':
      c = '_';
      break;
    default:
      break;
    }
  }
  if (out == "." || out == "..")
    out = "_";
  return out;
}

// ---------------------------------------------------------------------------
// Disc structures.
// ---------------------------------------------------------------------------
struct FstEntry
{
  std::string name;
  bool directory = false;
  std::uint32_t offset = 0;  // absolute disc offset for files
  std::uint32_t size = 0;    // bytes for files
  std::vector<FstEntry> children;
  std::uint64_t total_bytes = 0;
};

struct DiscInfo
{
  std::string disc_id;
  std::string title;
  std::uint64_t iso_size = 0;
  std::uint32_t iso_crc32 = 0;
  std::uint32_t dol_offset = 0;
  std::uint32_t dol_size = 0;
  std::uint32_t fst_offset = 0;
  std::uint32_t fst_size = 0;
  std::uint32_t apploader_offset = 0x2440;
  std::uint32_t apploader_size = 0;
  std::string dol_sha256;
};

bool ReadAt(std::ifstream& file, std::uint64_t offset, void* data, std::size_t size)
{
  file.clear();
  file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!file)
    return false;
  file.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
  return static_cast<std::size_t>(file.gcount()) == size;
}

// std::filesystem::path::u8string() yields char8_t on C++20, which does not
// implicitly convert at the call sites that need a plain std::string.
std::string PathToDisplay(const fs::path& path)
{
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// fs::u8path is deprecated in C++20; build the path from UTF-8 bytes directly.
fs::path PathFromUtf8(const std::string& utf8)
{
#if defined(__cpp_lib_char8_t)
  return fs::path(std::u8string(utf8.begin(), utf8.end()));
#else
  return fs::path(utf8);
#endif
}

std::vector<std::uint8_t> ReadFilePrefix(std::ifstream& file, std::uint64_t offset,
                                         std::uint64_t size, std::size_t limit)
{
  const std::size_t take = static_cast<std::size_t>(std::min<std::uint64_t>(size, limit));
  std::vector<std::uint8_t> data(take);
  if (!ReadAt(file, offset, data.data(), take))
    data.clear();
  return data;
}

bool ParseFst(const std::vector<std::uint8_t>& fst, FstEntry& root, std::string& error)
{
  if (fst.size() < 12)
  {
    error = "FST is too small";
    return false;
  }

  const std::uint32_t root_word = ReadBE32(fst.data());
  const std::uint32_t root_type = root_word >> 24;
  // The root entry's trailing field is the total entry count; the entry table
  // ends there and the name string table follows, so the table length must not
  // be derived from the whole FST size.
  const std::uint32_t count = ReadBE32(fst.data() + 8);
  if (count == 0 || static_cast<std::uint64_t>(count) * 12u > fst.size())
  {
    error = "FST root entry count is out of range";
    return false;
  }

  // Names are offsets into the string table, which begins right after the
  // 12-byte-per-entry table.
  const std::size_t name_table = static_cast<std::size_t>(count) * 12u;

  auto name_at = [&](std::uint32_t offset) -> std::string {
    const std::size_t start = name_table + offset;
    if (start >= fst.size())
      return {};
    std::size_t end = start;
    while (end < fst.size() && fst[end] != 0)
      ++end;
    return SjisToUtf8(std::string(reinterpret_cast<const char*>(fst.data() + start),
                                  end - start));
  };

  std::function<std::pair<FstEntry, std::size_t>(std::size_t)> parse;
  parse = [&](std::size_t index) -> std::pair<FstEntry, std::size_t> {
    FstEntry entry;
    if (index >= count)
      return {entry, count};
    const std::uint8_t* p = fst.data() + index * 12;
    const std::uint32_t word = ReadBE32(p);
    const std::uint32_t field1 = ReadBE32(p + 4);
    const std::uint32_t field2 = ReadBE32(p + 8);
    const std::uint32_t type = word >> 24;
    const std::uint32_t name_offset = word & 0x00FFFFFFu;

    entry.name = name_at(name_offset);
    if (type == 1)
    {
      entry.directory = true;
      std::size_t child = index + 1;
      const std::size_t end = std::min<std::size_t>(field2, count);
      while (child < end)
      {
        auto [sub, next] = parse(child);
        if (next <= child)
          break;
        entry.children.push_back(std::move(sub));
        child = next;
      }
      return {std::move(entry), end};
    }

    entry.directory = false;
    entry.offset = field1;
    entry.size = field2;
    return {std::move(entry), index + 1};
  };

  if (root_type != 1)
  {
    error = "FST root is not a directory";
    return false;
  }

  auto [parsed_root, next] = parse(0);
  (void)next;
  root = std::move(parsed_root);

  std::function<void(FstEntry&)> compute = [&](FstEntry& entry) {
    if (!entry.directory)
    {
      entry.total_bytes = entry.size;
      return;
    }
    entry.total_bytes = 0;
    for (auto& child : entry.children)
    {
      compute(child);
      entry.total_bytes += child.total_bytes;
    }
  };
  compute(root);

  // Reject entries that try to escape the output directory.
  std::function<bool(const FstEntry&)> safe = [&](const FstEntry& entry) {
    for (const auto& child : entry.children)
    {
      if (child.name == "." || child.name == ".." || child.name.empty())
        return false;
      if (child.name.find('/') != std::string::npos ||
          child.name.find('\\') != std::string::npos)
        return false;
      if (!safe(child))
        return false;
    }
    return true;
  };
  if (!safe(root))
  {
    error = "FST contains an unsafe path";
    return false;
  }
  return true;
}

struct Options
{
  fs::path input;
  fs::path output;
  std::string expect_id = std::string(kExpectedDiscId);
  std::string expect_dol = std::string(kExpectedDolSha256);
  bool force = false;
  bool no_dol_check = false;
};

fs::path ExecutableDirectory()
{
#if defined(_WIN32)
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0)
    return fs::current_path();
  buffer.resize(length);
  return fs::path(buffer).parent_path();
#else
  return fs::current_path();
#endif
}

void PrintUsage()
{
  std::cout
      << "DOR2Prepare " << kVersion << " - WWE Day of Reckoning 2 disc extractor\n\n"
      << "Usage:\n"
      << "  DOR2Prepare --input <disc.iso|extracted-folder> [--output <folder>]\n"
      << "              [--expect-id GW2E78] [--expect-dol <sha256>]\n"
      << "              [--force] [--no-dol-check]\n\n"
      << "Prints RecompLauncher progress lines on stdout. Exit codes:\n"
      << "  0 success, 1 usage/io error, 2 disc validation failed\n";
}

std::optional<Options> ParseOptions(int argc, char** argv, int& exit_code)
{
  Options options;
  options.output = ExecutableDirectory() / "Game";

  for (int i = 1; i < argc; ++i)
  {
    const std::string_view arg = argv[i];
    auto value = [&](const char* name) -> std::optional<std::string> {
      if (i + 1 >= argc)
      {
        std::cerr << "error: " << name << " needs a value\n";
        exit_code = 1;
        return std::nullopt;
      }
      return std::string(argv[++i]);
    };

    if (arg == "--input" || arg == "-i")
    {
      auto v = value("--input");
      if (!v)
        return std::nullopt;
      options.input = PathFromUtf8(*v);
    }
    else if (arg == "--output" || arg == "-o")
    {
      auto v = value("--output");
      if (!v)
        return std::nullopt;
      options.output = PathFromUtf8(*v);
    }
    else if (arg == "--expect-id")
    {
      auto v = value("--expect-id");
      if (!v)
        return std::nullopt;
      options.expect_id = *v;
    }
    else if (arg == "--expect-dol")
    {
      auto v = value("--expect-dol");
      if (!v)
        return std::nullopt;
      options.expect_dol = ToLower(*v);
    }
    else if (arg == "--force")
    {
      options.force = true;
    }
    else if (arg == "--no-dol-check")
    {
      options.no_dol_check = true;
    }
    else if (arg == "--help" || arg == "-h")
    {
      PrintUsage();
      exit_code = 1;
      return std::nullopt;
    }
    else if (arg == "--version")
    {
      std::cout << "DOR2Prepare " << kVersion << '\n';
      exit_code = 1;
      return std::nullopt;
    }
    else
    {
      std::cerr << "error: unknown argument: " << arg << '\n';
      exit_code = 1;
      return std::nullopt;
    }
  }

  if (options.input.empty())
  {
    PrintUsage();
    exit_code = 1;
    return std::nullopt;
  }
  return options;
}

bool ReadMarker(const fs::path& output, std::string& id, std::string& dol_sha)
{
  std::ifstream marker(output / ".dor2-prepared", std::ios::binary);
  if (!marker)
    return false;
  std::string line;
  while (std::getline(marker, line))
  {
    if (line.rfind("disc_id=", 0) == 0)
      id = line.substr(8);
    else if (line.rfind("dol_sha256=", 0) == 0)
      dol_sha = line.substr(11);
  }
  return !id.empty() && !dol_sha.empty();
}

bool WriteMarker(const fs::path& output, const DiscInfo& info)
{
  std::ofstream marker(output / ".dor2-prepared", std::ios::binary | std::ios::trunc);
  if (!marker)
    return false;
  marker << "format=1\n";
  marker << "disc_id=" << info.disc_id << '\n';
  marker << "dol_sha256=" << info.dol_sha256 << '\n';
  marker << "iso_size=" << info.iso_size << '\n';
  marker << "iso_crc32=" << std::hex << std::setw(8) << std::setfill('0')
         << info.iso_crc32 << std::dec << '\n';
  marker << "prepared_by=DOR2Prepare " << kVersion << '\n';
  return true;
}

int PrepareFromFolder(const Options& options)
{
  const fs::path source = options.input;
  const fs::path source_dol = source / "sys" / "main.dol";
  if (!fs::is_regular_file(source_dol))
    return Fail("input folder has no sys/main.dol", 2);
  if (!fs::is_directory(source / "files"))
    return Fail("input folder has no files/ tree", 2);

  // Validate the DOL in place.
  std::ifstream dol_file(source_dol, std::ios::binary);
  if (!dol_file)
    return Fail("cannot open sys/main.dol", 1);
  Sha256 sha;
  std::array<std::uint8_t, 1 << 20> buffer{};
  while (dol_file)
  {
    dol_file.read(reinterpret_cast<char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));
    const std::streamsize got = dol_file.gcount();
    if (got > 0)
      sha.Update(buffer.data(), static_cast<std::size_t>(got));
  }
  const std::string hash = ToHex(sha.Finish().data(), 32);
  if (!options.no_dol_check && hash != options.expect_dol)
    return Fail("DOL SHA-256 does not match the retail USA disc (got " + hash + ")", 2);

  if (fs::equivalent(source, options.output))
  {
    Progress(1.0, "Using existing extraction");
    std::cout << "DOR2PREPARE: disc_id=" << options.expect_id << " dol_sha256=" << hash
              << '\n';
    return 0;
  }

  Progress(0.050, "Copying the extracted game files");
  std::error_code ec;
  fs::remove_all(options.output, ec);
  fs::create_directories(options.output, ec);
  if (ec)
    return Fail("cannot create output folder: " + ec.message(), 1);

  fs::copy(source / "sys", options.output / "sys",
           fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec)
    return Fail("cannot copy sys/: " + ec.message(), 1);
  Progress(0.400, "Copying the extracted game files");
  fs::copy(source / "files", options.output / "files",
           fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec)
    return Fail("cannot copy files/: " + ec.message(), 1);

  DiscInfo info;
  info.disc_id = options.expect_id;
  info.dol_sha256 = hash;
  WriteMarker(options.output, info);
  Progress(1.0, "Extraction complete");
  std::cout << "DOR2PREPARE: disc_id=" << info.disc_id << " dol_sha256=" << hash << '\n';
  return 0;
}

int PrepareFromIso(const Options& options)
{
  std::ifstream iso(options.input, std::ios::binary);
  if (!iso)
    return Fail("cannot open disc image: " + PathToDisplay(options.input), 1);

  std::error_code ec;
  const std::uint64_t iso_size = fs::file_size(options.input, ec);
  if (ec)
    return Fail("cannot read disc image size", 1);

  Progress(0.000, "Opening " + PathToDisplay(options.input.filename()));

  std::array<std::uint8_t, 0x440> header{};
  if (!ReadAt(iso, 0, header.data(), header.size()))
    return Fail("disc image is too small for a GameCube header", 2);

  const std::uint32_t magic = ReadBE32(header.data() + 0x1C);
  if (magic != kGcMagic)
    return Fail("not a GameCube disc image (bad magic)", 2);

  DiscInfo info;
  info.iso_size = iso_size;
  info.disc_id.assign(reinterpret_cast<const char*>(header.data()), 6);
  info.title.assign(reinterpret_cast<const char*>(header.data() + 0x20),
                    strnlen(reinterpret_cast<const char*>(header.data() + 0x20), 0x40));

  const std::uint32_t dol_offset = ReadBE32(header.data() + 0x420);
  info.fst_offset = ReadBE32(header.data() + 0x424);
  info.fst_size = ReadBE32(header.data() + 0x428);

  if (info.disc_id != options.expect_id)
    return Fail("wrong disc: expected " + options.expect_id + ", found " + info.disc_id, 2);

  if (dol_offset == 0 || dol_offset >= iso_size)
    return Fail("invalid DOL offset", 2);

  // DOL header: 7 text offsets at 0x00, 11 data offsets at 0x1C, sizes at
  // 0x90/0xAC. The DOL image extends to the end of its last section.
  std::array<std::uint8_t, 0x100> dol_header{};
  if (!ReadAt(iso, dol_offset, dol_header.data(), dol_header.size()))
    return Fail("cannot read the DOL header", 2);

  std::uint32_t dol_end = 0x100;
  for (int i = 0; i < 7; ++i)
  {
    const std::uint32_t off = ReadBE32(dol_header.data() + i * 4);
    const std::uint32_t size = ReadBE32(dol_header.data() + 0x90 + i * 4);
    if (size != 0)
      dol_end = std::max(dol_end, off + size);
  }
  for (int i = 0; i < 11; ++i)
  {
    const std::uint32_t off = ReadBE32(dol_header.data() + 0x1C + i * 4);
    const std::uint32_t size = ReadBE32(dol_header.data() + 0xAC + i * 4);
    if (size != 0)
      dol_end = std::max(dol_end, off + size);
  }
  if (dol_end > iso_size || dol_offset + dol_end > iso_size)
    return Fail("DOL extends past the end of the disc image", 2);
  info.dol_size = dol_end;

  Progress(0.020, "Verifying the retail DOL");
  std::vector<std::uint8_t> dol(dol_end);
  if (!ReadAt(iso, dol_offset, dol.data(), dol.size()))
    return Fail("cannot read the DOL", 2);

  {
    Sha256 sha;
    sha.Update(dol.data(), dol.size());
    const auto digest = sha.Finish();
    info.dol_sha256 = ToHex(digest.data(), digest.size());
  }
  if (!options.no_dol_check && info.dol_sha256 != options.expect_dol)
    return Fail("DOL SHA-256 does not match the retail USA disc (got " +
                    info.dol_sha256 + ")",
                2);

  // Read and parse the FST.
  Progress(0.050, "Reading the disc file table");
  if (info.fst_offset == 0 || info.fst_size == 0 ||
      info.fst_offset + info.fst_size > iso_size)
    return Fail("invalid FST location", 2);
  std::vector<std::uint8_t> fst(info.fst_size);
  if (!ReadAt(iso, info.fst_offset, fst.data(), fst.size()))
    return Fail("cannot read the FST", 2);

  FstEntry root;
  std::string fst_error;
  if (!ParseFst(fst, root, fst_error))
    return Fail("cannot parse the FST: " + fst_error, 2);

  // Apploader: header (0x20) + payload + trailer.
  std::array<std::uint8_t, 0x20> app_header{};
  if (ReadAt(iso, 0x2440, app_header.data(), app_header.size()))
  {
    info.apploader_offset = 0x2440;
    info.apploader_size = 0x20 + ReadBE32(app_header.data() + 0x14) +
                          ReadBE32(app_header.data() + 0x18);
    if (info.apploader_offset + info.apploader_size > iso_size)
      info.apploader_size = 0;
  }

  Progress(0.060, "Computing the disc checksum");
  {
    // 4 MiB on the heap: the default main-thread stack is too small for this.
    std::vector<std::uint8_t> buffer(4 << 20);
    std::uint32_t crc = 0;
    std::uint64_t done = 0;
    iso.clear();
    iso.seekg(0, std::ios::beg);
    while (iso)
    {
      iso.read(reinterpret_cast<char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));
      const std::streamsize got = iso.gcount();
      if (got <= 0)
        break;
      crc = Crc32Update(crc, buffer.data(), static_cast<std::size_t>(got));
      done += static_cast<std::uint64_t>(got);
      Progress(0.060 + 0.090 * (static_cast<double>(done) / static_cast<double>(iso_size)),
               "Computing the disc checksum");
    }
    info.iso_crc32 = crc;
  }

  // Prepare a staging directory so a failed extraction never leaves a
  // half-written game tree behind.
  fs::path staging = options.output;
  staging += ".extracting";
  fs::remove_all(staging, ec);
  fs::create_directories(staging / "sys", ec);
  if (ec)
    return Fail("cannot create the staging folder: " + ec.message(), 1);

  auto write_file = [&](const fs::path& relative, const void* data, std::size_t size) {
    const fs::path dest = staging / relative;
    fs::create_directories(dest.parent_path(), ec);
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
  };

  Progress(0.160, "Writing system files");
  std::array<std::uint8_t, 0x2000> bi2{};
  if (!ReadAt(iso, 0x440, bi2.data(), bi2.size()))
    return Fail("cannot read bi2.bin", 2);

  if (!write_file("sys/boot.bin", header.data(), header.size()) ||
      !write_file("sys/bi2.bin", bi2.data(), bi2.size()) ||
      !write_file("sys/main.dol", dol.data(), dol.size()) ||
      !write_file("sys/fst.bin", fst.data(), fst.size()))
    return Fail("cannot write the sys/ files", 1);

  if (info.apploader_size != 0)
  {
    std::vector<std::uint8_t> apploader(info.apploader_size);
    if (ReadAt(iso, info.apploader_offset, apploader.data(), apploader.size()))
      write_file("sys/apploader.img", apploader.data(), apploader.size());
  }

  const std::uint64_t total_files = root.total_bytes;
  std::uint64_t written = 0;

  std::function<bool(const FstEntry&, const fs::path&)> extract =
      [&](const FstEntry& entry, const fs::path& relative) -> bool {
    if (entry.directory)
    {
      fs::create_directories(staging / relative, ec);
      if (ec)
        return false;
      for (const auto& child : entry.children)
      {
        if (!extract(child, relative / PathFromUtf8(SanitizeComponent(child.name))))
          return false;
      }
      return true;
    }

    const fs::path dest = staging / relative;
    fs::create_directories(dest.parent_path(), ec);
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;

    std::vector<std::uint8_t> buffer(4 << 20);
    std::uint32_t remaining = entry.size;
    std::uint64_t offset = entry.offset;
    while (remaining > 0)
    {
      const std::size_t chunk = std::min<std::size_t>(remaining, buffer.size());
      if (!ReadAt(iso, offset, buffer.data(), chunk))
        return false;
      out.write(reinterpret_cast<const char*>(buffer.data()),
                static_cast<std::streamsize>(chunk));
      if (!out)
        return false;
      offset += chunk;
      remaining -= static_cast<std::uint32_t>(chunk);
      written += chunk;
      Progress(0.160 + 0.820 *
                           (total_files == 0 ? 1.0
                                             : static_cast<double>(written) /
                                                   static_cast<double>(total_files)),
               "Extracting game files");
    }
    return true;
  };

  if (!extract(root, "files"))
  {
    fs::remove_all(staging, ec);
    return Fail("failed while extracting the file tree", 1);
  }

  if (!WriteMarker(staging, info))
  {
    fs::remove_all(staging, ec);
    return Fail("cannot write the extraction marker", 1);
  }

  Progress(0.980, "Publishing the extracted game");
  fs::remove_all(options.output, ec);
  fs::rename(staging, options.output, ec);
  if (ec)
  {
    fs::remove_all(staging, ec);
    return Fail("cannot publish the extracted game: " + ec.message(), 1);
  }

  Progress(1.0, "Extraction complete");
  std::cout << "DOR2PREPARE: disc_id=" << info.disc_id
            << " title=\"" << info.title << "\""
            << " dol_sha256=" << info.dol_sha256
            << " iso_crc32=" << std::hex << std::setw(8) << std::setfill('0')
            << info.iso_crc32 << std::dec << '\n';
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);
#endif

  int exit_code = 0;
  const auto options = ParseOptions(argc, argv, exit_code);
  if (!options)
    return exit_code;

  std::error_code ec;
  if (fs::is_directory(options->input))
  {
    if (!fs::exists(options->output, ec) || options->force)
      return PrepareFromFolder(*options);

    std::string id;
    std::string dol_sha;
    if (ReadMarker(options->output, id, dol_sha) && id == options->expect_id &&
        (options->no_dol_check || dol_sha == options->expect_dol))
    {
      Progress(1.0, "Using existing extraction");
      std::cout << "DOR2PREPARE: disc_id=" << id << " dol_sha256=" << dol_sha << '\n';
      return 0;
    }
    return PrepareFromFolder(*options);
  }

  if (!fs::is_regular_file(options->input, ec))
    return Fail("input does not exist: " + PathToDisplay(options->input), 1);

  // Reuse a valid extraction instead of copying a 1.4 GB tree again.
  if (!options->force && fs::is_directory(options->output, ec))
  {
    std::string id;
    std::string dol_sha;
    if (ReadMarker(options->output, id, dol_sha) && id == options->expect_id &&
        (options->no_dol_check || dol_sha == options->expect_dol))
    {
      Progress(1.0, "Using existing extraction");
      std::cout << "DOR2PREPARE: disc_id=" << id << " dol_sha256=" << dol_sha << '\n';
      return 0;
    }
  }

  return PrepareFromIso(*options);
}
