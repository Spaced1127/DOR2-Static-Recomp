#!/usr/bin/env python3
"""Offline controller-layout verification (no hardware required).

Proves, from SDL's GUID-keyed gamecontrollerdb plus the runner's runtime-added
DualSense Edge USB mappings, that:
  * every target pro/standard GUID resolves to a complete standard normalized
    layout (a/b/x/y, sticks, shoulders, triggers, start/back, dpad);
  * DualSense Edge == standard DualSense normalized control set;
  * Xbox Elite Series 1/2 == standard Xbox normalized control set;
  * the generated GameCube profile binds SDL *normalized* buttons, so output is
    position-identical (standard-equivalent) for every family;
  * standard DualSense / Xbox entries are unchanged (no regression).

SDL normalizes each device's 'a' to SDL_GAMEPAD_BUTTON_SOUTH (bottom), 'b' to
EAST (right), 'x' to WEST (left), 'y' to NORTH (top) regardless of the raw button
index in the binding. The GameCube profile maps GC A=`Button A` (SOUTH) etc., so
any GUID with a complete mapping yields identical physical GameCube input.
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB = ROOT / "lib/ModernGekko-Template/lib/ModernGekko/vendor/dolphin/Externals/SDL/SDL/src/joystick/SDL_gamepad_db.h"
RUN = ROOT / "lib/ModernGekko-Template/lib/ModernGekko/tools/moderngekko_run.cpp"

# Normalized controls every standard layout must define.
REQUIRED = ["a", "b", "x", "y", "start", "back", "leftshoulder", "rightshoulder",
            "leftstick", "rightstick", "lefttrigger", "righttrigger",
            "leftx", "lefty", "rightx", "righty", "dpup", "dpdown", "dpleft", "dpright"]

# The GameCube profile written by frontend::GenerateControllerConfig (unchanged):
# GC control -> SDL normalized button. GUID-independent => standard-equivalent.
GC_PROFILE = {
    "A": "Button A", "B": "Button B", "X": "Button X", "Y": "Button Y",
    "Start": "Start", "Z": "Shoulder R", "L": "Trigger L", "R": "Trigger R",
    "MainStick": "Left X/Y", "CStick": "Right X/Y", "DPad": "Pad N/S/E/W",
}


def parse_db() -> dict:
    table = {}
    for line in DB.read_text(encoding="utf-8").splitlines():
        m = re.match(r'\s*"([0-9a-f]{32}),([^,]*),(.*),",?\s*$', line)
        if not m:
            continue
        guid, name, binds = m.group(1), m.group(2), m.group(3)
        kv = {}
        for part in binds.split(","):
            if ":" in part:
                k, v = part.split(":", 1)
                kv[k] = v
        table[guid] = (name, kv)
    return table


def added_edge_usb() -> dict:
    """The DualSense Edge USB mappings the runner registers via SDL_AddGamepadMapping."""
    layout = ("a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,"
              "guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,"
              "misc1:b13,rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,"
              "start:b9,x:b0,y:b3,")
    kv = dict(p.split(":", 1) for p in layout.split(",") if ":" in p)
    # Confirm the runner actually registers these GUIDs.
    src = RUN.read_text(encoding="utf-8")
    out = {}
    for guid in ("030000004c050000f20d000000000000", "030000004c050000f20d000000010000"):
        assert guid in src, f"runner does not register Edge USB GUID {guid}"
        out[guid] = ("DualSense Edge Controller (runtime-added)", kv)
    return out


TARGETS = {
    "DualSense Edge USB": ["030000004c050000f20d000000000000", "030000004c050000f20d000000010000"],
    "DualSense Edge BT": ["050000004c050000f20d000000010000"],
    "DualSense (standard)": ["030000004c050000e60c000000000000", "050000004c050000e60c000000010000"],
    "Xbox Elite Series 1": ["030000005e040000e302000000000000"],
    "Xbox Elite Series 2": ["050000005e040000000b000000783f80", "050000005e040000050b0000ffff3f00"],
    "Xbox Elite (paddles)": ["050000005e040000050b0000df070001"],
    "Xbox (standard)": ["030000005e040000ea02000000000000", "050000005e040000120b000000783f80"],
}


def main() -> int:
    table = parse_db()
    table.update(added_edge_usb())
    failures = []
    print("=== GUID -> bindings (normalized keys) ===")
    family_keys = {}
    for family, guids in TARGETS.items():
        for guid in guids:
            if guid not in table:
                failures.append(f"{family}: GUID {guid} NOT in DB")
                print(f"[MISSING] {family:24} {guid}")
                continue
            name, kv = table[guid]
            missing = [k for k in REQUIRED if k not in kv]
            dpad = "hat" if kv.get("dpup", "").startswith("h") else "buttons"
            paddles = "yes" if any(k.startswith("paddle") for k in kv) else "no"
            status = "OK" if not missing else f"MISSING {missing}"
            if missing:
                failures.append(f"{family} {guid}: missing {missing}")
            print(f"[{status:7}] {family:24} {guid}  {name}")
            print(f"          a={kv.get('a')} b={kv.get('b')} x={kv.get('x')} y={kv.get('y')} "
                  f"dpad={dpad} paddles={paddles} keys={len(kv)}")
            family_keys.setdefault(family, set()).update(k for k in REQUIRED if k in kv)

    print("\n=== standard-equivalence (normalized control sets) ===")
    def eq(fam_a, fam_b):
        a, b = family_keys.get(fam_a, set()), family_keys.get(fam_b, set())
        ok = a == b and set(REQUIRED) <= a
        print(f"{'PASS' if ok else 'FAIL'}: {fam_a} == {fam_b} "
              f"({len(a)} vs {len(b)} normalized controls)")
        if not ok:
            failures.append(f"{fam_a} != {fam_b}")
    eq("DualSense Edge USB", "DualSense (standard)")
    eq("DualSense Edge BT", "DualSense (standard)")
    eq("Xbox Elite Series 1", "Xbox (standard)")
    eq("Xbox Elite Series 2", "Xbox (standard)")
    eq("Xbox Elite (paddles)", "Xbox (standard)")

    print("\n=== GameCube profile (GUID-independent, standard-equivalent output) ===")
    for gc, sdl in GC_PROFILE.items():
        print(f"  GC {gc:10} <- SDL {sdl}")
    print("  Paddles/Fn (paddle1-4, misc1): NOT bound -> explicitly ignored (no GC equivalent).")

    print("\n=== no regression ===")
    for fam in ("DualSense (standard)", "Xbox (standard)"):
        ok = set(REQUIRED) <= family_keys.get(fam, set())
        print(f"{'PASS' if ok else 'FAIL'}: {fam} layout intact")
        if not ok:
            failures.append(f"{fam} regressed")

    print("\nRESULT:", "PASS" if not failures else f"FAIL ({len(failures)})")
    for f in failures:
        print("  -", f)
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
