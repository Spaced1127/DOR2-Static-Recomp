# Static-core corruption investigation (v0.1.1)

Status: root cause identified as a **cycle/timing-fidelity** difference in the
experimental static recomp core; the default **JIT** core is unaffected and is
the shipped safe core. A full fix (cycle-accurate timing) is a separate, larger
effort.

## Symptom

Rare, in some heavy modes (entrances / fatal-4-way): the static core corrupts
guest memory, producing wild-pointer `Invalid read/write` panic alerts and a
frame-rate collapse (the alert spam blocks the CPU thread). Observed PCs:
`0x800055e4` (memmove), `0x800e9130`, `0x80123728`, `0x800b584c`. JIT runs the
same content at 60 fps with zero faults.

## Method (reproducible)

`scripts/ramdiff.ps1` loads the identical savestate (`work/states/diag-long.sav`)
on one core, then uses the automation protocol (`read_memory` 16 MB +
`pad_frames` with no input) to dump guest RAM at frame checkpoints.
`scripts/ramdiff_compare.py` diffs static vs JIT dumps byte-for-byte.

```powershell
powershell -File scripts\ramdiff.ps1 -Core static -Tag rd-static
powershell -File scripts\ramdiff.ps1 -Core jit    -Tag rd-jit
python scripts\ramdiff_compare.py 5
```

## Findings (2026-11-11)

- Even at frame 0 the cores differ; the recurring first divergence is a
  magic-double int->float temporary (`43 30 00 00 00 XX XX XX`) whose integer is
  a **timebase/tick count**: static reads ~16-22 ticks ahead of JIT. The game
  converts timebase to float every frame, propagating the offset.
- A structural divergence burst at ~frame 180 (35904 bytes, 30 regions; largest
  33 KB at `0x80cf0594`, plus `0x80593xxx` / `0x8078axxx` heap/asset blocks) is
  consistent with DMA/asset transfers completing at different instants.
- Conclusion: the static core's **timebase accounting and DMA/interrupt latency**
  differ from the reference; the game's timing-sensitive logic occasionally
  corrupts under that offset. This is a fidelity issue, not a single data-path
  codegen bug (CR0/CTR/branches, `dcbz`, `lha`, mem helpers, SJIS parser, FP
  helpers all reviewed correct; MMU-on does not fix it).

## Eliminated hypotheses

- Single codegen bug (reviewed correct): CR0/CTR/BO, `dcbz`, `lha`, mem helpers,
  SJIS parser + int->float conversion, FP helpers.
- MMU translation gap (MMU-on still crashes).
- FPSCR feedback (only 2 `mffs` in the whole DOL).
- Lockstep integer "divergences" are loop-budget truncation artifacts, not bugs.

## Next steps (if pursuing a fix)

1. Make static timebase advancement cycle-accurate (match per-instruction
   accounting, not per-block budget charges).
2. Align DMA/interrupt delivery latency with the reference.
3. Re-run the RAM differential until static==JIT (modulo masked timing scratch),
   then re-verify heavy scenes + user replay.
