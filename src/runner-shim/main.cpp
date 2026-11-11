// DOR2RecompRunner - the launcher/RecompLauncher-facing runner.
//
// The upstream ModernGekko runner selects its CPU core from
// MODERNGEKKO_STATICRECOMP at startup and has no --core switch. This shim adds
// the documented `--core static|jit` interface, then starts the real runtime
// (DOR2RecompRunnerCore.exe) with the remaining arguments unchanged.
//
// It contains no game data and does not touch the game module.

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{

std::wstring Widen(const std::string& text)
{
#if defined(_WIN32)
  if (text.empty())
    return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      wide.data(), length);
  return wide;
#else
  return std::wstring(text.begin(), text.end());
#endif
}

// Quote one argument using the CommandLineToArgvW / MSVCRT rules.
std::wstring QuoteArgument(const std::wstring& argument)
{
  if (!argument.empty() &&
      argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    return argument;

  std::wstring out = L"\"";
  for (auto it = argument.begin();; ++it)
  {
    std::size_t backslashes = 0;
    while (it != argument.end() && *it == L'\\')
    {
      ++it;
      ++backslashes;
    }

    if (it == argument.end())
    {
      out.append(backslashes * 2, L'\\');
      break;
    }
    if (*it == L'"')
    {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(L'"');
    }
    else
    {
      out.append(backslashes, L'\\');
      out.push_back(*it);
    }
  }
  out.push_back(L'"');
  return out;
}

// Tee the runtime's stderr into <user-dir>\Logs\ModernGekko.log so players and
// the build gate have a persistent log (the runtime itself only writes to the
// console). stdout passes through untouched.
struct StderrTee
{
  HANDLE log_file = INVALID_HANDLE_VALUE;
  HANDLE pipe_read = nullptr;
  HANDLE pipe_write = nullptr;
  HANDLE console_err = nullptr;
  std::thread pump;

  bool Start(const std::wstring& user_directory)
  {
    if (user_directory.empty())
      return false;
    const std::wstring logs = user_directory + L"\\Logs";
    std::error_code ec;
    std::filesystem::create_directories(logs, ec);
    log_file = CreateFileW((logs + L"\\ModernGekko.log").c_str(),
                           FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log_file == INVALID_HANDLE_VALUE)
      return false;
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    if (!CreatePipe(&pipe_read, &pipe_write, &inherit, 0))
    {
      CloseHandle(log_file);
      log_file = INVALID_HANDLE_VALUE;
      return false;
    }
    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);
    console_err = GetStdHandle(STD_ERROR_HANDLE);
    pump = std::thread([this] {
      char buffer[4096];
      DWORD got = 0;
      while (ReadFile(pipe_read, buffer, sizeof(buffer), &got, nullptr) && got)
      {
        DWORD written = 0;
        WriteFile(log_file, buffer, got, &written, nullptr);
        if (console_err && console_err != INVALID_HANDLE_VALUE)
          WriteFile(console_err, buffer, got, &written, nullptr);
      }
    });
    return true;
  }

  void Finish()
  {
    if (pipe_write)
    {
      CloseHandle(pipe_write);
      pipe_write = nullptr;
    }
    if (pump.joinable())
      pump.join();
    if (pipe_read)
    {
      CloseHandle(pipe_read);
      pipe_read = nullptr;
    }
    if (log_file != INVALID_HANDLE_VALUE)
    {
      CloseHandle(log_file);
      log_file = INVALID_HANDLE_VALUE;
    }
  }
};

std::wstring UserDirectoryArgument(const std::vector<std::string>& arguments)
{
  for (std::size_t i = 0; i < arguments.size(); ++i)
  {
    if (arguments[i] == "--user-dir" && i + 1 < arguments.size())
      return Widen(arguments[i + 1]);
    if (arguments[i].rfind("--user-dir=", 0) == 0)
      return Widen(arguments[i].substr(11));
  }
  return {};
}

std::wstring ExecutableDirectory()
{
#if defined(_WIN32)
  std::wstring buffer(32768, L'\0');
  const DWORD length =
      GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  buffer.resize(length);
  const std::size_t slash = buffer.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring() : buffer.substr(0, slash);
#else
  return L".";
#endif
}

void PrintUsage()
{
  std::printf(
      "DOR2RecompRunner - WWE Day of Reckoning 2 Recomp\n\n"
      "Usage: DOR2RecompRunner [--core static|jit] [runtime options]\n\n"
      "  --core jit      Dolphin JIT (default)\n"
      "  --core static   generated native module (experimental)\n\n"
      "All other arguments are passed to DOR2RecompRunnerCore.exe:\n"
      "  --game <root> --module <dll> --user-dir <dir> --load-state <file>\n"
      "  --graphics Vulkan|OpenGL --audio Cubeb|Null\n"
      "  --automation-dir <dir> --allow-interpreter --headless\n");
}

}  // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);
#endif

  std::vector<std::string> forwarded;
  forwarded.reserve(static_cast<std::size_t>(argc));
  forwarded.emplace_back(argv[0]);

  std::string core;
  for (int i = 1; i < argc; ++i)
  {
    const std::string argument = argv[i];
    if (argument == "--core")
    {
      if (i + 1 >= argc)
      {
        std::fprintf(stderr, "error: --core needs static or jit\n");
        return 2;
      }
      core = argv[++i];
    }
    else if (argument.rfind("--core=", 0) == 0)
    {
      core = argument.substr(7);
    }
    else if (argument == "--help" || argument == "-h" || argument == "--version")
    {
      PrintUsage();
      return 0;
    }
    else
    {
      forwarded.push_back(argument);
    }
  }

  if (!core.empty() && core != "static" && core != "jit")
  {
    std::fprintf(stderr, "error: --core must be static or jit\n");
    return 2;
  }

#if defined(_WIN32)
  if (!core.empty())
  {
    _putenv_s("MODERNGEKKO_STATICRECOMP", core == "static" ? "1" : "0");
  }
  if (core == "jit")
  {
    // The JIT core does not execute the static module, so the runtime needs
    // explicit permission for its interpreter fallback path.
    const auto has_flag = std::find(forwarded.begin(), forwarded.end(),
                                    std::string("--allow-interpreter"));
    if (has_flag == forwarded.end())
      forwarded.emplace_back("--allow-interpreter");
  }

  const std::wstring directory = ExecutableDirectory();
  const std::wstring runner = directory + L"\\DOR2RecompRunnerCore.exe";
  if (GetFileAttributesW(runner.c_str()) == INVALID_FILE_ATTRIBUTES)
  {
    std::fprintf(stderr,
                 "error: DOR2RecompRunnerCore.exe is missing next to this "
                 "executable\n");
    return 2;
  }

  std::wstring command_line = QuoteArgument(runner);
  for (std::size_t i = 1; i < forwarded.size(); ++i)
  {
    command_line.push_back(L' ');
    command_line += QuoteArgument(Widen(forwarded[i]));
  }

  StderrTee tee;
  const bool teeing = tee.Start(UserDirectoryArgument(forwarded));

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  if (teeing)
  {
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = tee.pipe_write;
  }
  PROCESS_INFORMATION process{};
  std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back(L'\0');

  if (!CreateProcessW(runner.c_str(), mutable_command.data(), nullptr, nullptr, TRUE,
                      0, nullptr, directory.c_str(), &startup, &process))
  {
    tee.Finish();
    std::fprintf(stderr, "error: cannot start the runtime (error %lu)\n",
                 static_cast<unsigned long>(GetLastError()));
    return 2;
  }

  WaitForSingleObject(process.hProcess, INFINITE);
  tee.Finish();
  DWORD exit_code = 1;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return static_cast<int>(exit_code);
#else
  (void)core;
  std::fprintf(stderr, "This shim targets Windows builds.\n");
  return 2;
#endif
}
