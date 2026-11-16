# Disc and DOL helpers

These scripts are diagnostic helpers copied from the
[GameCube Static Recomp Guide](https://github.com/Spaced1127/GameCube-Static-Recomp-Guide)
(`tools/`), GPL-3.0, with attribution. They never write into a disc image.

| Tool | Purpose |
| --- | --- |
| `gcdisc.py` | Inspect a GameCube ISO, list/extract the FST, dump DOL sections |
| `rvz_extract.py` | Decode a GameCube RVZ natively (Zstd/none/padding): extract `sys/`+`files/` directly, or reconstruct an ISO |
| `verify_dol.py` | Compare every `main.dol` copy against the disc |
| `dol_disasm.py` | Disassemble a DOL at a virtual address (Capstone) |
| `validate_manifest.py` | Validate a RecompLauncher manifest against the schema |
| `test_controller_layouts.py` | Offline proof that Edge/Elite/standard GUIDs map to standard-equivalent GameCube layouts (DB audit) |
| `controller_matrix.cpp` | Offline layout matrix: runs the real `DetectControllerMapping`/`GenerateControllerConfig` against the real SDL gamepad DB rows for Xbox 360/One/Series, Elite S1/S2, DualSense/Edge, 8BitDo Pro 2/Ultimate/64, Switch Pro, PS3/PS2 and asserts the GameCube layout per pad. Built and run by `scripts\test-controller-matrix.ps1` |

## Examples

```powershell
# Native RVZ -> extracted game (no manual ISO conversion); same files as the ISO path
python tools\rvz_extract.py --input "WWE Day of Reckoning 2 (USA).rvz" --output work\game

# Legacy RVZ -> ISO form (still supported)
python tools\rvz_extract.py "WWE Day of Reckoning 2 (USA).rvz" work\disc\game.iso
python tools\gcdisc.py info work\disc\game.iso
python tools\gcdisc.py list work\disc\game.iso sound
python tools\gcdisc.py extract work\disc\game.iso work\disc\raw
python tools\validate_manifest.py docs\recomplauncher.schema.json `
  docs\release\recomplauncher.json

# Controller gates (no controller hardware needed)
powershell -ExecutionPolicy Bypass -File scripts\test-controller-matrix.ps1
python tools\test_controller_layouts.py
# Real runner on a hidden desktop: a pad profile must survive a boot with no pad
powershell -ExecutionPolicy Bypass -File scripts\test-controller-profile.ps1
```

Requirements: Python 3, `zstandard` for RVZ, `jsonschema` for manifests,
`capstone` for disassembly.

The shipped launcher (`DOR2Recomp.exe`) extracts ISO **and RVZ** natively via
Dolphin's DiscIO (no Python at play time). The dev extractor `DOR2Prepare.exe`
handles ISO in native C++ and delegates RVZ to `tools/rvz_extract.py`, so
`scripts\recompile.ps1 -Iso <disc.rvz>` works with no manual conversion.
