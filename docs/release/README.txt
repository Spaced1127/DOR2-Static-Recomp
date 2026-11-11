DOR2 Static Recomp v0.1.0
=========================

A native Windows port of WWE Day of Reckoning 2 (GameCube, USA, GW2E78).

This package contains NO game data. It does not include a disc image, DOL,
file table, textures, audio, saves or cover art. You must own and dump your own
copy of the game.

Requirements
------------
- Windows 10 or 11, 64-bit
- A Vulkan-capable GPU with current drivers
- Your own USA disc image of WWE Day of Reckoning 2 (ISO)
  Redump size: 1,459,978,240 bytes
  Redump CRC-32: bb092c3a

Quick start (RecompLauncher)
----------------------------
1. Install or open this release in RecompLauncher.
2. Choose your USA ISO when asked.
3. Wait for "Extracting the game files from your disc".
4. Press Play.

Quick start (manual)
--------------------
1. Extract your ISO into this folder:

   DOR2Prepare.exe --input "D:\dumps\WWE Day of Reckoning 2 (USA).iso" --output "Game"

2. Start the game:

   DOR2RecompRunner.exe --game "Game" --module "gGW2E78_recomp.dll" ^
     --user-dir "%LOCALAPPDATA%\DOR2Recomp"

Or run DOR2Recomp.exe to use the launcher window.

Controls
--------
See PLAYERS.md. The runtime generates a GameCube controller profile for the
first detected gamepad.

Saves and logs
--------------
- Memory card: %LOCALAPPDATA%\DOR2Recomp\GC
- Savestates:  %LOCALAPPDATA%\DOR2Recomp\StateSaves
- Logs:        %LOCALAPPDATA%\DOR2Recomp\Logs
- Settings:    %LOCALAPPDATA%\DOR2Recomp\config.ini

When using RecompLauncher, the data folder is the launcher's local app folder
for this game instead of %LOCALAPPDATA%.

Troubleshooting
---------------
- "The disc did not validate": the dump is not the USA retail disc. Verify its
  size and CRC-32 against the values above.
- Black screen at boot: make sure Sys\GC\dsp_rom.bin and Sys\GC\dsp_coef.bin
  exist next to the executables, and that your GPU driver supports Vulkan.
- Low frame rate: try --core jit in RecompLauncher's CPU core setting to
  compare against Dolphin's reference core, and close background GPU load.
- Game window will not close: check Logs\ModernGekko.log for the last line.

CPU core
--------
- Static recomp: the generated native module (default).
- JIT: Dolphin's reference core, selectable in RecompLauncher.

Frame rate is locked to the GameCube VI rate of 59.94 Hz. There is no uncapped
mode.

License
-------
Project code: GPL-3.0. See LICENSE.
DolRecomp: GPL-3.0. ModernGekko/Dolphin: GPL-2.0-or-later (aggregate GPL-3.0).
WWE and all related marks are trademarks of their respective owners. This is
an unofficial fan project with no affiliation.
