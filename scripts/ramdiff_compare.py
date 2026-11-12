#!/usr/bin/env python3
"""Diff static vs JIT RAM dumps at each checkpoint; report first divergence."""
import sys, pathlib

root = pathlib.Path(__file__).resolve().parent.parent
a = root / "work" / "ramdiff" / "rd-static"
b = root / "work" / "ramdiff" / "rd-jit"

def first_diff(x: bytes, y: bytes):
    n = min(len(x), len(y))
    # scan in 4K blocks for speed, then byte-precise
    for off in range(0, n, 4096):
        if x[off:off+4096] != y[off:off+4096]:
            for i in range(off, min(off+4096, n)):
                if x[i] != y[i]:
                    return i
    return None

steps = int(sys.argv[1]) if len(sys.argv) > 1 else 5
for i in range(steps+1):
    fa, fb = a / f"ram_{i}.bin", b / f"ram_{i}.bin"
    if not fa.exists() or not fb.exists():
        print(f"frame {i*60}: missing dump"); continue
    x, y = fa.read_bytes(), fb.read_bytes()
    if x == y:
        print(f"frame {i*60:5d}: identical ({len(x)} bytes)")
        continue
    d = first_diff(x, y)
    # count differing bytes + bounding region
    nd = sum(1 for j in range(len(x)) if x[j] != y[j])
    print(f"frame {i*60:5d}: DIFFER first_off=0x{d:08x} (guest 0x{0x80000000+d:08x}) ndiff={nd}")
    # show a window around first diff
    lo = max(0, d-16); hi = min(len(x), d+48)
    print("  static:", x[lo:hi].hex(' '))
    print("  jit   :", y[lo:hi].hex(' '))
