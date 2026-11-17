// Canonical controller system: offline class + profile matrix.
//
// Drives the REAL port code (ciface::SDL::ResolveLayoutClass from
// LayoutClass.h, and frontend::EnsureCanonicalPadProfile / ParseControllerClass
// from frontend_config.cpp). No controller hardware and no window needed.
//
// What this proves:
//   * class resolution per pad: SDL gamepad type first, the 8BitDo Switch-mode
//     fallback second, the user override winning over both;
//   * the canonical profile is positional, byte-identical, and written once;
//   * a keyboard device still yields a keyboard profile;
//   * controller_class round-trips and the legacy controller_layout key is
//     accepted by ParseControllerClass.
//
// What it cannot prove (needs pads): the SDL type a physical pad reports.
//
// Build + run: scripts\test-controller-matrix.ps1
// Exit code: 0 = pass, 1 = mismatch.

#include "frontend_config.hpp"

#include "LayoutClass.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using ciface::SDL::LayoutClass;
using ciface::SDL::ResolveLayoutClass;
using moderngekko::frontend::ControllerClass;
using namespace moderngekko::frontend;

namespace {

int failures = 0;

const char *ClassName(LayoutClass layout_class) {
  return layout_class == LayoutClass::Nintendo ? "nintendo" : "positional";
}

// SDL_GamepadType values (SDL 3.4, SDL_gamepad.h).
enum SdlType {
  UNKNOWN = 0,
  STANDARD = 1,
  XBOX360 = 2,
  XBOXONE = 3,
  PS3 = 4,
  PS4 = 5,
  PS5 = 6,
  SWITCH_PRO = 7,
  JOYCON_LEFT = 8,
  JOYCON_RIGHT = 9,
  JOYCON_PAIR = 10,
  GAMECUBE = 11,
};

void ExpectClass(const char *label, int sdl_type, bool eightbitdo_clone,
                 int override_class, LayoutClass expected) {
  const LayoutClass got =
      ResolveLayoutClass(override_class, sdl_type, eightbitdo_clone);
  const bool ok = got == expected;
  std::printf("%s  %-34s sdl_type=%-2d clone=%d override=%2d -> %s\n",
              ok ? "PASS" : "FAIL", label, sdl_type, eightbitdo_clone ? 1 : 0,
              override_class, ClassName(got));
  if (!ok) {
    std::printf("      expected %s\n", ClassName(expected));
    ++failures;
  }
}

void ExpectTrue(const char *label, bool value) {
  std::printf("%s  %s\n", value ? "PASS" : "FAIL", label);
  if (!value)
    ++failures;
}

std::string ReadFile(const fs::path &path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool Contains(const std::string &text, const char *needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

int main() {
  // --- class resolution ---------------------------------------------------
  std::printf("== class resolution (SDL type, 8BitDo fallback, override) ==\n");
  // Every positional family: Xbox, PlayStation, generic/standard, GameCube.
  ExpectClass("Xbox 360", XBOX360, false, -1, LayoutClass::Positional);
  ExpectClass("Xbox One", XBOXONE, false, -1, LayoutClass::Positional);
  ExpectClass("DualShock 3", PS3, false, -1, LayoutClass::Positional);
  ExpectClass("DualShock 4", PS4, false, -1, LayoutClass::Positional);
  ExpectClass("DualSense / Edge", PS5, false, -1, LayoutClass::Positional);
  ExpectClass("generic / standard", STANDARD, false, -1, LayoutClass::Positional);
  ExpectClass("GameCube adapter", GAMECUBE, false, -1, LayoutClass::Positional);
  ExpectClass("unrecognised pad", UNKNOWN, false, -1, LayoutClass::Positional);
  // Nintendo first party.
  ExpectClass("Switch Pro", SWITCH_PRO, false, -1, LayoutClass::Nintendo);
  ExpectClass("Joy-Con L", JOYCON_LEFT, false, -1, LayoutClass::Nintendo);
  ExpectClass("Joy-Con R", JOYCON_RIGHT, false, -1, LayoutClass::Nintendo);
  ExpectClass("Joy-Con pair", JOYCON_PAIR, false, -1, LayoutClass::Nintendo);
  // 8BitDo Switch-mode clones that report an unknown type.
  ExpectClass("8BitDo Switch clone (unknown type)", UNKNOWN, true, -1,
              LayoutClass::Nintendo);
  ExpectClass("8BitDo XInput mode", XBOX360, false, -1, LayoutClass::Positional);
  // Override wins over everything.
  ExpectClass("override positional on Switch Pro", SWITCH_PRO, false, 0,
              LayoutClass::Positional);
  ExpectClass("override nintendo on Xbox", XBOX360, false, 1, LayoutClass::Nintendo);
  ExpectClass("override nintendo on DS4", PS4, false, 1, LayoutClass::Nintendo);

  // --- class + config text ------------------------------------------------
  std::printf("\n== controller_class parsing / aliases ==\n");
  ControllerClass parsed = ControllerClass::Auto;
  ExpectTrue("default is auto", std::string(ControllerClassName(ControllerClass::Auto)) == "auto");
  ExpectTrue("\"positional\" parses", ParseControllerClass("positional", &parsed) &&
                                         parsed == ControllerClass::Positional);
  ExpectTrue("legacy \"standard\" parses",
             ParseControllerClass("standard", &parsed) && parsed == ControllerClass::Positional);
  ExpectTrue("\"nintendo\" parses", ParseControllerClass("nintendo", &parsed) &&
                                        parsed == ControllerClass::Nintendo);
  ExpectTrue("legacy \"switch\" parses",
             ParseControllerClass("switch", &parsed) && parsed == ControllerClass::Nintendo);
  ExpectTrue("legacy \"keyboard\" is positional",
             ParseControllerClass("keyboard", &parsed) && parsed == ControllerClass::Positional);
  ExpectTrue("garbage rejected", !ParseControllerClass("gamepad", &parsed));

  // --- canonical profile --------------------------------------------------
  std::printf("\n== canonical profile (write once, positional) ==\n");
  const fs::path dir = fs::temp_directory_path() /
                       ("dor2-controller-matrix-" +
                        std::to_string(static_cast<unsigned long long>(
                            std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path profile = dir / "Config" / "GCPadNew.ini";
  std::string message;
  ExpectTrue("first run writes the profile",
             EnsureCanonicalPadProfile(dir, "SDL/0/Xbox Wireless Controller", &message));
  const std::string first = ReadFile(profile);
  ExpectTrue("Buttons/A = Button A", Contains(first, "Buttons/A = `Button A`"));
  ExpectTrue("Buttons/B = Button B", Contains(first, "Buttons/B = `Button B`"));
  ExpectTrue("Buttons/X = Button X", Contains(first, "Buttons/X = `Button X`"));
  ExpectTrue("Buttons/Y = Button Y", Contains(first, "Buttons/Y = `Button Y`"));
  ExpectTrue("D-Pad/Left = Pad W (never swapped)", Contains(first, "D-Pad/Left = `Pad W`"));
  ExpectTrue("D-Pad/Right = Pad E", Contains(first, "D-Pad/Right = `Pad E`"));
  ExpectTrue("device line recorded", Contains(first, "Device = SDL/0/Xbox Wireless Controller"));
  ExpectTrue("no controller profile reads back",
             ReadConfiguredController(dir) == "SDL/0/Xbox Wireless Controller");

  // A second call with a different pad must not touch the file.
  ExpectTrue("second run keeps the file",
             EnsureCanonicalPadProfile(dir, "SDL/0/DualSense Wireless Controller", &message));
  ExpectTrue("profile is byte-identical after a second run", ReadFile(profile) == first);
  ExpectTrue("message says kept", Contains(message, "kept"));

  // Keyboard device: still a static profile, keyboard bindings.
  const fs::path kbd_dir = dir / "kbd";
  ExpectTrue("keyboard profile generated",
             EnsureCanonicalPadProfile(kbd_dir, "DInput/0/Keyboard Mouse", &message));
  const std::string kbd = ReadFile(kbd_dir / "Config" / "GCPadNew.ini");
  ExpectTrue("keyboard A = RETURN", Contains(kbd, "Buttons/A = `RETURN`"));

  // --- config.ini ---------------------------------------------------------
  std::printf("\n== config.ini controller_class ==\n");
  const auto parsed_config = moderngekko::frontend::ParseConfig(
      "resolution=1920x1080\ncontroller_class=nintendo\n");
  ExpectTrue("controller_class=nintendo round-trips",
             parsed_config && parsed_config.controller_class == "nintendo");
  const auto legacy = moderngekko::frontend::ParseConfig(
      "resolution=1920x1080\ncontroller_layout=standard\n");
  ExpectTrue("legacy controller_layout still accepted",
             legacy && legacy.controller_class == "positional");
  const auto bad = moderngekko::frontend::ParseConfig(
      "resolution=1920x1080\ncontroller_class=gamepad\n");
  ExpectTrue("invalid controller_class rejected", !bad);

  std::error_code ec;
  fs::remove_all(dir, ec);

  std::printf("\nRESULT: %s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL",
              failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}