#!/usr/bin/env python3
"""Native RVZ (Dolphin compressed disc image) extractor for DOR2 Static Recomp.

Reads a .rvz directly (no manual RVZ->ISO step) and produces the SAME extracted
layout as the ISO path (DOR2Prepare / the launcher's DiscIO extractor):

    <output>/sys/boot.bin
    <output>/sys/bi2.bin
    <output>/sys/apploader.img
    <output>/sys/fst.bin
    <output>/sys/main.dol
    <output>/files/...            (FST file tree)

The RVZ container decoding (zstd groups + Dolphin's lagged-Fibonacci padding
PRNG) is reused from the GameCube-Static-Recomp-Guide's rvz_extract.py, which
mirrors Dolphin's DiscIO WIA/RVZ reader. Only GameCube discs with Zstandard /
None compression are supported (this project's retail dump).

Usage:
    python tools/rvz_extract.py --input disc.rvz --output Game
    python tools/rvz_extract.py --input disc.rvz --iso disc.iso   # ISO only

Requires: zstandard  (pip install zstandard)
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import struct
import sys
import tempfile
import zlib
from pathlib import Path

import zstandard

RVZ_MAGIC = b"RVZ\x01"
DISC_HEADER_SIZE = 0x80
COMPRESSION_NONE = 0
COMPRESSION_ZSTD = 5
GC_MAGIC = 0xC2339F3D

MASK32 = 0xFFFFFFFF
LFG_K = 521
LFG_J = 32
LFG_SEED_WORDS = 17
LFG_BYTES = LFG_K * 4
BLOCK_SIZE = 0x8000

EXPECTED_DISC_ID = "GW2E78"
EXPECTED_DOL_SHA256 = "e47abf73e2c7162098c01de34b90ab66031c3f161fd72b36dde0173ea4af3610"


def progress(fraction: float, text: str) -> None:
    fraction = max(0.0, min(1.0, fraction))
    print(f"progress {fraction:.3f} {text}", flush=True)


class LaggedFibonacciGenerator:
    """Dolphin's GC/Wii disc padding PRNG (LaggedFibonacciGenerator.cpp)."""

    def __init__(self, seed: bytes):
        if len(seed) != LFG_SEED_WORDS * 4:
            raise ValueError(f"bad LFG seed size: {len(seed)}")
        self.buffer = [0] * LFG_K
        for i in range(LFG_SEED_WORDS):
            self.buffer[i] = struct.unpack_from(">I", seed, i * 4)[0]
        for i in range(LFG_SEED_WORDS, LFG_K):
            self.buffer[i] = (
                (self.buffer[i - 17] << 23)
                ^ (self.buffer[i - 16] >> 9)
                ^ self.buffer[i - 1]
            ) & MASK32
        for i in range(LFG_K):
            x = self.buffer[i]
            x = (x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000)
            self.buffer[i] = x
        self.pos = 0
        for _ in range(4):
            self._forward()

    def _forward(self):
        b = self.buffer
        for i in range(LFG_J):
            b[i] ^= b[i + LFG_K - LFG_J]
        for i in range(LFG_J, LFG_K):
            b[i] ^= b[i - LFG_J]

    def skip(self, count: int):
        self.pos += count
        while self.pos >= LFG_BYTES:
            self._forward()
            self.pos -= LFG_BYTES

    def get(self, count: int) -> bytes:
        out = bytearray()
        while count:
            word = self.buffer[self.pos // 4]
            byte_in_word = self.pos % 4
            raw = word.to_bytes(4, "big")
            take = min(count, 4 - byte_in_word)
            out += raw[byte_in_word:byte_in_word + take]
            self.pos += take
            count -= take
            if self.pos == LFG_BYTES:
                self._forward()
                self.pos = 0
        return bytes(out)


def unpack_rvz(stream: bytes, output_size: int, data_offset: int) -> bytes:
    out = bytearray()
    p = 0
    while len(out) < output_size:
        if p + 4 > len(stream):
            raise ValueError("RVZ pack stream truncated while reading run size")
        size = struct.unpack_from(">I", stream, p)[0]
        p += 4
        junk = bool(size & 0x80000000)
        size &= 0x7FFFFFFF
        if junk:
            if p + LFG_SEED_WORDS * 4 > len(stream):
                raise ValueError("RVZ pack stream truncated while reading seed")
            lfg = LaggedFibonacciGenerator(stream[p:p + LFG_SEED_WORDS * 4])
            p += LFG_SEED_WORDS * 4
            lfg.skip(data_offset % BLOCK_SIZE)
            out += lfg.get(size)
        else:
            if p + size > len(stream):
                raise ValueError("RVZ pack stream truncated while reading plain run")
            out += stream[p:p + size]
            p += size
        data_offset += size
    if len(out) != output_size:
        raise ValueError(f"RVZ pack produced {len(out)} bytes, expected {output_size}")
    return bytes(out)


def read_compressed_table(f, offset: int, size: int, compression: int, expect: int) -> bytes:
    f.seek(offset)
    data = f.read(size)
    if len(data) != size:
        raise ValueError("compressed table truncated")
    if compression == COMPRESSION_ZSTD:
        data = zstandard.ZstdDecompressor().decompress(data, max_output_size=expect)
    elif compression != COMPRESSION_NONE:
        raise NotImplementedError(f"compression type {compression} is not supported")
    if len(data) < expect:
        raise ValueError(f"table decompressed to {len(data)}, expected at least {expect}")
    return data


def rvz_to_iso(rvz_path: Path, iso_path: Path) -> int:
    """Reconstruct the raw GameCube ISO from the RVZ container. Returns ISO size."""
    with rvz_path.open("rb") as f:
        head = f.read(0x48)
        if len(head) != 0x48 or head[:4] != RVZ_MAGIC:
            raise ValueError("not an RVZ file (bad magic)")
        version, compat, header2_size = struct.unpack_from(">III", head, 4)
        iso_size, file_size = struct.unpack_from(">QQ", head, 0x24)
        if rvz_path.stat().st_size != file_size:
            raise ValueError("RVZ file size does not match header")

        h2 = f.read(header2_size)
        disc_type, compression, compr_level, chunk_size = struct.unpack_from(">IIII", h2, 0)
        if disc_type != 1:
            raise NotImplementedError(f"disc_type {disc_type} is not GameCube")
        if compression not in (COMPRESSION_NONE, COMPRESSION_ZSTD):
            raise NotImplementedError(f"compression type {compression} is not supported")
        if chunk_size % BLOCK_SIZE != 0:
            raise ValueError("invalid chunk size")

        disc_header = h2[0x10:0x10 + DISC_HEADER_SIZE]
        p = 0x10 + DISC_HEADER_SIZE
        n_part, part_t_size = struct.unpack_from(">II", h2, p)
        p += 8
        part_off, = struct.unpack_from(">Q", h2, p)
        p += 8 + 20
        n_raw, = struct.unpack_from(">I", h2, p)
        p += 4
        raw_off, = struct.unpack_from(">Q", h2, p)
        p += 8
        raw_size, = struct.unpack_from(">I", h2, p)
        p += 4
        n_groups, = struct.unpack_from(">I", h2, p)
        p += 4
        group_off, = struct.unpack_from(">Q", h2, p)
        p += 8
        group_size, = struct.unpack_from(">I", h2, p)
        if n_part != 0:
            raise NotImplementedError("Wii partition entries are not supported")

        raw_table = read_compressed_table(f, raw_off, raw_size, compression, n_raw * 24)
        entries = [struct.unpack_from(">QQII", raw_table, i * 24) for i in range(n_raw)]
        group_table = read_compressed_table(f, group_off, group_size, compression, n_groups * 12)

        progress(0.02, f"Decoding RVZ (ISO {iso_size:,} bytes, {n_groups:,} groups)")
        with iso_path.open("wb") as out:
            out.truncate(iso_size)
            out.seek(0)
            out.write(disc_header)
            for (data_off, data_size, group_index, groups) in entries:
                skipped = data_off % BLOCK_SIZE
                aligned_off = data_off - skipped
                aligned_size = data_size + skipped
                for i in range(groups):
                    total_group = group_index + i
                    file_off4, group_data_size, packed_size = struct.unpack_from(
                        ">III", group_table, total_group * 12)
                    compressed = bool(group_data_size & 0x80000000)
                    group_data_size &= 0x7FFFFFFF
                    this_chunk = min(chunk_size, aligned_size - i * chunk_size)
                    if this_chunk <= 0:
                        break
                    if group_data_size == 0:
                        chunk = bytes(this_chunk)
                    else:
                        f.seek(file_off4 * 4)
                        raw = f.read(group_data_size)
                        if len(raw) != group_data_size:
                            raise ValueError("group data truncated")
                        if compressed:
                            dec = zstandard.ZstdDecompressor()
                            if packed_size:
                                packed = dec.decompress(raw, max_output_size=packed_size)
                                if len(packed) != packed_size:
                                    raise ValueError("packed group size mismatch")
                                chunk = unpack_rvz(packed, this_chunk, i * chunk_size)
                            else:
                                chunk = dec.decompress(raw, max_output_size=this_chunk)
                                if len(chunk) != this_chunk:
                                    raise ValueError(f"group {total_group}: size mismatch")
                        else:
                            chunk = raw
                            if packed_size:
                                chunk = unpack_rvz(chunk, this_chunk, i * chunk_size)
                            if len(chunk) != this_chunk:
                                raise ValueError("uncompressed group size mismatch")
                    out.seek(aligned_off + i * chunk_size)
                    out.write(chunk)
                    if (i + 1) % 256 == 0 or i + 1 == groups:
                        done = aligned_off + (i + 1) * chunk_size
                        progress(0.02 + 0.40 * min(1.0, done / iso_size), "Decoding RVZ groups")
    return iso_size


def be32(b: bytes, o: int) -> int:
    return struct.unpack_from(">I", b, o)[0]


def decode_name(raw: bytes) -> str:
    try:
        return raw.decode("shift_jis")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def sanitize(name: str) -> str:
    for ch in '/\\:*?"<>|':
        name = name.replace(ch, "_")
    return "_" if name in (".", "..") else name


def parse_fst(fst: bytes):
    """Yield (relative_path, offset, size) for every file. Mirrors DOR2Prepare."""
    count = be32(fst, 8)
    if count == 0 or count * 12 > len(fst):
        raise ValueError("FST root entry count out of range")
    name_table = count * 12

    def name_at(off: int) -> str:
        start = name_table + off
        if start >= len(fst):
            return ""
        end = fst.index(b"\x00", start) if b"\x00" in fst[start:] else len(fst)
        return decode_name(fst[start:end])

    files = []

    def walk(index: int, rel: Path, end: int):
        i = index
        while i < end:
            word = be32(fst, i * 12)
            field1 = be32(fst, i * 12 + 4)
            field2 = be32(fst, i * 12 + 8)
            typ = word >> 24
            name = sanitize(name_at(word & 0x00FFFFFF))
            if typ == 1:
                child_end = min(field2, count)
                walk(i + 1, rel / name, child_end)
                i = child_end
            else:
                files.append((rel / name, field1, field2))
                i += 1

    if be32(fst, 0) >> 24 != 1:
        raise ValueError("FST root is not a directory")
    walk(1, Path(), count)
    return files


def extract_iso(iso_path: Path, output: Path, expect_id: str, expect_dol: str,
                no_dol_check: bool) -> dict:
    """Extract sys/ + files/ from a raw GameCube ISO. Mirrors DOR2Prepare output."""
    data = iso_path.read_bytes()
    if be32(data, 0x1C) != GC_MAGIC:
        raise ValueError("reconstructed image is not a GameCube disc (bad magic)")
    disc_id = data[0:6].decode("ascii")
    title = data[0x20:0x60].split(b"\x00")[0].decode("ascii", "replace")
    if disc_id != expect_id:
        raise ValueError(f"wrong disc: expected {expect_id}, found {disc_id}")

    dol_offset = be32(data, 0x420)
    fst_offset = be32(data, 0x424)
    fst_size = be32(data, 0x428)

    dol_end = 0x100
    for i in range(7):
        off = be32(data, dol_offset + i * 4)
        size = be32(data, dol_offset + 0x90 + i * 4)
        if size:
            dol_end = max(dol_end, off + size)
    for i in range(11):
        off = be32(data, dol_offset + 0x1C + i * 4)
        size = be32(data, dol_offset + 0xAC + i * 4)
        if size:
            dol_end = max(dol_end, off + size)
    dol = data[dol_offset:dol_offset + dol_end]
    dol_sha = hashlib.sha256(dol).hexdigest()
    if not no_dol_check and dol_sha != expect_dol:
        raise ValueError(f"DOL SHA-256 mismatch (got {dol_sha})")

    fst = data[fst_offset:fst_offset + fst_size]
    files = parse_fst(fst)

    app_size = 0
    app_header = data[0x2440:0x2460]
    if len(app_header) == 0x20:
        app_size = 0x20 + be32(app_header, 0x14) + be32(app_header, 0x18)

    progress(0.50, "Writing system files")
    staging = output.with_name(output.name + ".extracting")
    if staging.exists():
        shutil.rmtree(staging)
    (staging / "sys").mkdir(parents=True)
    (staging / "sys" / "boot.bin").write_bytes(data[0:0x440])
    (staging / "sys" / "bi2.bin").write_bytes(data[0x440:0x440 + 0x2000])
    (staging / "sys" / "main.dol").write_bytes(dol)
    (staging / "sys" / "fst.bin").write_bytes(fst)
    if app_size:
        (staging / "sys" / "apploader.img").write_bytes(data[0x2440:0x2440 + app_size])

    total = sum(size for _, _, size in files) or 1
    written = 0
    progress(0.55, "Extracting game files")
    for rel, off, size in files:
        dest = staging / "files" / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data[off:off + size])
        written += size
        if written % (64 << 20) < size:
            progress(0.55 + 0.40 * min(1.0, written / total), "Extracting game files")

    iso_crc = zlib.crc32(data) & 0xFFFFFFFF
    (staging / ".dor2-prepared").write_text(
        f"format=1\ndisc_id={disc_id}\ndol_sha256={dol_sha}\n"
        f"iso_size={len(data)}\niso_crc32={iso_crc:08x}\nprepared_by=rvz_extract.py\n",
        encoding="ascii")

    progress(0.98, "Publishing the extracted game")
    if output.exists():
        shutil.rmtree(output)
    staging.rename(output)
    progress(1.0, "Extraction complete")
    return {"disc_id": disc_id, "title": title, "dol_sha256": dol_sha,
            "iso_size": len(data), "iso_crc32": f"{iso_crc:08x}", "files": len(files)}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Native RVZ extractor for DOR2 Static Recomp")
    ap.add_argument("--input", "-i", help="input .rvz disc image")
    ap.add_argument("--output", "-o", help="extract sys/ + files/ into this folder")
    ap.add_argument("--iso", help="also/only reconstruct the raw ISO to this path")
    ap.add_argument("--expect-id", default=EXPECTED_DISC_ID)
    ap.add_argument("--expect-dol", default=EXPECTED_DOL_SHA256)
    ap.add_argument("--no-dol-check", action="store_true")
    ap.add_argument("positional", nargs="*", help=argparse.SUPPRESS)
    args = ap.parse_args(argv)

    # Legacy positional form (v0.1.0): rvz_extract.py input.rvz output.iso
    if not args.input and len(args.positional) == 2:
        args.input, args.iso = args.positional
    if not args.input:
        ap.print_usage(sys.stderr)
        print("error: pass --input <rvz> (or legacy: input.rvz output.iso)", file=sys.stderr)
        return 1
    if not args.output and not args.iso:
        print("error: pass --output <folder> and/or --iso <path>", file=sys.stderr)
        return 1

    rvz = Path(args.input)
    if not rvz.is_file():
        print(f"error: input not found: {rvz}", file=sys.stderr)
        return 1

    try:
        if args.iso:
            iso_path = Path(args.iso)
            rvz_to_iso(rvz, iso_path)
            print(f"Wrote ISO {iso_path} ({iso_path.stat().st_size:,} bytes)")
            if not args.output:
                return 0
        else:
            tmp = tempfile.NamedTemporaryFile(suffix=".iso", delete=False)
            iso_path = Path(tmp.name)
            tmp.close()
            try:
                rvz_to_iso(rvz, iso_path)
                info = extract_iso(iso_path, Path(args.output), args.expect_id,
                                   args.expect_dol.lower(), args.no_dol_check)
            finally:
                iso_path.unlink(missing_ok=True)
            print(f"RVZ_EXTRACT: disc_id={info['disc_id']} title=\"{info['title']}\" "
                  f"dol_sha256={info['dol_sha256']} iso_crc32={info['iso_crc32']} "
                  f"files={info['files']}")
            return 0
        # --iso + --output: extract from the reconstructed ISO
        info = extract_iso(iso_path, Path(args.output), args.expect_id,
                           args.expect_dol.lower(), args.no_dol_check)
        print(f"RVZ_EXTRACT: disc_id={info['disc_id']} dol_sha256={info['dol_sha256']} "
              f"iso_crc32={info['iso_crc32']} files={info['files']}")
        return 0
    except (ValueError, NotImplementedError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
