// Offline controller-layout matrix.
//
// Drives the REAL port code (frontend::DetectControllerMapping +
// frontend::GenerateControllerConfig) with the REAL SDL gamepad DB rows that the
// shipped runner resolves each pad against (SDL 3.4.4 SDL_gamepad_db.h), and
// asserts the resulting GameCube layout per family.
//
// This is the regression gate for the v0.1.5 controller bug class: a layout rule
// that misfires on a family we do not own hardware for (Xbox, PS3, generic) must
// fail here, not in a player's hands.
//
// Build + run: scripts\test-controller-matrix.ps1
//   controller_matrix.exe [path\to\SDL_gamepad_db.h]
//
// Exit code: 0 = every case matched, 1 = mismatch (printed as FAIL).

#include "frontend_config.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using moderngekko::frontend::ControllerLayout;
using moderngekko::frontend::ControllerMapping;
using moderngekko::frontend::DetectControllerMapping;

namespace {

struct DbRow {
  std::string name;
  std::string mapping; // "name,binds" tail SDL hands back, without the GUID
};

// Parse "<guid>,<name>,<binds>," rows out of SDL_gamepad_db.h.
std::map<std::string, DbRow> LoadDb(const fs::path &path) {
  std::map<std::string, DbRow> rows;
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t first_quote = line.find('"');
    if (first_quote == std::string::npos)
      continue;
    const std::size_t last_quote = line.rfind('"');
    if (last_quote == std::string::npos || last_quote <= first_quote)
      continue;
    const std::string body = line.substr(first_quote + 1, last_quote - first_quote - 1);
    const std::size_t first_comma = body.find(',');
    if (first_comma == std::string::npos || first_comma != 32)
      continue;
    const std::size_t second_comma = body.find(',', first_comma + 1);
    if (second_comma == std::string::npos)
      continue;
    const std::string guid = body.substr(0, first_comma);
    DbRow row;
    row.name = body.substr(first_comma + 1, second_comma - first_comma - 1);
    row.mapping = body.substr(first_comma + 1); // "name,binds"
    rows[guid] = std::move(row);
  }
  return rows;
}

struct Case {
  std::string label;
  std::string guid;    // 32 hex chars, or empty for the keyboard path
  std::string device;  // explicit device string; empty = "SDL/0/<db name>"
  ControllerLayout layout;
  bool swap_dpad;
};

int failures = 0;

std::string StripQuoted(std::string_view text) {
  std::string out;
  bool in_quote = false;
  for (const char c : text) {
    if (c == '"') {
      in_quote = !in_quote;
      continue;
    }
    if (in_quote)
      out.push_back(c);
  }
  return out;
}

std::vector<std::string> FindGuids(std::string_view text) {
  std::vector<std::string> guids;
  for (std::size_t i = 0; i + 32 <= text.size(); ++i) {
    bool hex = true;
    for (std::size_t k = 0; k < 32; ++k) {
      const char c = text[i + k];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
        hex = false;
        break;
      }
    }
    if (!hex)
      continue;
    if (i > 0 && (std::isalnum(static_cast<unsigned char>(text[i - 1])) != 0))
      continue;
    if (i + 32 < text.size() &&
        std::isalnum(static_cast<unsigned char>(text[i + 32])) != 0)
      continue;
    guids.push_back(std::string(text.substr(i, 32)));
    i += 31;
  }
  return guids;
}

// The runner registers a few GUIDs at runtime with SDL_AddGamepadMapping because
// the bundled DB has no row for them (DualSense Edge USB). Read them back out of
// the runner source so this matrix literally tests what ships.
std::map<std::string, DbRow> LoadRuntimeRows(const fs::path &runner_source) {
  std::map<std::string, DbRow> rows;
  std::ifstream input(runner_source);
  if (!input)
    return rows;
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  const std::size_t layout = text.find("kEdgeUsbLayout");
  if (layout == std::string::npos)
    return rows;
  const std::size_t start = text.find('=', layout);
  const std::size_t end = text.find(';', start);
  if (start == std::string::npos || end == std::string::npos)
    return rows;
  const std::string binds = StripQuoted(
      std::string_view(text).substr(start, end - start));
  const std::string window = text.substr(layout, 1400);
  for (const std::string &guid : FindGuids(window)) {
    DbRow row;
    row.name = "DualSense Edge Controller (runtime)";
    row.mapping = row.name + "," + binds;
    rows[guid] = std::move(row);
  }
  return rows;
}

void Check(const std::map<std::string, DbRow> &db, const Case &test) {
  std::string device = test.device;
  std::string mapping;
  std::string guid = test.guid;
  if (!guid.empty()) {
    const auto found = db.find(guid);
    if (found == db.end()) {
      std::printf("FAIL  %-30s guid %s not in the SDL DB\n", test.label.c_str(),
                  guid.c_str());
      ++failures;
      return;
    }
    if (device.empty())
      device = "SDL/0/" + found->second.name;
    // Exactly the string SDL_GetGamepadMappingForID() hands the runner.
    mapping = guid + "," + found->second.mapping;
  }
  const ControllerMapping got = DetectControllerMapping(device, mapping, guid);
  const bool ok = got.layout == test.layout && got.swap_dpad_lr == test.swap_dpad;
  std::printf("%s  %-30s %-32s a/b->%-8s dpad=%-7s\n",
              ok ? "PASS" : "FAIL", test.label.c_str(), guid.c_str(),
              moderngekko::frontend::ControllerLayoutName(got.layout),
              got.swap_dpad_lr ? "swapped" : "normal");
  if (!ok) {
    std::printf("      expected layout=%s dpad=%s\n",
                moderngekko::frontend::ControllerLayoutName(test.layout),
                test.swap_dpad ? "swapped" : "normal");
    ++failures;
  }
}

// The GameCube face-button bindings a generated profile lands on, so the matrix
// proves the whole pipeline (detection -> profile), not just the enum.
bool ProfileFaces(const std::string &dir, const std::string &device,
                  ControllerMapping mapping, std::string *a, std::string *b,
                  std::string *left) {
  std::error_code ec;
  fs::remove_all(dir, ec);
  std::string message;
  if (!moderngekko::frontend::GenerateControllerConfig(dir, device, mapping,
                                                       &message))
    return false;
  std::ifstream input(fs::path(dir) / "Config" / "GCPadNew.ini");
  std::string line;
  while (std::getline(input, line)) {
    const auto value = [&line](std::string_view key) {
      if (line.rfind(key, 0) != 0)
        return std::string{};
      return line.substr(key.size());
    };
    if (const std::string v = value("Buttons/A = "); !v.empty())
      *a = v;
    else if (const std::string v2 = value("Buttons/B = "); !v2.empty())
      *b = v2;
    else if (const std::string v3 = value("D-Pad/Left = "); !v3.empty())
      *left = v3;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  fs::path db_path = argc > 1
                         ? fs::path(argv[1])
                         : fs::path("lib/ModernGekko-Template/lib/ModernGekko/"
                                    "vendor/dolphin/Externals/SDL/SDL/src/"
                                    "joystick/SDL_gamepad_db.h");
  if (!fs::exists(db_path)) {
    std::printf("FAIL  SDL gamepad DB not found: %s\n", db_path.string().c_str());
    return 1;
  }
  auto db = LoadDb(db_path);
  const fs::path runner_source =
      argc > 2 ? fs::path(argv[2])
               : fs::path("lib/ModernGekko-Template/lib/ModernGekko/tools/"
                          "moderngekko_run.cpp");
  const auto runtime_rows = LoadRuntimeRows(runner_source);
  if (runtime_rows.empty()) {
    std::printf("FAIL  no runtime-registered GUIDs found in %s\n",
                runner_source.string().c_str());
    ++failures;
  }
  for (const auto &[guid, row] : runtime_rows)
    db[guid] = row;
  std::printf("SDL gamepad DB: %s (%zu GUIDs)\n", db_path.string().c_str(),
              db.size());
  std::printf("Runtime-registered: %zu GUID(s) from %s\n\n",
              runtime_rows.size(), runner_source.filename().string().c_str());

  // Ground truth is SDL's own normalization: the DB binds SDL "a" to the
  // physical BOTTOM button and "b" to the RIGHT button for every positional
  // (Xbox/PlayStation/8BitDo-XInput) pad, so the GameCube profile must bind
  // GC A = Button A and GC B = Button B. Nintendo-labelled pads print "A" on
  // the right (SDL "B") and need the swap. Only first-party Nintendo and the
  // 8BitDo Nintendo-mode reports (vendor 0x2DC8, raw a index > b index) swap.
  const std::vector<Case> cases = {
      // --- Xbox: the family players reported dead in v0.1.5 ---------------
      {"Xbox 360 (USB)", "030000005e0400008e02000010010000", "",
       ControllerLayout::Standard, false},
      {"Xbox 360 (wireless)", "050000005e0400008e02000000783f00", "",
       ControllerLayout::Standard, false},
      {"Xbox 360 (Steam virtual)", "030000005e0400008e02000001000000", "",
       ControllerLayout::Standard, false},
      {"Xbox One (USB)", "030000005e040000ea02000000000000", "",
       ControllerLayout::Standard, false},
      {"Xbox One S (BT)", "050000005e040000ea02000000783f00", "",
       ControllerLayout::Standard, false},
      {"Xbox One (BT HIDAPI)", "050000005e040000fd020000ffff3f00", "",
       ControllerLayout::Standard, false},
      {"Xbox Series X (BT)", "050000005e040000120b000000783f80", "",
       ControllerLayout::Standard, false},
      {"Xbox Series X (BT, 0B13)", "050000005e040000130b0000ffff3f00", "",
       ControllerLayout::Standard, false},
      {"Xbox Elite S1 (BT)", "050000005e040000050b0000df070001", "",
       ControllerLayout::Standard, false},
      {"Xbox Elite S1 (BT, guide)", "050000005e040000050b0000ff070001", "",
       ControllerLayout::Standard, false},
      {"Xbox Elite S2 (BT)", "050000005e040000000b000000783f80", "",
       ControllerLayout::Standard, false},
      {"Xbox Elite S2 (BT, 0B05)", "050000005e040000050b0000ffff3f00", "",
       ControllerLayout::Standard, false},
      {"Xbox One Wired (02E3)", "030000005e040000e302000000000000", "",
       ControllerLayout::Standard, false},

      // --- PlayStation ----------------------------------------------------
      {"DualSense (USB)", "030000004c050000e60c000000000000", "",
       ControllerLayout::Standard, false},
      {"DualSense (BT)", "050000004c050000e60c000000010000", "",
       ControllerLayout::Standard, false},
      {"DualSense Edge (USB)", "030000004c050000f20d000000000000", "",
       ControllerLayout::Standard, false},
      {"DualSense Edge (BT)", "050000004c050000f20d000000010000", "",
       ControllerLayout::Standard, false},
      {"DualShock 4 (USB)", "030000004c050000cc09000000000000", "",
       ControllerLayout::Standard, false},

      // --- Nintendo first party ------------------------------------------
      {"Switch Pro (USB)", "030000007e0500000920000000000000", "",
       ControllerLayout::Nintendo, false},
      {"Switch Pro (BT)", "050000007e05000009200000ff070000", "",
       ControllerLayout::Nintendo, false},
      {"Joy-Con L+R", "060000007e0500000620000000000000", "",
       ControllerLayout::Nintendo, false},

      // --- 8BitDo ---------------------------------------------------------
      // Nintendo-mode reports: raw a index > b index -> swap (the v0.1.5 fix).
      {"8BitDo Pro 2 (Nintendo)", "03000000c82d00000660000000000000", "",
       ControllerLayout::Nintendo, false},
      {"8BitDo SN30 Pro+ (Nintendo)", "03000000c82d00000260000000000000", "",
       ControllerLayout::Nintendo, false},
      {"8BitDo SF30 Pro (Nintendo)", "03000000c82d00000060000000000000", "",
       ControllerLayout::Nintendo, false},
      {"8BitDo Zero 2 (Nintendo)", "03000000c82d00001890000000000000", "",
       ControllerLayout::Nintendo, false},
      // Standard-mode reports must NOT be swapped (no blanket vendor rule).
      {"8BitDo Ultimate (std)", "03000000c82d00001330000000000000", "",
       ControllerLayout::Standard, false},
      {"8BitDo Ultimate 2C (std)", "03000000c82d00001b30000000000000", "",
       ControllerLayout::Standard, false},
      {"8BitDo 64 (std)", "03000000c82d00001930000000000000", "",
       ControllerLayout::Standard, false},
      {"8BitDo M30 (std)", "03000000c82d00000650000000000000", "",
       ControllerLayout::Standard, false},
      // Pro 2 in XInput mode enumerates as an Xbox 360 pad; must stay standard
      // even though the device name still says "8BitDo Pro 2".
      {"8BitDo Pro 2 (XInput mode)", "030000005e0400008e02000000000000",
       "SDL/0/8BitDo Pro 2", ControllerLayout::Standard, false},

      // --- Pads that must keep the v0.1.4 behaviour ----------------------
      // PS3/PS2 and generic SNES-clone rows have a>b in the SDL DB but are
      // positionally labelled; v0.1.5 swapped them, v0.1.6 must not.
      {"PS3 (Sony 054C:0268)", "030000004c0500006802000000000000", "",
       ControllerLayout::Standard, false},
      {"PS3 (clone, a:b2/b:b1)", "03000000250900000500000000000000", "",
       ControllerLayout::Standard, false},
      {"PS2 (a:b2/b:b1)", "03000000100800000300000000000000", "",
       ControllerLayout::Standard, false},
      {"HORI Real Arcade Pro", "030000000d0f0000aa00000072050000", "",
       ControllerLayout::Standard, false},

      // --- Reversed D-pad correction (8BitDo hat / button reports) --------
      {"8BitDo reversed hat d-pad", "", "SDL/0/8BitDo Pro 2",
       ControllerLayout::Standard, false},

      // --- Keyboard ------------------------------------------------------
      {"Keyboard", "", "DInput/0/Keyboard Mouse", ControllerLayout::Keyboard,
       false},
  };

  for (const Case &test : cases) {
    if (test.label == "8BitDo reversed hat d-pad") {
      // Synthetic report: dpleft/dpright swapped in the hat index.
      const ControllerMapping got = DetectControllerMapping(
          test.device,
          "00000000000000000000000000000000,8BitDo Pro 2,a:b1,b:b0,"
          "dpleft:h0.2,dpright:h0.8,",
          "03000000c82d00000660000000000000");
      const bool ok = got.layout == ControllerLayout::Nintendo && got.swap_dpad_lr;
      std::printf("%s  %-30s %-32s a/b->%-8s dpad=%-7s\n", ok ? "PASS" : "FAIL",
                  test.label.c_str(), "(synthetic)",
                  moderngekko::frontend::ControllerLayoutName(got.layout),
                  got.swap_dpad_lr ? "swapped" : "normal");
      if (!ok) {
        std::printf("      expected layout=nintendo dpad=swapped\n");
        ++failures;
      }
      continue;
    }
    Check(db, test);
  }

  // --- End to end: detection -> generated GameCube profile ---------------
  std::printf("\nGenerated profile bindings (detection -> GCPadNew.ini):\n");
  const auto faces = [&](std::string_view label, const std::string &guid,
                         ControllerLayout expect_layout) {
    const auto found = db.find(guid);
    if (found == db.end()) {
      std::printf("FAIL  %-30s not in DB\n", std::string(label).c_str());
      ++failures;
      return;
    }
    const std::string device = "SDL/0/" + found->second.name;
    const std::string mapping = guid + "," + found->second.mapping;
    const ControllerMapping got = DetectControllerMapping(device, mapping, guid);
    std::string a, b, left;
    const std::string dir =
        (fs::temp_directory_path() / "dor2-controller-matrix" /
         std::string(label))
            .string();
    if (!ProfileFaces(dir, device, got, &a, &b, &left)) {
      std::printf("FAIL  %-30s profile generation failed\n",
                  std::string(label).c_str());
      ++failures;
      return;
    }
    const bool expect_swap = expect_layout == ControllerLayout::Nintendo;
    const bool ok = (a == (expect_swap ? "`Button B`" : "`Button A`")) &&
                    (b == (expect_swap ? "`Button A`" : "`Button B`")) &&
                    (left == "`Pad W`") && got.layout == expect_layout;
    std::printf("%s  %-30s A=%s B=%s D-Pad/Left=%s\n", ok ? "PASS" : "FAIL",
                std::string(label).c_str(), a.c_str(), b.c_str(), left.c_str());
    if (!ok)
      ++failures;
    std::error_code ec;
    fs::remove_all(dir, ec);
  };
  faces("Xbox One", "030000005e040000ea02000000000000", ControllerLayout::Standard);
  faces("Xbox Elite S2", "050000005e040000000b000000783f80",
        ControllerLayout::Standard);
  faces("DualSense Edge (USB)", "030000004c050000f20d000000000000",
        ControllerLayout::Standard);
  faces("8BitDo Pro 2 (Nintendo)", "03000000c82d00000660000000000000",
        ControllerLayout::Nintendo);
  faces("8BitDo Ultimate", "03000000c82d00001330000000000000",
        ControllerLayout::Standard);
  faces("PS3", "030000004c0500006802000000000000", ControllerLayout::Standard);
  faces("Switch Pro", "030000007e0500000920000000000000",
        ControllerLayout::Nintendo);

  // --- Audit: how many DB rows does the mapping heuristic reclassify? ----
  std::size_t nintendo_rows = 0;
  std::size_t total = 0;
  std::map<std::string, int> swapped_names;
  for (const auto &[guid, row] : db) {
    ++total;
    const std::string device = "SDL/0/" + row.name;
    const ControllerMapping got =
        DetectControllerMapping(device, guid + "," + row.mapping, guid);
    if (got.layout == ControllerLayout::Nintendo) {
      ++nintendo_rows;
      ++swapped_names[row.name];
    }
  }
  std::printf("\nAudit: %zu/%zu SDL DB rows resolve to a Nintendo (A/B swap) "
              "layout\n", nintendo_rows, total);
  for (const auto &[name, count] : swapped_names)
    std::printf("       %3d  %s\n", count, name.c_str());
  std::printf("       (expected: Nintendo first party + 8BitDo Nintendo-mode "
              "only)\n");

  std::printf("\nRESULT: %s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL",
              failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}