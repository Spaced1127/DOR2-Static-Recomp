# Players guide

## Default controller layout

The runtime creates a GameCube pad profile for the first detected gamepad. The
standard mapping is:

| GameCube | Xbox | PlayStation | Keyboard |
| --- | --- | --- | --- |
| A | A | Cross | X |
| B | B | Circle | Z |
| X | X | Square | C |
| Y | Y | Triangle | V |
| Start | Menu | Options | Enter |
| D-pad | D-pad | D-pad | Arrow keys |
| Control stick | Left stick | Left stick | WASD |
| C-stick | Right stick | Right stick | IJKL |
| L | LB | L1 | Q |
| R | RB | R1 | E |
| Z | LT | L2 | Shift |

If your pad is not detected, open `DOR2Recomp.exe`, choose a controller, and
save. The profile is written to the runtime's user folder under
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
