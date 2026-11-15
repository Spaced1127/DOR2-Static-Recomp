# Players guide

## Disc images

The launcher imports a **.iso or a Dolphin .rvz** directly (the file picker
lists both, plus .wbfs). RVZ is decoded natively — no manual conversion to ISO
is needed. The extract verifies the disc ID (`GW2E78`) and the retail DOL hash
before anything runs, and an RVZ produces exactly the same files as the ISO.

## Default controller layout

Controller support is built into the game itself: on every boot the runner
detects the connected pad(s) — **Xbox, PlayStation (DualSense/DualShock),
Nintendo Switch Pro, or any generic SDL gamepad** — and writes a GameCube pad
profile automatically. This works even when you launch through RecompLauncher
(which has no controller picker). If you swap controllers, the profile is
regenerated for the new pad on the next boot; a profile for a pad that is still
connected is left untouched. Up to four pads map to ports 1-4.

The mapping is position-consistent across pads (A = bottom, B = right, X = left,
Y = top):

| GameCube | Xbox | PlayStation | Switch Pro | Keyboard |
| --- | --- | --- | --- | --- |
| A | A | Cross | B | X |
| B | B | Circle | A | Z |
| X | X | Square | Y | C |
| Y | Y | Triangle | X | V |
| Start | Menu | Options | + | Enter |
| D-pad | D-pad | D-pad | D-pad | Arrow keys |
| Control stick | Left stick | Left stick | Left stick | WASD |
| C-stick | Right stick | Right stick | Right stick | IJKL |
| L | LB | L1 | L | Q |
| R | RB | R1 | R | E |
| Z | LT | L2 | ZL | Shift |

(The Switch column uses Nintendo's button labels; the physical positions match
Xbox/PlayStation.)

### Pro controllers (DualSense Edge, Xbox Elite Series 1 & 2)

These work with the **same layout as a standard DualSense / Xbox pad**. Detection
is by USB GUID (SDL's gamecontroller database), not the device name:

| Device | VID:PID | GUID prefix (USB / Bluetooth) |
| --- | --- | --- |
| DualSense Edge | 054C:0DF2 | `030000004c050000f20d…` / `050000004c050000f20d…` |
| Xbox Elite Series 1 | 045E:02E3 | `030000005e040000e302…` |
| Xbox Elite Series 2 | 045E:0B00 / 0B05 | `050000005e040000000b…` / `…050b…` |

The Edge's back paddles / Fn buttons and the Elite's paddles are extra buttons
with no GameCube equivalent, so they are **explicitly ignored** (not bound to any
control). Face buttons, sticks, triggers, shoulders and the D-pad behave exactly
like the standard pad.

### 8BitDo pads (Nintendo layout fix)

8BitDo pads in **Nintendo layout** (Pro 2 / SN30 / SF30 / N30, and Switch-mode)
report their face buttons swapped relative to Xbox. The runner reads the **live
SDL binding per GUID** and, when `a`/`b` are Nintendo-style, generates a Nintendo
profile (A/B and X/Y swapped) so the buttons match the printed labels. This is
**not** a blanket 8BitDo rule: the **Ultimate** and **64** families report a
standard layout and stay standard. A firmware that reports a reversed D-pad
left/right is corrected automatically.

### Checking your pad

Run `DOR2RecompRunner.exe --list-controllers` (no game starts). It prints one
diagnostic line per pad, also written to `Logs\ModernGekko.log` at game startup:

```
controller: name="<device>" guid=<32-hex> vid=<vid> pid=<pid> layout=<auto|standard|nintendo|keyboard> dpad=<normal|swapped>
controller:   sdl_mapping=<SDL binding string>
```

If a pad is not detected or maps incorrectly, send that line (name + GUID +
`sdl_mapping`) so the exact GUID variant can be handled.

### Manual layout override

To force a layout instead of auto-detect:

- **Launcher:** CONTROLLER page -> "Button layout" -> Auto-detect / Standard
  (Xbox / PS) / Nintendo (A/B swap).
- **Config:** set `controller_layout=nintendo` (or `standard`/`auto`) in
  `%LOCALAPPDATA%\DOR2Recomp\config.ini`.
- **CLI:** `DOR2RecompRunner.exe --controller-layout nintendo` (aliases:
  `switch`->nintendo, `xbox`/`playstation`->standard).

A forced layout regenerates the profile; a hand-edited (custom)
`Config\GCPadNew.ini` is preserved unless the device changes.

## Menus

The game's menus read the **main stick** and the A/B buttons. If a menu does
not respond to the D-pad, use the left stick.

## Saving

The port uses a folder memory card, not a card image:

```
<user folder>\GC\US\Card A\*.gci
```

You can copy `.gci` files from Dolphin into that folder. Savestates are written
with F1 under `<user folder>\StateSaves\`.

## Performance

The GameCube VI runs at 59.94 Hz. A title-bar reading of 59.9 is full speed.
The default core is **JIT**, verified clean and full-speed on all tested
content. If the port dips, try:

1. 2x resolution instead of 3x.
2. MSAA off.
3. Close other GPU-heavy applications.

The **static recomp** core (the generated native module) is available as an
experimental opt-in (`CPU core` setting). It is full-speed on the reference
scene but can, rarely, become unstable in some heavy modes (entrances /
fatal-4-way): "Invalid read/write" alerts and a frame-rate drop. If that
happens, switch back to JIT. See the repository README "Known issues".

## Reporting a problem

Include:

- the last 60 lines of `Logs\ModernGekko.log`
- your GPU and driver version
- whether the static or JIT core was selected
- the scene and the on-screen frame rate

Never upload the game's files or a disc image with a report.
