#!/usr/bin/env python3
"""Manual test: run the relinker on a real homebrew .nro you own.

The .nro must come from a free homebrew release (e.g. Checkpoint) or your
own build. No decryption is involved: homebrew .nro files are plaintext.
Only synthetic fixtures may be committed.

Usage:
    python3 test_real_nro.py --relinker <relinker> --nro <file.nro> [--out <file.elf>]
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile


def parse_nro_header(path: str) -> dict:
    with open(path, "rb") as f:
        hdr = f.read(0x80)
    assert len(hdr) == 0x80, "file smaller than NRO header"
    assert hdr[0x10:0x14] == b"NRO0", "missing NRO0 magic at 0x10"
    fields = struct.unpack_from("<IIIIIIIIIII", hdr, 0x14)
    return {
        "version": fields[0], "size": fields[1], "flags": fields[2],
        "text_mem": fields[3], "text_size": fields[4],
        "ro_mem": fields[5], "ro_size": fields[6],
        "data_mem": fields[7], "data_size": fields[8],
        "bss": fields[9],
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--relinker", required=True)
    ap.add_argument("--nro", required=True)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    info = parse_nro_header(args.nro)
    print("NRO header:")
    for k, v in info.items():
        print(f"  {k} = {v:#x} ({v})")
    assert info["version"] == 0, "unsupported NRO version"

    with open(args.nro, "rb") as f:
        nro = f.read()
    text = nro[0x80:0x80 + info["text_size"]]
    assert len(text) == info["text_size"], "truncated .text"
    # .text must look like AArch64 code (Checkpoint entry starts with STP).
    print(f"  text[0:16] = {text[:16].hex()}")

    out = args.out or os.path.join(tempfile.mkdtemp(prefix="nro_real_"),
                                   "game.out.elf")
    proc = subprocess.run([args.relinker, args.nro, out],
                          capture_output=True, text=True)
    print("--- relinker stdout ---")
    print(proc.stdout.strip())
    if proc.returncode != 0:
        print("--- relinker stderr ---")
        print(proc.stderr.strip())
        return 1

    with open(out, "rb") as f:
        elf = f.read()
    assert elf[:4] == b"\x7fELF", "bad ELF magic"
    assert struct.unpack_from("<H", elf, 16)[0] == 3, "not ET_DYN"
    assert b"/lib64/ld-linux-x86-64.so.2" in elf, "missing interp"
    # Original code must survive the wrap: sample 1 KiB windows of .text.
    hits = sum(1 for i in range(0, len(text) - 1024, 4096)
               if text[i:i + 1024] in elf)
    total = max(1, len(range(0, len(text) - 1024, 4096)))
    print(f"code passthrough: {hits}/{total} sampled windows present")
    assert hits == total, "code bytes lost in conversion"
    print(f"OK: {args.nro} -> {out} ({len(elf)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
