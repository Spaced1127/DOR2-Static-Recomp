# Reproducing the build from a fresh clone

The runtime submodules are pinned to upstream (`ExpansionPak`). The DOR2
settings/log/core-default changes live in `lib/ModernGekko` and are carried as
a patch so the upstream pins stay clean.

```powershell
git clone --recurse-submodules https://github.com/Spaced1127/DOR2-Static-Recomp.git
cd DOR2-Static-Recomp

# Apply the DOR2 runtime delta (settings, [video]/[audio] logs, JIT default).
powershell -ExecutionPolicy Bypass -File scripts\apply-patches.ps1

# Build runtime + launcher + runner + prepare (needs VS Build Tools, LLVM, CMake, Ninja).
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# Generate + compile the game module from YOUR OWN USA disc (ISO).
powershell -ExecutionPolicy Bypass -File scripts\recompile.ps1 -Iso "<your USA disc.iso>"

# Run (JIT default) or opt into the experimental static core.
powershell -ExecutionPolicy Bypass -File scripts\run.ps1
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Core static
```

Notes:
- No game data is in this repository. Supply your own legally-dumped USA disc
  (Redump size 1,459,978,240, CRC-32 `bb092c3a`).
- `patches/moderngekko-settings.patch` applies to
  `lib/ModernGekko-Template/lib/ModernGekko` (pin `edd460ad`).
- The published release zip is prebuilt and self-contained; it does not require
  rebuilding the submodules.
