# AGENTS.md

Rules for automated contributors working on this repository.

Read first: `README.md`, `STATE.md`, `docs/BUILD.md`, then the relevant guide
document (performance, release, RecompLauncher).

## Hard rules

- **No game data** in Git, releases, fixtures or docs: no ISO/RVZ/GCM/WBFS,
  DOL, FST, `Game/`, `files/`, textures, audio, saves, savestates or cover art.
- **No generated-code edits.** Fix the emitter/runtime and regenerate. The
  generated chunks live under the module cache, never in this repo.
- **No clean builds** unless the user explicitly asks. `build\moderngekko` and
  the module cache are expensive.
- **No commit, tag, release or push** without explicit user approval.
- **No claim without evidence.** Build, run, load a real state, read
  `status.txt`, inspect the log. "Should work" is not evidence.
- **Do not edit submodules casually.** `lib/ModernGekko-Template` and its
  children are pinned. Project changes belong in the top-level `CMakeLists.txt`,
  `src/`, `scripts/`, `assets/` and `docs/`.
- **Stop after three failed variations** of the same fix. Name the assumption,
  test it directly, then change approach.

## Build gate

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
powershell -ExecutionPolicy Bypass -File scripts\recompile.ps1 -Iso "<disc.iso>"
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Core static
Get-Content work\user\Logs\ModernGekko.log -Tail 80
```

A change is not done until the runner boots from the module and the log shows
`fallback=0`, `smc_failed=0`, no new crash, and the target rate on the reference
scene.

## Performance rule

0. Fix the host first: pin to physical cores, create `Cache\Shaders`, close
   background GPU load, confirm audio is not muted.
1. Run the same scene at 1x/no AA and at shipped settings.
2. If low settings are not faster, the static module is CPU-bound.
3. Run the JIT reference (`--core jit`).
4. If JIT holds 59.94 and static does not, keep static as the artifact and say
   so with numbers. Do not hide it.
5. Profile with PGO counts and `STATICRECOMP_DISPATCH_SAMPLES=1` before
   micro-optimizing.

The GameCube VI is 59.94 Hz. 59.9 is full speed.

## Release gate

- `scripts\package.ps1` must pass its game-data scan.
- `recomplauncher.json` must validate against the official schema.
- Download the published asset back and verify the hash, top-level layout,
  icons and absence of game data.
- Never replace an asset under an existing tag; bump the version.

## Evidence to keep

- `status.txt` with `fps`, `vps`, `speed`, `frame_count`, `present_count`
- log tail with `[video]`, `[audio]`, `[staticrecomp]`
- exact command line and config
- before/after numbers
- release hash and verification output
