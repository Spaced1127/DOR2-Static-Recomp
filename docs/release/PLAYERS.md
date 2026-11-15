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

To override the automatic choice, open `DOR2Recomp.exe`, choose a controller,
and save. The profile is written to the runtime's user folder under
`Config\GCPadNew.ini`.

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
