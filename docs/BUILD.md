# Build guide

Exact commands for a Windows x64 build. All paths are relative to the repository
root.

## Toolchain

Verified on this machine:

| Tool | Version |
| --- | --- |
| Visual Studio Build Tools | 2026 / MSVC 19.51 |
| LLVM/Clang | 22.1.8 (`C:\Program Files\LLVM\bin`) |
| CMake | 4.5 (3.24 minimum) |
| Ninja | 1.13+ |
| Python | 3.13+ (helper scripts only) |

Clang must be on `PATH` or in `C:\Program Files\LLVM\bin`. The runtime itself is
built with MSVC; the generated game module must be built with Clang.

## 1. Clone with submodules

```powershell
git clone --recurse-submodules <repo-url> "DOR2 Static Recomp"
cd "DOR2 Static Recomp"
```

If the clone was made without submodules:

```powershell
git submodule update --init --recursive
```

`lib/ModernGekko-Template` is a pinned submodule; its own `lib/DolRecomp` and
`lib/ModernGekko` submodules (and Dolphin's nested vendor tree) are required.
The first sync pulls several hundred MB.

## 2. Build the runtime

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

This configures `build\moderngekko` once, then builds:

- `moderngekko-run` -> `DOR2RecompRunnerCore.exe`
- `moderngekko-launcher` -> `DOR2Recomp.exe`
- `DOR2RecompRunner.exe` (the `--core` shim)
- `DOR2Prepare.exe`

Do **not** clean the build tree between changes. A cold Dolphin build is
expensive.

The equivalent manual command (from a Visual Studio developer prompt):

```powershell
cmake -S . -B build\moderngekko -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build\moderngekko --target moderngekko-run moderngekko-launcher moderngekko-port DOR2Prepare -j 8
```

Branding, the required disc ID (`GW2E78`) and the retail DOL SHA-256 are set in
the top-level `CMakeLists.txt` before the runtime subdirectory is added.

## 3. Prepare the disc

The shipped extractor validates the retail dump and writes the layout the
runtime expects:

```powershell
build\moderngekko\DOR2Prepare.exe `
  --input "D:\dumps\WWE Day of Reckoning 2 (USA).iso" `
  --output work\game
```

Expected output ends with:

```text
progress 1.000 Extraction complete
DOR2PREPARE: disc_id=GW2E78 title="WWE Day of Reckoning 2" dol_sha256=e47abf... iso_crc32=bb092c3a
```

If the dump is RVZ, convert it first (the release manifest asks for ISO):

```powershell
python tools\rvz_extract.py "WWE Day of Reckoning 2 (USA).rvz" work\disc\game.iso
```

`--output` may be omitted to use `<exe-dir>\Game`. A valid existing extraction
is reused; pass `--force` to redo it.

## 4. Build the static module

```powershell
powershell -ExecutionPolicy Bypass -File scripts\recompile.ps1 `
  -Iso "D:\dumps\WWE Day of Reckoning 2 (USA).iso"
```

The script:

1. extracts the disc to `work\game` (git-ignored),
2. emits C with DolRecomp's C backend,
3. compiles it with Clang, and
4. copies `gGW2E78_recomp.dll` to `build\moderngekko\`.

Module builds use a short path (`C:\d2modules` by default, override with
`-ModuleRoot`) because generated chunk object paths exceed Windows/CMake limits
under folders containing spaces.

Compiler flags set by the script:

```text
-march=x86-64-v3 -mtune=generic
-DDOLRECOMP_C_LOOP_CYCLE_BUDGET=4096
-DDOLRECOMP_C_MAX_CALL_DEPTH=128
```

The module template adds `-O3 -flto=thin -fvisibility=hidden -ffp-contract=off
-fno-fast-math`.

### Verify the flags reached the compiler

```powershell
Select-String -Path C:\d2modules\GW2E78\*\module-build\CMakeCache.txt `
  -Pattern 'DOLRECOMP_C_'
Select-String -Path C:\d2modules\GW2E78\*\module-build\build.ninja `
  -Pattern 'DOLRECOMP_C_|march=x86-64-v3'
```

## 5. Run

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Core static
```

or directly:

```powershell
build\moderngekko\DOR2RecompRunner.exe `
  --core static `
  --game work\game `
  --module build\moderngekko\gGW2E78_recomp.dll `
  --user-dir work\user `
  --graphics Vulkan --audio Cubeb
```

The shim sets `MODERNGEKKO_STATICRECOMP=1` for static and `0` for JIT, then
starts `DOR2RecompRunnerCore.exe`. `--core jit` also adds
`--allow-interpreter`.

## 6. Evidence

A healthy boot log contains:

```text
[staticrecomp] module loaded: ... entry=0x80003154
[video] backend=Vulkan ...
[staticrecomp] shutdown: native=... fallback=0 native_exc=... hook_fb=... smc_failed=0
```

Read:

```powershell
Get-Content work\user\Logs\ModernGekko.log -Tail 80
```

`fallback=0` and `smc_failed=0` are the baseline.

### Counter semantics

- `fallback` counts interpreter/JIT fallback steps; `smc_failed` counts
  self-modifying-code chunk failures. Both must stay 0.
- `native_exc` counts **guest** exceptions raised by the generated code and
  delivered to the game's own exception vectors. Nonzero is normal for this
  title; it is not a host crash. Investigate only if it grows together with
  gameplay faults.
- `hook_fb` counts instructions executed through runtime hooks, mostly
  `dcbf`/`dcbst`/`dcbi`/`icbi` cache ops. Streaming games issue thousands per
  frame, so millions over a long run are expected.
- `STATICRECOMP_LOCKSTEP=1` enables differential verification against the
  interpreter (summary on stderr at shutdown). Known v0.1.0 limitation: FPSCR
  result-classification bits (FPRF/XX) can differ after single-precision and
  paired-single FP ops (`fmuls`, `vmhaddshs`), with occasional cascading
  float-formatting diffs. Baseline measured 2026-11-11: 4447 checks, 37
  reports, no crashes, `fallback=0`, `smc_failed=0`, full speed.

## PGO (later versions)

PGO is not part of v0.1.0. The template supports it through
`moderngekko-port pgo-run`. When adding it:

1. Build an instrumented module with `pgo-run`.
2. Play a representative segment (boot and gameplay separately).
3. Merge with `llvm-profdata merge`.
4. Rebuild with `-fprofile-use`.

Keep `.profraw`/`.profdata` under `work/pgo` (git-ignored).

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| `clang was not found` | LLVM not on `PATH` | install LLVM or set the fallback path |
| `vswhere.exe not found` | Build Tools missing | install the C++ x64 workload |
| `DOL SHA-256 does not match` | wrong dump or region | use the USA retail disc |
| `Failed to initialize DSP emulation` | missing DSP data | keep `Sys\GC\dsp_rom.bin` and `dsp_coef.bin` next to the runner |
| Module build path errors | long folder with spaces | use `-ModuleRoot C:\d2modules` |
| `fallback=` not zero | module/runtime mismatch | rebuild both from the same tree |
| Boot hangs with no frames after a forced kill | shader cache written by a killed run | delete `<user-dir>\Cache\Shaders` and retry |
| No `Logs\ModernGekko.log` | runner started without `--user-dir` | the shim tees stderr only when a user dir is given |
| Settings seem ignored (e.g. resolution falls back to 3x) | relative `--user-dir`/`--automation-dir` | the shim sets the core's cwd to the exe dir; always pass absolute paths |
