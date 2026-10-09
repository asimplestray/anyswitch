#!/usr/bin/env python3
"""Build a minimal synthetic ARM64 ELF64 fixture for the relinker pipeline.

Layout mirrors NsoReader::ConvertToElf output: EHDR + 4 PHDRs
(PT_LOAD text/rodata/data + PT_DYNAMIC with a single DT_NULL entry).

Usage:
    python3 make_synthetic_arm64_elf.py <output.elf>
"""

import struct
import sys

EM_AARCH64 = 183
ET_DYN = 3
PT_LOAD = 1
PT_DYNAMIC = 2
PF_X, PF_W, PF_R = 1, 2, 4

# ret; nop; nop; nop (valid AArch64, safe for future translator stages)
DEFAULT_TEXT = bytes.fromhex("c0035fd61f2003d51f2003d51f2003d5")
DEFAULT_RODATA = b"ANYSWITCH_FIXTURE_v1\x00"
DEFAULT_DATA = bytes.fromhex("efbeadde78563412")


def align16(v: int) -> int:
    return (v + 15) & ~15


def build_elf(text: bytes, rodata: bytes, data: bytes, bss: int = 0) -> bytes:
    phnum = 4
    ehsize, phentsize = 64, 56
    header_end = ehsize + phnum * phentsize
    text_off = header_end
    ro_off = align16(text_off + len(text))
    data_off = align16(ro_off + len(rodata))
    dyn_off = data_off + len(data)
    dyn_size = 16  # one Elf64_Dyn: DT_NULL
    data_filesz = len(data) + dyn_size

    # Vaddrs mirror NSO-style relative layout (internal consistency only).
    text_v = 0x0
    ro_v = align16(text_v + len(text))
    data_v = align16(ro_v + len(rodata))
    dyn_v = data_v + len(data)

    out = bytearray()
    out += b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)  # ELF64 LE, v1, SysV
    out += struct.pack("<HHIQQQIHHHHHH", ET_DYN, EM_AARCH64, 1, text_v,
                       ehsize, 0, 0, ehsize, phentsize, phnum, 0, 0, 0)

    def phdr(ptype, flags, off, vaddr, filesz, memsz):
        return struct.pack("<IIQQQQQQ", ptype, flags, off, vaddr, vaddr,
                           filesz, memsz, 0x1000)

    out += phdr(PT_LOAD, PF_R | PF_X, text_off, text_v, len(text), len(text))
    out += phdr(PT_LOAD, PF_R, ro_off, ro_v, len(rodata), len(rodata))
    out += phdr(PT_LOAD, PF_R | PF_W, data_off, data_v, data_filesz,
                data_filesz + bss)
    out += phdr(PT_DYNAMIC, PF_R | PF_W, dyn_off, dyn_v, dyn_size, dyn_size)

    while len(out) < text_off:
        out += b"\x00"
    out += text
    while len(out) < ro_off:
        out += b"\x00"
    out += rodata
    while len(out) < data_off:
        out += b"\x00"
    out += data
    out += struct.pack("<qQ", 0, 0)  # DT_NULL
    return bytes(out)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <output.elf>", file=sys.stderr)
        return 1
    with open(sys.argv[1], "wb") as f:
        f.write(build_elf(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA))
    print(f"Wrote {sys.argv[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
