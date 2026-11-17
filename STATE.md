# STATE

Updated: 2026-11-16 (v0.1.6 controller hotfix, pending approval)

## Disc

- Title: WWE Day of Reckoning 2 (USA)
- Disc ID: `GW2E78`
- Region: USA (`E`)
- ISO size: `1,459,978,240` bytes
- ISO CRC-32: `bb092c3a`
- Boot DOL SHA-256: `e47abf73e2c7162098c01de34b90ab66031c3f161fd72b36dde0173ea4af3610`
- Entry point: `0x80003154`
- FST: 3,400 files, 1,273,577,729 bytes
- Local dump: `WWE Day of Reckoning 2 (USA).rvz` (git-ignored); ISO at
  `work\disc\dor2.iso`. **Native RVZ support** (no manual conversion): the
  launcher picker + DiscIO extractor accept `.rvz`; `tools/rvz_extract.py`
  (zstandard, reused from the guide) decodes RVZ directly to `sys/`+`files/`;
  `DOR2Prepare.exe` detects `RVZ\x01` and delegates to it; `recompile.ps1 -Iso`
  accepts `.rvz`. Acceptance (RVZ path vs ISO path): 3405 files, 0 mismatches,
  identical aggregate tree SHA-256 `cba194579c752a7d177d8cdbb0d065cc61283bf74b443a2158a6fc3273da972f`;
  `main.dol` `e47abf73…`, `fst.bin` `a0a279f7…`, reconstructed ISO CRC `bb092c3a`.

## Build

- Runtime: ModernGekko-Template @ `c2553a99584b2045dbe46a70e1570994dae8beef`
- ModernGekko @ `edd460adad0e82b56909af1d793642c83b6d31c8`
- DolRecomp @ `1bec3554ecc4817cf78319ca3d8a0669477f29fa`
- **Local submodule modifications (takeover, user-approved, uncommitted):**
  - `tools/frontend_config.{hpp,cpp}`: parse/validate/round-trip `msaa`, `ssaa`,
    `anisotropy`, `vsync`, `efb_access`, `aspect`, `widescreen_hack`, `mute`,
    `volume`, `emulation_speed`, `cpu_core`
  - `tools/moderngekko_run.cpp`: map those keys into `RuntimeConfig`
  - `include/moderngekko/runtime.hpp`: matching `GraphicsSettings`/
    `AudioSettings`/`RuntimeConfig` fields
  - `src/runtime/dolphin_runtime.cpp`: `Config::SetBase` wiring for the above,
    `[video]`/`[audio]` stderr lines, `cpu_core` fallback (env wins)
  - `tests/frontend_config_test.cpp`: round-trip + validation coverage
- Project-owned: `src/runner-shim/main.cpp` is **deprecated/unused** as of
  v0.1.2. The runner (`moderngekko-run` → `DOR2RecompRunner.exe`) now selects
  the CPU core natively (`--core jit|static`), so no shim or separate
  `DOR2RecompRunnerCore.exe` is built or shipped. The launcher's Play path
  captures runner stderr into `<user-dir>\Logs\ModernGekko.log`; the dev
  scripts (`run.ps1`, `work/verify-package.ps1`) capture it by redirection.
- Runtime compiler: MSVC 19.51 (VS 2026 Build Tools)
- Module compiler: Clang 22.1.8, `-march=x86-64-v3`, `-O3`, ThinLTO,
  loop budget 4096, call depth 128
- Module output: `build\moderngekko\gGW2E78_recomp.dll`
- Module SHA-256: `72ddd48bcc85611432c71fde92fdc6ffb1dfaf2a9de473e63fa290957fd449ef`
  (unchanged by the takeover; no emitter edits yet)
- Module cache: `C:\d2modules\GW2E78\<dol-sha>-<key>\`

## Runtime

- Default core: **JIT** (user-approved ship decision 2026-11-11); static recomp
  is an experimental opt-in (`--core static` / `cpu_core=static`)
- `cpu_core=static|jit` in `config.ini` honored when the env override is absent
  (launcher Play path); `--core` / `MODERNGEKKO_STATICRECOMP` win
- Graphics: Vulkan; audio: Cubeb (DSP LLE)
- Settings honored from `config.ini`: resolution, backend, fullscreen,
  show_fps_in_title, msaa, ssaa, anisotropy, vsync, efb_access, aspect,
  widescreen_hack, mute, volume, emulation_speed (clamped to 1.0), cpu_core
- User directory: `work\user*` for local tests, `{local}\DOR2Recomp` in releases
- Controllers: **baked into the runner** (no launcher picker needed, works via
  RecompLauncher). `EnsureDefaultControllerConfig` detects connected SDL pads
  every boot (0.8 s warm-up for Bluetooth pads), logs name/GUID/VID:PID/layout/
  dpad/source + the SDL mapping, and keeps a matching `GCPadNew.ini` or
  regenerates it when the configured pad is gone (pad-swap safe). **A boot with
  no pad detected keeps the existing profile** (v0.1.6; v0.1.5 overwrote it with
  the keyboard profile, which is why Xbox pads looked dead). Layout is
  vendor-scoped: Microsoft/Sony/Valve always standard, 8BitDo by live a/b index,
  Nintendo first-party by name. Position-consistent SDL mapping; up to 4 ports.
  Verified: DualSense (BT) on hardware; Xbox/Edge/Elite/8BitDo/PS3/PS2 offline
  matrix against the shipped SDL DB; profile-keep scenarios on a hidden desktop.
- Memory card (v0.1.7): raw 128 Mbit card in slot A, empty slot B.
  `Config/Dolphin.ini` `[Core] SlotA=1` / `SlotB=255` written by
  `ForceMemoryCardLayout` before `UICommon::CreateDirectories()` +
  `UICommon::Init()` (DolphinQt order), and forced into the live config layer
  after Init. Card file: `<user>\GC\MemoryCardA.<region>.raw` (16,777,216
  bytes). Never `SlotB=0` (Dummy device). Manifest seeds both keys.
- Target: 59.94 Hz (GameCube VI)

## Evidence (measured 2026-11-11, reference scene = mid-match savestate)

- [x] Runner boots to gameplay from the module (`docs\evidence\final-static`)
- [x] Title/gameplay renders through Vulkan (`frame.png` in evidence dirs)
- [x] Steady-state 59.94 Hz measured (table below)
- [x] `fallback=0`, `smc_failed=0` in every static run
- [x] Automation runs captured in `work\auto-*`
- [x] Log tails saved in `docs\evidence` and `<user>\Logs\ModernGekko.log`

| Run | Core | Settings | fps | vps | speed |
| --- | --- | --- | --- | --- | --- |
| final-static | static | 3x, MSAA2, vsync on | 59.9489 | 59.9518 | 1.00165 |
| s-static-novsync | static | 3x, MSAA2, vsync off | 59.9329 | 59.9424 | 0.99893 |
| s-static-1x | static | 1x, MSAA1, vsync off | 59.9388 | 59.9401 | 0.99973 |
| s-jit | jit | 3x, MSAA2, vsync on | 59.9790 | 59.9780 | 1.00361 |
| s-jit-config2 | jit (config) | 3x, MSAA2, vsync on | 59.9398 | 59.9401 | 1.00000 |
| prof-light | static | 3x, MSAA1, vsync off | 59.9399 | — | 0.99816 |
| prof-heavy (attract) | static | 3x, MSAA1, vsync off | 59.9315 | — | 1.00000 |
| soak-static (attract) | static | 4x, MSAA1, vsync off | 39.4906 | 39.4456 | 0.65418 |
| soak-jit (attract) | jit | 4x, MSAA1, vsync off | 59.9386 | 59.9401 | 0.99993 |

- One early shipped-settings run (`s-static`) averaged 59.1235 fps during
  shader warm-up; later identical-config runs hold 59.94+. Disclosed, not hidden.
- Lockstep (`STATICRECOMP_LOCKSTEP=1`, 60 s): checks=4447 reports=37
  skipped_fallback=17 skipped_zero=42 undercharges=0. Report classes:
  FPSCR FPRF/XX after single/paired-single FP ops (known approximation), and
  loop-budget partial-block artifacts at memmove/string loops (verifier
  compares a budget-truncated native block against a full shadow block).
- Settings reflection verified: `[video]` lines differ correctly between
  shipped and 1x runs (efb_scale 3 vs 1, msaa 2 vs 1, vsync 1 vs 0).
- Package artifact: `dist\DOR2StaticRecomp-v0.1.0-Windows-x64.zip`,
  SHA-256 `e6ed526d29583f1c90f12f8b9aa8c053e2a7befde599a5ed90ed3c1e0993ea67`
  (pre shim-Logs-fix repack; repack pending). Expanded-zip checks: hash,
  top-level layout, icons, game-data scan, schema 0 errors, staged
  `DOR2Prepare` (progress 1.000, DOL/CRC match) and staged runner boot.

## Next actions

**LAUNCH PAUSE (hub directive, 2026-11-16):** the user is at the machine - do NOT
launch anything (no game runner, launcher, Ryujinx, no windows, no audio) until
the hub lifts the pause. Everything below that needs a launch is BLOCKED; only
code/analysis/build/docs work is allowed.

1. [BLOCKED - launch] Drive the in-game Load menu with automation pad input to
   prove save -> load on screen (currently inferred from the unchanged card
   across boots).
2. [BLOCKED - launch] Run `scripts\test-memcard.ps1` (authored, never executed:
   fresh-dir card lifecycle gate) once the pause lifts.
3. [BLOCKED - launch] Optional fresh perf line for v0.1.7: static boot with a
   savestate, capture `status.txt` (fps/vps/speed) and `fallback=0`/`smc_failed=0`.
4. [host only] Ask players for the `controller:` diagnostic lines from pads that
   still misbehave (name/GUID/layout/dpad/source/sdl_mapping); any uncovered GUID
   goes into `tools\controller_matrix.cpp` as a new case before touching the rules.
5. [host only] Memory-card follow-ups: decide whether to migrate saves out of the
   old `GC\<region>\Card A` folder into the raw card (or document a manual copy);
   folder-era savestates re-enable the folder device (documented).
6. [host only] Netplay path (CONFIRMED gap, not fixed - v0.1.7 matches the
   published tag and an unverified code change will not be committed):
   `tools/netplay_session.cpp:524` calls `UICommon::SetUserDirectory()` +
   `UICommon::Init()` itself and then `detail::SetExternalUICommon(true)`, so
   `Runtime::Create` skips `ForceMemoryCardLayout()` and
   `UICommon::CreateDirectories()`. Netplay therefore still runs slot A on the
   GCI-folder default and may resolve the card path before `GC/<region>` exists.
   Planned fix when launches are allowed again: call the same helper and
   `UICommon::CreateDirectories()` between `SetUserDirectory` and `Init` in
   `RunNetplayLobby`, then verify a netplay host/join boot writes into
   `GC/MemoryCardA.<region>.raw` and not `GC/<region>/Card A`.

## Open issues

| Symptom | First PC / log line | Owner | Status |
| --- | --- | --- | --- |
| Static core slows at 4x (39.5 fps attract) while JIT holds 59.94; user reports entrances/fatal-4-way slow at 4x | GPU ~5%, 1-2 CPU cores hot; 3x full speed in all tested scenes | performance | attributing (per-thread/GPU at 4x) |
| Invalid read spam in user match at 4x | `Invalid read from 0x8180001c, PC = 0x800055e4` (memmove byte loop; 0x1c past MEM1 end) | correctness | repro soak running (4x + match state) |
| FPSCR FPRF/XX divergence vs interpreter | lockstep reports after `fmuls`/`vmhaddshs` | correctness | accepted v0.1.0 limitation, documented |
| Lockstep false positives on budget-truncated loops | DIVERGE #12/#20-23 (r5 delta = remaining iterations) | tooling | verifier improvement, v0.2.0 |
| Force-killed run can poison shader cache; next boot hangs | no frames, `booted=1` | robustness | documented workaround: delete `Cache\Shaders` |
| Heavy-content corruption: wild-pointer invalid reads/writes, then fps collapse from panic-alert spam | `Invalid read 0x8180001c PC=0x800055e4`; `Invalid write 0x511c65a0 PC=0x800e9130`; attract `Invalid read 0x329b4xxx PC=0x80123728`; MMU-on `Invalid read 0x3fb PC=0x800b584c` | correctness | static-core-only (JIT clean). v0.1.1 RAM-differential (identical savestate, no input) shows static's timebase runs ~16-22 ticks ahead of JIT plus DMA/asset-buffer timing offsets; a structural divergence burst at ~frame 180. Root cause = cycle/timing fidelity (timebase accounting + DMA/interrupt latency), not a single codegen bug. Full fix = cycle-accurate timing (large effort); JIT default ships as the safe core. |

## Release

- Version: **v0.1.7** (published 2026-11-16). Supersedes v0.1.5. v0.1.6
  (controller-only) was staged and never published; v0.1.7 contains both fixes.
- Ship decision (user-approved 2026-11-11): **JIT default, static recomp opt-in
  (experimental)** due to the rare static-core heavy-mode corruption.
- v0.1.7 adds the **memory-card fix** (shared pattern with Aggressive Inline):
  - `Config/Dolphin.ini` gets `[Core] SlotA = 1` (raw card), `SlotB = 255`
    (none; never 0 = Dummy -> "THE DEVICE IN SLOT B IS NOT SUPPORTED").
  - `Runtime::Create` (`dolphin_runtime.cpp`, `ForceMemoryCardLayout`) writes
    those keys, forces them into the live config layer (`Config::SetBase`
    *after* `UICommon::Init`; doing it before Init crashed 0xC0000005 because
    the config layers did not exist yet), then
    `UICommon::CreateDirectories()` -> `UICommon::Init()` (DolphinQt order) so
    `GC/USA|EUR|JAP` exist before the EXI bus resolves the card path. Applies to
    every launch, incl. RecompLauncher (which never opens the settings GUI).
  - Manifest (`docs/release/recomplauncher.json`) seeds `Core/SlotA=1` and
    `Core/SlotB=255` in ensure/always/new for launcher-only installs.
  - Verified (fresh user dir, hidden desktop): GC/USA|EUR|JAP created,
    `effective slot A=1 slot B=255`, `GC/MemoryCardA.USA.raw` = 16,777,216
    bytes; the game wrote its save (dir + backup entries `GW2E/78//wmed_sys`);
    card SHA `1E2860E5…DEA52983` unchanged across a second boot (save read and
    accepted, no `Card A` folder). Staged zip reproduces it (SHA
    `AB53CE79…FB1433B`, same save entry).
  - Savestate caveat confirmed: loading `work/states/boot-static.sav` (folder
    era) restores `MemoryCardFolder` -> both the raw card and
    `GC/USA/Card A/78-GW2E-__2f__wmed_sys.gci` appear. Test cards on fresh
    boots, not old states.
  - Not verified: in-game Load menu driven by script (no automated menu input),
    so save->load is inferred from the unchanged card, not shown on screen.
- v0.1.7 also contains the **controller hotfix** (details below): Xbox
  "not working at all" root cause = v0.1.5 overwrote a real pad profile with the
  keyboard profile whenever no pad had enumerated yet; and the `a>b => Nintendo`
  rule is now scoped to 8BitDo (0x2DC8) so PS3/PS2/generic pads keep standard
  buttons and unmirrored D-pads.
- Published: **yes** (v0.1.7) - https://github.com/Spaced1127/DOR2-Static-Recomp/releases/tag/v0.1.7
  (user-approved 2026-11-16; commit 1c083a1, tag v0.1.7, assets
  `DOR2StaticRecomp-v0.1.7-Windows-x64.zip` + `SHA256SUMS.txt`).
  Prior: v0.1.5 (69f9a5d), v0.1.4 (0de9381), v0.1.3 (96de8a8), v0.1.2 (1b5585f),
  v0.1.1 (c31cb7f), v0.1.0 (7f11135).
- v0.1.5 added: **8BitDo Nintendo-layout fix** (per-GUID live SDL a/b detection;
  Pro 2/SN30/SF30/N30 Nintendo-mode get A/B+X/Y swapped + reversed D-pad
  corrected; Ultimate/64 stay standard) and **DualSense Edge +
  Xbox Elite S1/S2 support**. Defects fixed in v0.1.7 above.
- v0.1.4 added: **native RVZ support** (no manual RVZ->ISO conversion). Launcher
  picker + DiscIO read `.rvz`; `tools/rvz_extract.py` (zstd + Dolphin LFG padding,
  reused from the guide) decodes RVZ directly to `sys/`+`files/` (legacy
  positional RVZ->ISO kept); `DOR2Prepare` detects `RVZ\x01` and delegates;
  `recompile.ps1 -Iso` accepts `.rvz`. Acceptance: RVZ path byte-identical to ISO
  path (3405 files, 0 mismatches; aggregate tree SHA-256 `cba19457…`).
- v0.1.3 added: **controller support baked into the runner** (RecompLauncher has
  no controller picker). Auto-detects Xbox/PlayStation/Switch/generic SDL pads
  every boot, logs the family, keeps a matching `GCPadNew.ini` or regenerates it
  when the configured pad is gone (pad-swap safe, up to 4 ports), position-
  consistent SDL mapping. Verified in-game with an Xbox One pad (fresh/adapt/keep).
- v0.1.2 added: **single-runner launcher parity** with the shared launcher —
  one launcher (`DOR2Recomp.exe`) + one runner (`DOR2RecompRunner.exe`, native
  `--core`); removed the runner-shim + `DOR2RecompRunnerCore.exe` pair and
  `Play.cmd`; RecompLauncher prepare now runs the launcher headless
  (`--extract`) instead of `DOR2Prepare.exe`; licenses in `LICENSES/`. Plus the
  SB-parity themed UI, SteamGridDB cover/logo/icon (user-approved), resize-safe
  SDL background, card/sidebar text margins, and a UTF-8 `git apply`-able patch.
- Manifest: `docs\release\recomplauncher.json` (validates, 0 errors);
  `prepare` = `DOR2Recomp.exe --extract {game}`; `start` = single runner with
  `--module`/`--user-dir`; `cpu_core=jit` in ensure/new
- Package (v0.1.7, prepared): `dist\DOR2StaticRecomp-v0.1.7-Windows-x64\` + zip +
  `SHA256SUMS.txt`; zip SHA-256
  `b986ad02970eef5dc6abc29ac0f9c0a0834c6fd7acc7cf66c18e875e0df0038c`
  (superseded staging: v0.1.6 `214c64b2…c43800a1`, never published)
- Verified (v0.1.7): package game-data scan clean; schema 0 errors (incl. the new
  `Config/Dolphin.ini` entry); staged exes report 0.1.7.0; staged runner boot on a
  hidden desktop creates `GC/MemoryCardA.USA.raw` (16,777,216 bytes, SHA
  `AB53CE79…FB1433B`, save entry `GW2E/78//wmed_sys`), writes the `[memcard]`
  lines, keeps the controller diagnostic, Vulkan backend up; static core loads
  the module (`module loaded ... entry=0x80003154`).
- Not re-measured for v0.1.7: 59.94 fps / `fallback=0` / `smc_failed=0`. No
  core/emitter/module changes (module hash unchanged); re-run
  `scripts\run.ps1 -Core static` with a savestate before publishing if a fresh
  perf line is wanted. Note: a *new* card is created on a fresh user dir, so do
  not mix old folder-era savestates into a card test (see the savestate caveat).
- Package: `dist\DOR2StaticRecomp-v0.1.5-Windows-x64\` + zip + `SHA256SUMS.txt`
- Zip SHA-256 (v0.1.5): `58af3a39f0aa765d298f1a45b2fda58a167793867b3e490eb2fc4a5e45bef975`
  (prior v0.1.4: `8221a18558c0cbdab981a5b16ec8df8fb383f59bfc3029a8f30b21a78fc294c4`)
- Verified: expanded-zip hash/layout/icons(both exes)/game-data-scan/schema(0 err);
  staged launcher `--extract` (progress 1.000); staged single-runner JIT boot on a
  hidden desktop (run-hidden.ps1) 59.9403 fps / vps 59.9401 / speed 1; controller
  unit tests pass (8BitDo Nintendo/Ultimate/64/Switch-mode/reversed dpad/no regression).
- Release notes: `dist\release-notes-v0.1.7.md` (superseded: v0.1.6, v0.1.5)
- Published: **yes** — https://github.com/Spaced1127/DOR2-Static-Recomp/releases/tag/v0.1.5
  (user-approved; commit 69f9a5d, tag v0.1.5). Prior: v0.1.4 (0de9381), v0.1.3 (96de8a8), v0.1.2 (1b5585f), v0.1.1 (c31cb7f), v0.1.0 (7f11135).
- `repo` field: `Spaced1127/DOR2-Static-Recomp`
- Download-back verified (v0.1.7): asset SHA-256 matches `SHA256SUMS.txt` and the
  local build (`151206e5…f96c55`, 33,764,697 bytes); release public, not a draft;
  assets: zip + SHA256SUMS only; game-data scan clean on the downloaded zip.
- Release notes published from `dist\release-notes-v0.1.7.md`.
- Release notes amended 2026-11-16 (text only, assets untouched) to state
  explicitly: profile rewrite only with a pad present, device fallback, SDL hint
  parity, 8BitDo scoping, DualSense/Edge D-pad un-mirrored, Elite S2 `0B22`,
  memcard migration for existing installs, the unverified in-game Load path, and
  the "send us your controller line" ask. No rebuild was needed: the packaged
  payload (runner, module, `recomplauncher.json`, `PLAYERS.md`) hashes are
  identical to the final tree, so v0.1.7 already *is* the combined artifact.
- Download-back verified (v0.1.5): zip sha256 matches SHA256SUMS; recomplauncher.json
  at top; parity layout (no Play.cmd/RunnerCore/Prepare); identical DOR2 icon on
  both exes; game-data scan clean; schema 0 errors.
- Source reproducibility: upstream submodule pins + `patches/moderngekko-settings.patch`
  applied via `scripts/apply-patches.ps1` (see REPRODUCE.md).
- Compliance: savestates moved out of `docs\evidence` into `work\states`
  (game data must not enter Git); `smoke-test.ps1` writes states there;
  `.planning/` git-ignored; README legal wording now discloses the shipped
  GameCube DSP firmware (`Sys/GC/dsp_rom.bin`, `dsp_coef.bin`) per
  `docs/ASSETS.md` — review before redistributing.

## Notes

- **Launch pause (hub directive 2026-11-16, in force):** no launches at all -
  no runner, launcher, Ryujinx, no test windows and no audio while the user is at
  the machine, until the hub lifts it. Host-side work only (code, analysis,
  builds without GUI, docs). Test cycles suspended; blocked actions are listed
  under "Next actions".
- **Quiet mode lifted (user directive 2026-11-16):** when launches ARE allowed
  again, run test apps as normal VISIBLE windows. Do not use
  `H:\Orchestration\tools\run-hidden.ps1` or any hidden-desktop/minimize/
  kill-window trick. End test runs with the automation `command=stop` file
  (`--automation-dir <dir>`, write `<dir>\commands\stop.txt`), which exits
  gracefully and flushes the memory card.

- Inherited scaffold (prior agent): project structure, scripts, manifest,
  branding, first boot evidence. Takeover added: settings implementation,
  log tee, compliance fixes, verification matrix, this STATE.
- The launcher captures runner stderr into `<user>\Logs\ModernGekko.log`
  itself (exclusive handle while running). As of v0.1.2 there is no shim: the
  single runner writes diagnostics to stderr, and the dev scripts capture them
  by redirection. Direct RecompLauncher launches rely on the launcher's Logs
  action / Dolphin's own log, matching the shared launcher.
- No clean builds: keep `build\moderngekko` and the module cache between runs.
