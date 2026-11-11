# Disc and DOL helpers

These scripts are diagnostic helpers copied from the
[GameCube Static Recomp Guide](https://github.com/Spaced1127/GameCube-Static-Recomp-Guide)
(`tools/`), GPL-3.0, with attribution. They never write into a disc image.

| Tool | Purpose |
| --- | --- |
| `gcdisc.py` | Inspect a GameCube ISO, list/extract the FST, dump DOL sections |
| `rvz_extract.py` | Convert a GameCube RVZ to ISO (Zstd/none/padding) |
| `verify_dol.py` | Compare every `main.dol` copy against the disc |
| `dol_disasm.py` | Disassemble a DOL at a virtual address (Capstone) |
| `validate_manifest.py` | Validate a RecompLauncher manifest against the schema |

## Examples

```powershell
python tools\rvz_extract.py "WWE Day of Reckoning 2 (USA).rvz" work\disc\game.iso
python tools\gcdisc.py info work\disc\game.iso
python tools\gcdisc.py list work\disc\game.iso sound
python tools\gcdisc.py extract work\disc\game.iso work\disc\raw
python tools\validate_manifest.py docs\recomplauncher.schema.json `
  docs\release\recomplauncher.json
```

Requirements: Python 3, `zstandard` for RVZ, `jsonschema` for manifests,
`capstone` for disassembly.

The shipped `DOR2Prepare.exe` implements the production extraction path in
native C++ and does not require Python at play time.
