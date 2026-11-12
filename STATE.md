# STATE

Updated: 2026-11-11 (takeover completion pass; supersedes the scaffold-era STATE)

## Disc

- Title: WWE Day of Reckoning 2 (USA)
- Disc ID: `GW2E78`
- Region: USA (`E`)
- ISO size: `1,459,978,240` bytes
- ISO CRC-32: `bb092c3a`
- Boot DOL SHA-256: `e47abf73e2c7162098c01de34b90ab66031c3f161fd72b36dde0173ea4af3610`
- Entry point: `0x80003154`
- FST: 3,400 files, 1,273,577,729 bytes
- Local dump: `WWE Day of Reckoning 2 (USA).rvz` (git-ignored); converted ISO at
  `work\disc\dor2.iso`

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
- Project-owned: `src/runner-shim/main.cpp` tees runtime stderr into
  `<user-dir>\Logs\ModernGekko.log` (recursive directory creation)
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

- Version: **v0.1.1** (current; supersedes v0.1.0)
- Ship decision (user-approved 2026-11-11): **JIT default, static recomp opt-in
  (experimental)** due to the rare static-core heavy-mode corruption.
- v0.1.1 adds: DOR2-themed launcher (parity with shared themed launchers) +
  corrected, consistent DOR2 icon across all four exes.
- Manifest: `docs\release\recomplauncher.json` (validates, 0 errors);
  `cpu_core=jit` in ensure/new; core setting relabeled JIT default / static experimental
- Package: `dist\DOR2StaticRecomp-v0.1.1-Windows-x64\` + zip + `SHA256SUMS.txt`
- Zip SHA-256 (v0.1.1): `090114b6146da145c22d4fa9ceda406630345f2e4c6b514b74c457cf520f4eb7`
- Verified: expanded-zip hash/layout/icons(1 unique across 4 exes)/game-data-scan/
  schema(0 err); staged DOR2Prepare (progress 1.000, DOL+CRC match); staged JIT boot 59.94 fps
- Release notes: `dist\release-notes-v0.1.1.md`
- Published: **yes** — https://github.com/Spaced1127/DOR2-Static-Recomp/releases/tag/v0.1.1
  (user-approved; commit c31cb7f, tag v0.1.1). Prior: v0.1.0 (commit 7f11135).
- `repo` field: `Spaced1127/DOR2-Static-Recomp`
- Download-back verified (v0.1.1): zip sha256 matches SHA256SUMS; recomplauncher.json
  at top; identical DOR2 icon on all exes; game-data scan clean; schema 0 errors.
- Source reproducibility: upstream submodule pins + `patches/moderngekko-settings.patch`
  applied via `scripts/apply-patches.ps1` (see REPRODUCE.md).
- Compliance: savestates moved out of `docs\evidence` into `work\states`
  (game data must not enter Git); `smoke-test.ps1` writes states there;
  `.planning/` git-ignored; README legal wording now discloses the shipped
  GameCube DSP firmware (`Sys/GC/dsp_rom.bin`, `dsp_coef.bin`) per
  `docs/ASSETS.md` — review before redistributing.

## Notes

- Inherited scaffold (prior agent): project structure, scripts, manifest,
  branding, first boot evidence. Takeover added: settings implementation,
  log tee, compliance fixes, verification matrix, this STATE.
- The launcher captures runner stderr into `<user>\Logs\ModernGekko.log`
  itself (exclusive handle while running); the shim tee covers Play.cmd /
  RecompLauncher paths.
- No clean builds: keep `build\moderngekko` and the module cache between runs.
