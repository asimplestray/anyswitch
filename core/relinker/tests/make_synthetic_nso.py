#!/usr/bin/env python3
"""Build synthetic NSO0 fixtures (uncompressed or LZ4-compressed).

Usage:
    python3 make_synthetic_nso.py <plain.nso> <lz4.nso>

The LZ4 encoder emits literals-only blocks, which are always valid LZ4
(raw block, no frame). A hand-made match sequence is available via
match_block_example() for decoder coverage.
"""

import struct
import sys

from make_synthetic_arm64_elf import (DEFAULT_DATA, DEFAULT_RODATA,
                                      DEFAULT_TEXT, align16)


def lz4_compress_literals(data: bytes) -> bytes:
    """Encode data as a single literals-only LZ4 block."""
    n = len(data)
    nib = 15 if n >= 15 else n
    out = bytearray([(nib << 4) | 0])
    if n >= 15:
        m = n - 15
        while m >= 255:
            out += b"\xff"
            m -= 255
        out += bytes([m])
    out += data
    return bytes(out)


def match_block_example() -> tuple[bytes, bytes]:
    """Hand-assembled LZ4 block using a match: b'AB' + match(off=2,len=4)."""
    compressed = bytes([0x20]) + b"AB" + bytes([0x02, 0x00])
    return compressed, b"ABABAB"


def build_nso(text: bytes, rodata: bytes, data: bytes, bss: int = 0,
              compress_flags: int = 0,
              raw_blocks: dict | None = None) -> bytes:
    """compress_flags: bit0=text, bit1=rodata, bit2=data (LZ4 literals).

    raw_blocks maps segment index -> already-compressed bytes, for
    hand-assembled decoder vectors (sets the flag bit automatically).
    """
    raw_blocks = raw_blocks or {}
    payloads = []
    csize = []
    flags = compress_flags
    for i, seg in enumerate((text, rodata, data)):
        if i in raw_blocks:
            comp = raw_blocks[i]
            flags |= 1 << i
        elif compress_flags & (1 << i):
            comp = lz4_compress_literals(seg)
        else:
            comp = seg
        payloads.append(comp)
        # hactool-style: uncompressed segments report csize == decomp size.
        csize.append(len(comp) if (flags & (1 << i)) else len(seg))

    text_mem, ro_mem = 0x0, align16(len(text))
    data_mem = align16(ro_mem + len(rodata))

    hdr = bytearray(0x100)
    hdr[0:4] = b"NSO0"
    struct.pack_into("<I", hdr, 0x04, 0)  # version
    struct.pack_into("<I", hdr, 0x0C, flags)

    off = 0x100
    seg_offs = []
    for p in payloads:
        seg_offs.append(off)
        off += len(p)

    struct.pack_into("<III", hdr, 0x10, seg_offs[0], text_mem, len(text))
    struct.pack_into("<III", hdr, 0x20, seg_offs[1], ro_mem, len(rodata))
    struct.pack_into("<III", hdr, 0x30, seg_offs[2], data_mem, len(data))
    struct.pack_into("<I", hdr, 0x3C, bss)
    struct.pack_into("<III", hdr, 0x60, csize[0], csize[1], csize[2])

    out = bytes(hdr)
    for p in payloads:
        out += p
    return out


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <plain.nso> <lz4.nso>", file=sys.stderr)
        return 1
    with open(sys.argv[1], "wb") as f:
        f.write(build_nso(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA))
    with open(sys.argv[2], "wb") as f:
        f.write(build_nso(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA,
                          compress_flags=0b111))
    print(f"Wrote {sys.argv[1]} and {sys.argv[2]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
