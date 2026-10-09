#!/usr/bin/env python3
"""End-to-end relinker test: synthetic ELF + NSO fixtures -> host ELF.

Generates fixtures in a temp dir, runs the relinker on each, and validates
the outputs structurally (ELF magic, ET_DYN, interp, marker passthrough).
Covers: plain ELF input, uncompressed NSO, LZ4-compressed NSO (literals),
LZ4 match-sequence decoding, and truncated-input rejection.

Usage:
    python3 test_end_to_end.py --relinker <path/to/relinker>
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from make_synthetic_arm64_elf import (DEFAULT_DATA, DEFAULT_RODATA,
                                      DEFAULT_TEXT, build_elf)
from make_synthetic_nso import build_nro, build_nso, match_block_example

INTERP = b"/lib64/ld-linux-x86-64.so.2"


def run_relinker(relinker: str, src: bytes, name: str, workdir: str) -> bytes:
    inp = os.path.join(workdir, name)
    outp = os.path.join(workdir, name + ".out.elf")
    with open(inp, "wb") as f:
        f.write(src)
    proc = subprocess.run([relinker, inp, outp], capture_output=True, text=True)
    assert proc.returncode == 0, (
        f"{name}: relinker failed rc={proc.returncode}\n"
        f"stdout: {proc.stdout}\nstderr: {proc.stderr}")
    with open(outp, "rb") as f:
        return f.read()


def check_host_elf(out: bytes, name: str, marker: bytes) -> None:
    assert out[:4] == b"\x7fELF", f"{name}: bad ELF magic"
    assert out[4] == 2, f"{name}: not ELF64"
    etype = struct.unpack_from("<H", out, 16)[0]
    assert etype == 3, f"{name}: e_type={etype}, expected ET_DYN(3)"
    assert INTERP in out, f"{name}: missing linux interp"
    assert marker in out, f"{name}: marker payload lost in output"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--relinker", required=True)
    args = ap.parse_args()
    if not os.path.isfile(args.relinker):
        print(f"relinker not found: {args.relinker}", file=sys.stderr)
        return 1

    workdir = tempfile.mkdtemp(prefix="anyswitch_e2e_")

    # 1. Plain synthetic ARM64 ELF.
    out = run_relinker(args.relinker,
                       build_elf(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA),
                       "synth.elf", workdir)
    check_host_elf(out, "synth.elf", DEFAULT_RODATA)
    print("PASS plain ELF -> host ELF")

    # 2. Uncompressed NSO wrapping the same segments.
    out = run_relinker(args.relinker,
                       build_nso(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA),
                       "plain.nso", workdir)
    check_host_elf(out, "plain.nso", DEFAULT_RODATA)
    print("PASS uncompressed NSO -> host ELF")

    # 3. Fully LZ4-compressed NSO (literals-only blocks).
    out = run_relinker(args.relinker,
                       build_nso(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA,
                                 compress_flags=0b111),
                       "lz4.nso", workdir)
    check_host_elf(out, "lz4.nso", DEFAULT_RODATA)
    print("PASS LZ4 NSO -> host ELF")

    # 4. Hand-made LZ4 match sequence: text decodes to b"ABABAB".
    comp, expected = match_block_example()
    out = run_relinker(args.relinker,
                       build_nso(expected, b"", b"", raw_blocks={0: comp}),
                       "match.nso", workdir)
    check_host_elf(out, "match.nso", expected)
    print("PASS LZ4 match decoding")

    # 5. Synthetic NRO (homebrew layout).
    out = run_relinker(args.relinker,
                       build_nro(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA),
                       "synth.nro", workdir)
    check_host_elf(out, "synth.nro", DEFAULT_RODATA)
    print("PASS synthetic NRO -> host ELF")

    # 6. Truncated NSO must be rejected, not crash.
    bad = os.path.join(workdir, "bad.nso")
    with open(bad, "wb") as f:
        f.write(b"NSO0" + b"\x00" * 10)
    proc = subprocess.run([args.relinker, bad, bad + ".out"],
                          capture_output=True, text=True)
    assert proc.returncode != 0, "truncated NSO was unexpectedly accepted"
    assert "FAIL" in proc.stderr, "expected FAIL diagnostics on stderr"
    print("PASS truncated NSO rejected")

    # 7. Real translation when Remill+LLVM are available; otherwise the
    # --to-intel flag fails fast with a clear message (skip, don't fail).
    elf_in = os.path.join(workdir, "intel.elf")
    with open(elf_in, "wb") as f:
        f.write(build_elf(DEFAULT_TEXT, DEFAULT_RODATA, DEFAULT_DATA))
    proc = subprocess.run([args.relinker, "--to-intel", elf_in,
                           elf_in + ".out.elf"],
                          capture_output=True, text=True)
    if proc.returncode != 0 and "LLVM/Remill" in proc.stderr:
        print("SKIP --to-intel (Remill/LLVM not built)")
    else:
        assert proc.returncode == 0, f"--to-intel failed:\n{proc.stderr}"
        assert "Translated code: 0 bytes" not in proc.stdout, \
            "expected nonzero translation"
        print("PASS --to-intel translation")

    print(f"ALL E2E TESTS PASSED ({workdir})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
