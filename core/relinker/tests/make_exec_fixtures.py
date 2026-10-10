#!/usr/bin/env python3
"""Hand-built .nro fixtures for the execution harness.

We have no Switch cross-compiler, so guest programs are assembled by hand.
Each builder emits a valid NRO0 whose .text is a known instruction sequence.

Usage:
    python3 make_exec_fixtures.py <outdir>

Produces:
    ret42.nro    MOV X0,#42 ; RET           -> X0 == 42
    add.nro      MOV X0,#7 ; ADD X0,X0,#35   -> X0 == 42
    branch.nro   CMP/CBZ/CBNZ path          -> X0 == 1 or 2
    loop.nro     counted loop               -> X0 == 5
"""

import os
import struct
import sys

from make_synthetic_arm64_elf import align16


def movz(reg: int, imm: int) -> bytes:
    return struct.pack("<I", (1 << 31) | (0xA5 << 23) | (imm << 5) | reg)


def load_imm32(reg: int, value: int) -> bytes:
    """MOVZ/MOVK pair that materialises a 32-bit address, so fixtures do not
    have to reason about page offsets by hand."""
    out = struct.pack("<I", (1 << 31) | (0xA5 << 23) | ((value & 0xFFFF) << 5) | reg)
    high = (value >> 16) & 0xFFFF
    if high:
        out += struct.pack("<I", (1 << 31) | (0xE5 << 23) | (1 << 21) |
                           (high << 5) | reg)
    return out


def build_nro_text(text: bytes, rodata: bytes, data: bytes, bss: int = 0) -> bytes:
    """Minimal NRO0: 0x80 header, contiguous text/rodata/data."""
    text_mem, ro_mem = 0x0, align16(len(text))
    data_mem = align16(ro_mem + len(rodata))
    hdr = bytearray(0x80)
    hdr[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", hdr, 0x20, text_mem)
    struct.pack_into("<I", hdr, 0x24, len(text))
    struct.pack_into("<I", hdr, 0x28, ro_mem)
    struct.pack_into("<I", hdr, 0x2C, len(rodata))
    struct.pack_into("<I", hdr, 0x30, data_mem)
    struct.pack_into("<I", hdr, 0x34, len(data))
    struct.pack_into("<I", hdr, 0x38, bss)
    out = bytes(hdr) + text + rodata + data
    struct.pack_into("<I", hdr, 0x18, len(text) + len(rodata) + len(data))
    return bytes(hdr) + text + rodata + data


def nop() -> bytes:
    return bytes.fromhex("1f2003d5")


def ret() -> bytes:
    return bytes.fromhex("c0035fd6")


def mov_imm(reg: int, imm: int) -> bytes:
    """MOV Xd, #imm for small immediates (one instruction when possible)."""
    assert 0 <= imm < 0x10000, "keep immediates small for single-instruction MOV"
    # MOVZ Xd, #imm : bits 30-23 = 0xA5
    word = (1 << 31) | (0xA5 << 23) | (imm << 5) | reg
    return struct.pack("<I", word)


def add_imm(rd: int, rn: int, imm: int) -> bytes:
    """ADD Xd, Xn, #imm (opcode 0x91 lives in bits 31-24; bits 30-23 = 0x22)"""
    word = (1 << 31) | (0x22 << 23) | (imm << 10) | (rn << 5) | rd
    return struct.pack("<I", word)


def sub_imm(rd: int, rn: int, imm: int) -> bytes:
    """SUB Xd, Xn, #imm (bits 30-23 = 0xA2)"""
    word = (1 << 31) | (0xA2 << 23) | (imm << 10) | (rn << 5) | rd
    return struct.pack("<I", word)


def cbz(reg: int, off_instrs: int) -> bytes:
    word = (0x34 << 24) | ((off_instrs & 0x7FFFF) << 5) | reg
    return struct.pack("<I", word)


def cbz_back(reg: int, back: int) -> bytes:
    return cbz(reg, -back)


def cbz_neg(reg: int, back: int) -> bytes:
    word = (0x34 << 24) | (((-back) & 0x7FFFF) << 5) | reg
    return struct.pack("<I", word)


def b(off_instrs: int) -> bytes:
    word = (0x14 << 24) | (off_instrs & 0x3FFFFFF)
    return struct.pack("<I", word)


FIXTURES = {
    # X0 == 42
    "ret42": mov_imm(0, 42) + ret(),
    # X0 == (7 + 35) == 42
    "add": mov_imm(0, 7) + add_imm(0, 0, 35) + ret(),
    # X0 == 12 - 5 == 7
    "sub": mov_imm(0, 12) + sub_imm(0, 0, 5) + ret(),
    # two blocks joined by a taken conditional branch:
    #   X0 = 0 ; CBZ X0 -> L1 ; (skipped) MOV X0,#99 ; L1: MOV X0,#1 ; RET
    "branch_taken": mov_imm(0, 0) + cbz(0, 2) + mov_imm(0, 99) + mov_imm(0, 1) + ret(),
    # loop: X0 = 0 ; L: X0 += 1 ; CMP-ish via SUBS+CBZ ; X0 == 5
    #   X0=0; L: X0+=1; X0==5? no: B L
    "loop": (mov_imm(0, 0)
             + add_imm(0, 0, 1)
             + mov_imm(1, 5)
             + sub_imm(1, 1, 0)  # placeholder, replaced below
             + ret()),
}


def build_loop() -> bytes:
    # X0 = 0
    # L:  X0 += 1
    #     X1 = X0 - 5      (SUBS sets flags; we only need the value)
    #     CBZ X1, done
    #     B L
    # done: RET
    #
    # Layout in instructions from L (index 1):
    #   1: ADD X0, X0, #1
    #   2: SUB X1, X0, #5
    #   3: CBZ X1, +2      -> index 5 (done)
    #   4: B -3            -> index 1 (L)
    #   5: RET
    text = mov_imm(0, 0)
    text += add_imm(0, 0, 1)
    text += sub_imm(1, 0, 5)
    text += cbz(1, 2)
    text += b(-3)
    text += ret()
    return text


def main() -> int:
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    def svc(n: int) -> bytes:
        # SVC #n  : bits 31-24 = 0xD4, immediate in bits 20-5, LL = 0b00001
        return struct.pack("<I", (0xD4 << 24) | ((n & 0xFFFF) << 5) | 0b00001)

    programs = {
        "ret42": mov_imm(0, 42) + ret(),
        # Prints its own rodata through svc #0x27, then exits. Proves the
        # guest-to-host seam with data the guest reads itself.
        "syscall": load_imm32(0, 0x5080 >> 4) + movz(1, 22) + svc(0x27) + svc(0x07),
        "add": mov_imm(0, 7) + add_imm(0, 0, 35) + ret(),
        "sub": mov_imm(0, 12) + sub_imm(0, 0, 5) + ret(),
        "branch_taken": mov_imm(0, 0) + cbz(0, 2) + mov_imm(0, 99) + mov_imm(0, 1) + ret(),
        "branch_nottaken": mov_imm(0, 1) + cbz(0, 2) + mov_imm(0, 77) + ret(),
        "loop": build_loop(),
    }

    # The printing fixture's address depends on where build_nro_text places
    # rodata, so build it once with a placeholder, read the real offset back out
    # of the header, then rebuild the fixture with the correct immediate.
    rodata_overrides = {}
    for _ in range(2):
        prober = build_nro_text(programs["syscall"], b"S", b"\x00" * 16)
        ro_mem = struct.unpack_from("<I", prober, 0x28)[0]
        if ro_mem:
            programs["syscall"] = (load_imm32(0, ro_mem) + movz(1, 22) +
                                   struct.pack("<I", (0xD4 << 24) | (0x27 << 5) | 1) +
                                   struct.pack("<I", (0xD4 << 24) | (7 << 5) | 1))
            rodata_overrides["syscall"] = b"ANYSWITCH GUEST OUTPUT\x00"
            break

    for name, code in programs.items():
        msg = rodata_overrides.get(name, b"RODATA")
        with open(os.path.join(outdir, f"{name}.nro"), "wb") as f:
            f.write(build_nro_text(code, msg, b"\x00" * 16))


    with open(os.path.join(outdir, "expected.txt"), "w") as f:
        for name, x0 in (("ret42", 42), ("add", 42), ("sub", 7),
                         ("branch_taken", 1), ("branch_nottaken", 77), ("loop", 5),
                         # ServiceSyscall currently returns 0 in X0.
                         ("syscall", 0)):
            f.write(f"{name} {x0}\n")
    print("wrote expected.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
