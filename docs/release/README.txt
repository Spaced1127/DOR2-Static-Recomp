WWE Day of Reckoning 2
Standalone PC port (JIT default, static recompilation included) - Windows x64
===============================================================

This package contains NO game data. There is no disc image, no DOL, no file
table, no textures, no audio and no saves in these files. The recompiled module
is program code only; the art, sound and levels are read at runtime from the
game files you supply.


WHAT YOU NEED
-------------
An original GameCube copy of "WWE Day of Reckoning 2" (USA, disc ID GW2E78),
as a .iso image (1,459,978,240 bytes, clean 1:1 Redump dump, CRC-32 bb092c3a).

The disc is imported by this app. Other regions and other games are rejected.


QUICK START
-----------
1. Put this folder anywhere you have write access (for example
   C:\Games\DOR2Recomp). Do not put it in Program Files: the extracted game is
   written into a "Game" folder next to the executable.

2. Run DOR2Recomp.exe. This is the launcher; it is the only file you need to
   open. There is no .cmd or batch file to run.

3. Click "Browse for ISO / WBFS / RVZ", pick your disc image, then click
   "Extract and Play". The disc is extracted once into a "Game" folder beside
   the executable and remembered in default-game.txt.

   If you already extracted the disc elsewhere, click "Browse for extracted
   game folder" and pick the folder that contains "sys" and "files".

4. Change any settings you like (graphics, audio, controller, CPU core), then
   click Play. The game window opens. That is the whole flow:
   launcher -> Play -> game. Esc opens/closes the in-game menu.

The extracted folder needs about 1.4 GB. Re-importing the same disc reuses the
existing extraction.


SETTINGS
--------
All settings are stored in config.ini in %LOCALAPPDATA%\DOR2Recomp\ and persist
between sessions. The launcher reads and writes them; you do not need to edit
the file by hand.

The launcher has pages for PLAY, GRAPHICS, PERFORMANCE, AUDIO and CONTROLLER.
Everything is saved automatically.

  Graphics backend        Vulkan (recommended) or OpenGL
  Internal resolution     640x528 (1x) up to 7680x4320 (12x)
  Anti-aliasing (MSAA)    Off / 2x / 4x / 8x, plus SSAA
  Anisotropic filtering   1x / 2x / 4x / 8x / 16x
  VSync                   on / off
  Aspect ratio            Auto (game native) / 16:9 / 4:3 / Stretch
  Widescreen hack         stretches 3D to the chosen aspect
  Audio                   Mute all audio, master volume 0-100%
  2D compatibility        EFB access, ON by default
  Show FPS in title       on / off
  Fullscreen, controller profile, netplay host/join


FULLSCREEN / SHUTDOWN
---------------------
Fullscreen is borderless. The window is created windowed and switches to the
monitor after the graphics backend is ready, so Alt+F4 closes cleanly. The
runner waits up to 3 seconds for the game's power-button handler and then exits
even if the game does not handle it.


FRAME RATE
----------
The game runs at the GameCube's 59.94 Hz VI timing. There is no uncapped mode;
the game cannot run faster than the console. 59.9 fps is full speed.


CPU CORE
--------
PERFORMANCE page -> "Game core".

  * JIT (default) uses Dolphin's JIT core. It holds full speed in all tested
    scenes, including entrances and fatal-4-way matches, with zero faults.
  * Static recomp runs gGW2E78_recomp.dll, the static-recompilation build. It
    is the faithful static artifact and is selectable at any time, but it is
    EXPERIMENTAL: rarely, in some heavy modes (entrances / fatal-4-way), it
    corrupts memory and shows "Invalid read/write" alerts with a frame-rate
    drop. If that happens, switch back to JIT. See docs/STATIC_CORRUPTION.md
    in the repository.

Both modes read the same extracted game files. The static module ships in this
package. The launcher passes your choice to the runner as --core jit|static;
the runner is a single executable (DOR2RecompRunner.exe) that selects the core
itself. There is no separate "core" executable and no shim.


AUDIO
-----
AUDIO page -> "Mute all audio" silences the game, movies and menu sounds; the
master volume slider sets the mix level. Mute is applied at the audio backend,
so it also silences the intro videos. Audio uses DSP LLE, which requires the
GameCube DSP firmware shipped in Sys\GC\ (dsp_rom.bin, dsp_coef.bin).


2D COMPATIBILITY (EFB ACCESS)
-----------------------------
Leave this ON. The legal notice, the title screen and the menus are drawn
through the GameCube's embedded framebuffer and render black without it.
Accurate reads have no measurable cost in 3D scenes, so there is no reason to
turn it off.


CONTROLS (default)
------------------
  Left stick / D-pad      move
  A                       confirm / grapple
  B                       cancel / strike
  X, Y                    game actions
  Start                   pause
  Esc                     open/close emulator menu
  F11                     toggle fullscreen

Controllers are auto-detected (Xbox, DualSense, Switch Pro and most SDL
gamepads). Pads are laid out in the launcher. On a fresh install that never
opens the launcher, the runner generates a default controller profile for the
first detected gamepad automatically.


FILES
-----
  DOR2Recomp.exe           launcher, importer and settings (open this)
  DOR2RecompRunner.exe     game process (started by the launcher's Play)
  gGW2E78_recomp.dll       optional static-recomp core (no assets)
  dor2_grid.png            launcher background art (SteamGridDB)
  dor2_logo.png            launcher header logo (SteamGridDB)
  Sys\                     graphics shaders, runtime tables, DSP firmware
  LICENSES\                third-party licenses
  config.ini               created on first run (in %LOCALAPPDATA%\DOR2Recomp)
  Game\                    created when you import your disc


TROUBLESHOOTING
---------------
"expected disc ID GW2E78; selected ..."
    The image is a different game or a different region.

"game root is not a readable directory"
    The selected extracted folder is wrong. Pick the folder that directly
    contains "sys" and "files".

"main DOL does not match the pinned release"
    The extracted program file is not the retail build this port was compiled
    for. Re-extract from a clean, unmodified disc image.

The launcher window is black.
    Update your graphics driver. The game window needs Vulkan or OpenGL 4.4.

Low frame rate
    Lower the internal resolution, turn off MSAA/SSAA, and check the GPU
    driver. The default settings target full speed at 1080p. Make sure you are
    on the JIT core if the static core is misbehaving.

If a run is force-killed and the next boot hangs, delete
%LOCALAPPDATA%\DOR2Recomp\Cache\Shaders and retry.

Save states and controller profiles are per-user in %LOCALAPPDATA%\DOR2Recomp.


LICENSE
-------
Project code GPL-3.0; DolRecomp GPL-3.0; ModernGekko/Dolphin GPL-2.0-or-later
(aggregate GPL-3.0). See LICENSES\. WWE and all related marks are trademarks of
their respective owners. This is an unofficial fan project with no affiliation.
Launcher artwork is from SteamGridDB (game id 17046); see docs/ASSETS.md in the
repository for sources and attribution.
