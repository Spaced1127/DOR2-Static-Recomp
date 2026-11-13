# Release guide

## What a release contains

```
recomplauncher.json
DOR2Recomp.exe
DOR2RecompRunner.exe
gGW2E78_recomp.dll
dor2_grid.png
dor2_logo.png
Sys/...
LICENSES/...
README.txt
PLAYERS.md
```

The launcher (`DOR2Recomp.exe`) is the single entry point: open it, import the
disc, change settings, click Play. Play starts the runner
(`DOR2RecompRunner.exe`) directly. There is no `Play.cmd`, no separate
"core" executable and no shim — the runner selects the CPU core itself from
`--core jit|static`. `DOR2Prepare.exe` is a dev-only headless extractor and is
not shipped; the RecompLauncher prepare step runs the launcher's `--extract`.

No ISO/RVZ, DOL, FST, extracted `Game/` or `files/` tree, textures, audio,
saves or cover art.

## Version

- Tag: `v0.1.0`
- Asset: `DOR2StaticRecomp-v0.1.0-Windows-x64.zip`
- `recomplauncher.json` sits at the **top** of the zip.

## Build the package

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
powershell -ExecutionPolicy Bypass -File scripts\recompile.ps1 -Iso "<disc.iso>"
powershell -ExecutionPolicy Bypass -File scripts\package.ps1 -Version 0.1.0
```

The package script:

1. copies only the executables, module, runtime `Sys/` data and docs;
2. refuses to continue if it finds a disc image, DOL, file table, `Game/`,
   `files/`, texture, audio or save file;
3. validates `recomplauncher.json` against the official schema;
4. writes `SHA256SUMS.txt` and the zip.

## Manifest

- `game.kind` is `iso`. GameCube discs are not ISO 9660, so there is no
  `check`.
- `game.dumps` advertises the Redump USA size and CRC-32. `DOR2Prepare.exe`
  checks the disc ID and retail DOL hash during setup.
- `prepare` runs the launcher headless (`DOR2Recomp.exe --extract {game}`) and
  prints `progress 0..1` lines. It extracts into `Game/` beside the executable
  and records the path in `default-game.txt`.
- `start` runs the single runner `DOR2RecompRunner.exe` with the module and user
  directory; the game root is read from `default-game.txt`. The CPU core comes
  from `config.ini` (`cpu_core`) or `--core`.
- `settings` exposes CPU core, resolution, window mode, VSync and MSAA. There
  is no uncapped frame-rate option.
- The `repo` field must be the final `owner/name` before publishing. It is a
  placeholder until the repository exists.

Validate manually:

```powershell
python tools\validate_manifest.py docs\recomplauncher.schema.json `
  docs\release\recomplauncher.json
```

## Publish (requires explicit approval)

Nothing below is run automatically.

```powershell
# 1. confirm the working tree is clean and staged as intended
git status

# 2. commit and push by hand
git add -A
git commit -m "DOR2 Static Recomp v0.1.0"
git remote add origin https://github.com/<owner>/DOR2-Static-Recomp.git
git push -u origin main

# 3. tag and release
git tag v0.1.0
git push origin v0.1.0
gh release create v0.1.0 `
  dist\DOR2StaticRecomp-v0.1.0-Windows-x64.zip `
  dist\SHA256SUMS.txt `
  --title "DOR2 Static Recomp v0.1.0" `
  --notes-file dist\release-notes-v0.1.0.md
```

Do not replace the asset under the same tag: RecompLauncher caches releases for
six hours and a same-tag replacement is not an update.

## Verify the published artifact

1. Download the asset back from GitHub.
2. Compare its SHA-256 with `SHA256SUMS.txt`.
3. Confirm `recomplauncher.json` is at the top of the zip.
4. Validate the manifest against the schema.
5. Extract the icon from each executable.
6. Scan the contents for game data.
7. Install through RecompLauncher into an empty folder:
   - choose the ISO;
   - confirm the Redump match;
   - watch the progress lines;
   - reach "Ready to play";
   - press Play and confirm the runner (not the launcher) starts.
8. Change one setting, confirm the next `[video]` log line reflects it.
9. Confirm saves and logs appear in the configured folders.
10. Update from the previous release and confirm `Game/` survives.

## Release checklist

- [ ] `fallback=0`, `smc_failed=0` in the steady-state log
- [ ] no new exceptions in the log
- [ ] 59.94 Hz target measured on the reference scene
- [ ] package contains no game data
- [ ] manifest validates
- [ ] icons present on both executables
- [ ] zip hash recorded
- [ ] download-back verified
- [ ] user approved commit, tag and publish
