# DOR2 Static Recomp

A native Windows static recompilation of **WWE Day of Reckoning 2** (GameCube,
USA, `GW2E78`), built with [DolRecomp](https://github.com/ExpansionPak/DolRecomp)
and the [ModernGekko](https://github.com/ExpansionPak/ModernGekko) runtime,
following the [GameCube Static Recomp Guide](https://github.com/Spaced1127/GameCube-Static-Recomp-Guide).

This repository contains **no game data**. You must supply your own legally
dumped disc image. The launcher extracts it locally; nothing is uploaded.

## Status: v0.1.1

| Item | Value |
| --- | --- |
| Disc | `GW2E78` (USA), ISO 1,459,978,240 bytes, CRC-32 `bb092c3a` |
| Retail DOL SHA-256 | `e47abf73e2c7162098c01de34b90ab66031c3f161fd72b36dde0173ea4af3610` |
| Runtime | ModernGekko (Dolphin-derived GX/VI/audio); JIT default, static recomp opt-in |
| Target rate | 59.94 Hz (the GameCube VI rate) |
| Package | Standalone `DOR2Recomp.exe` launcher, `DOR2RecompRunner.exe`, `DOR2Prepare.exe` |
| Distribution | RecompLauncher-compatible release zip (`recomplauncher.json` at the top) |

## What is in the box

- `DOR2Recomp.exe` - the launcher. Import/extract a disc, pick graphics and
  controller settings, then play.
- `DOR2RecompRunner.exe` - the game process. Loads the static module, runs the
  GameCube runtime, renders through Vulkan.
- `DOR2Prepare.exe` - headless disc extractor used by RecompLauncher's setup
  step. Prints `progress` lines and validates the retail DOL.
- `gGW2E78_recomp.dll` - the generated native game module (built by you, not
  committed).

## Requirements

- Windows 10/11 x64
- Visual Studio 2022/2026 Build Tools with the C++ x64 workload and Windows SDK
- LLVM/Clang on `PATH` (`C:\Program Files\LLVM\bin`)
- CMake 3.24+ and Ninja
- Python 3 (only for the helper scripts)
- Your own USA disc image (`WWE Day of Reckoning 2 (USA).iso`)

## Build

```powershell
git clone --recurse-submodules <this-repo>
cd "<this-repo>"
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

`scripts\build.ps1` configures and builds the runtime, launcher, runner and
prepare tool into `build\moderngekko\`. Keep that build tree; a cold Dolphin
build is expensive.

Then generate and compile the game module from your own disc:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\recompile.ps1 `
  -Iso "D:\dumps\WWE Day of Reckoning 2 (USA).iso"
```

The script extracts the disc to `work\game\` (git-ignored), emits the static C
with DolRecomp, and builds `gGW2E78_recomp.dll` with Clang.

## Run

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run.ps1
```

or directly:

```powershell
build\moderngekko\DOR2RecompRunner.exe `
  --game work\game `
  --module build\moderngekko\gGW2E78_recomp.dll `
  --user-dir work\user `
  --graphics Vulkan --audio Cubeb
```

The runner also discovers `gGW2E78_recomp.dll` next to the executable or in
`<user-dir>\StaticRecompModules\`.

### CPU core

The runtime ships two cores:

- **jit** (default) - Dolphin's JIT, verified clean and full-speed on all
  tested content including entrances and fatal-4-way matches.
- **static** (experimental, opt-in) - the generated native module, selected
  with `--core static` or `cpu_core=static`. It is full-speed on the reference
  scene but has a rare, timing-sensitive memory-corruption bug in some heavy
  modes (see Known issues); prefer JIT for those.

`config.ini` accepts `cpu_core=static|jit`. The GameCube VI runs at 59.94 Hz;
`emulation_speed` is clamped to 1.0 and uncapped modes are not exposed.

### Known issues (v0.1.1)

- The experimental **static** core can, rarely, corrupt memory in some heavy
  modes (entrances / fatal-4-way), producing "Invalid read/write" panic alerts
  and a frame-rate collapse. The default **JIT** core is not affected. If you
  opt into static and see this, switch back to JIT. Root cause is under
  investigation for v0.1.1.
- The static core's FPSCR result-classification bits can differ from the
  interpreter after single-precision/paired-single FP ops (no gameplay impact
  observed on the reference scene).

## Package a release

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package.ps1
```

The script assembles `dist\DOR2StaticRecomp-v0.1.1-Windows-x64\`, refuses to
continue if any game data is present, validates `recomplauncher.json` against
the official schema, and writes a zip plus `SHA256SUMS.txt`.

## Documentation

- [`docs/BUILD.md`](docs/BUILD.md) - toolchain, exact commands, module flags
- [`docs/RELEASE.md`](docs/RELEASE.md) - packaging, RecompLauncher, verification
- [`docs/ASSETS.md`](docs/ASSETS.md) - original icon/branding policy
- [`STATE.md`](STATE.md) - current build, evidence and open issues
- [`AGENTS.md`](AGENTS.md) - rules for automated contributors

## Legal

No game disc images, DOL/FST files, textures, audio, saves or cover art are
included or downloaded. You must own and dump your own copy of the game. The
release does ship the GameCube DSP firmware (`Sys/GC/dsp_rom.bin`,
`dsp_coef.bin`) required for DSP LLE audio, as documented in
[`docs/ASSETS.md`](docs/ASSETS.md); review that policy before redistributing.
The project code is licensed under GPL-3.0; see [`LICENSE`](LICENSE).
DolRecomp is GPL-3.0, ModernGekko/Dolphin is GPL-2.0-or-later (aggregate
GPL-3.0). Third-party components keep their own licenses.
