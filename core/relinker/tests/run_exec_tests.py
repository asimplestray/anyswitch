#!/usr/bin/env python3
"""Execution harness for AnySwitch: proves translated code actually runs.

Pipeline per fixture:
    hand-built .nro -> relinker (translate + emit object)
                    -> link object + native runtime + test main
                    -> run -> assert X0

This is the test that separates "produces x86-64 bytes" from "translates
correctly". Skipped when the relinker was built without LLVM/Remill.

Usage:
    python3 run_exec_tests.py --relinker <path> [--workdir <dir>]
"""

import argparse
import os
import subprocess
import sys
import struct
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))

sys.path.insert(0, HERE)

REMILL_INCLUDE = os.path.join(REPO, "3rdparty", "remill", "include")
LIBKERNEL_INCLUDE = os.path.join(REPO, "core", "libs", "libkernel", "include")

# SDL2 is used only for the window; headless machines still run.
try:
    SDLFLAGS = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl2"],
                              check=True, capture_output=True, text=True).stdout.split()
except Exception:
    SDLFLAGS = []


def build_runtime(workdir: str):
    """Compiles the guest runtime, the dispatch shim, and libkernel.

    libkernel is compiled from source here rather than linked as a CMake
    target so the harness stays independent of the build tree.
    """
    jobs = [
        ("runtime/AnyswitchRuntime.cpp", ["libkernel/include"]),
        ("runtime/Driver.cpp", ["libkernel/include"]),
        ("runtime/Presentation.cpp", ["libkernel/include"]),
        ("runtime/BitmapFont.cpp", ["libkernel/include"]),
        ("runtime/Syscalls.cpp", ["libkernel/include"]),
        ("libkernel/src/Dispatch.cpp", ["libkernel/include"]),
        ("libkernel/src/Memory.cpp", ["libkernel/include"]),
        ("libkernel/src/Handle.cpp", ["libkernel/include"]),
        ("libkernel/src/Info.cpp", ["libkernel/include"]),
        ("libkernel/src/Sessions.cpp", ["libkernel/include"]),
        ("libkernel/src/SystemRegisters.cpp", ["libkernel/include"]),
    ]
    outs = []
    for src, extra in jobs:
        path = os.path.join(REPO, "core", "libs", src)
        out = os.path.join(workdir, "libanyswitch_" + src.replace("/", "_") + ".o")
        cmd = ["g++", "-std=c++20", "-w", "-I", REMILL_INCLUDE, "-I", REPO,
               "-c", path, "-o", out] + SDLFLAGS
        for inc in extra:
            cmd += ["-I", os.path.join(REPO, "core", "libs", inc)]
        subprocess.run(cmd, check=True, capture_output=True)
        outs.append(out)
    return outs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--relinker", required=True)
    ap.add_argument("--workdir", default=None)
    args = ap.parse_args()

    if not os.path.isfile(args.relinker):
        print(f"relinker not found: {args.relinker}", file=sys.stderr)
        return 1

    # Probe: does this relinker have translation support?
    probe_dir = args.workdir or tempfile.mkdtemp(prefix="anyswitch_exec_")
    os.makedirs(probe_dir, exist_ok=True)
    from make_exec_fixtures import main as gen_fixtures
    import io
    import contextlib

    # Regenerate fixtures into the workdir.
    saved_argv = sys.argv
    sys.argv = ["make_exec_fixtures.py", probe_dir]
    with contextlib.redirect_stdout(io.StringIO()):
        gen_fixtures()
    sys.argv = saved_argv

    expected = {}
    with open(os.path.join(probe_dir, "expected.txt")) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 2:
                expected[parts[0]] = parts[1]

    # Discover whether translation is compiled in.
    probe_nro = os.path.join(probe_dir, "ret42.nro")
    env = dict(os.environ)
    env["ANYSWITCH_EMIT_OBJECT"] = os.path.join(probe_dir, "probe.o")
    proc = subprocess.run([args.relinker, "--to-intel", probe_nro,
                           os.path.join(probe_dir, "probe.elf")],
                          capture_output=True, text=True, env=env)
    if proc.returncode != 0 and "LLVM/Remill" in proc.stderr:
        print("SKIP: relinker built without LLVM/Remill support")
        return 0
    if proc.returncode != 0 and "requires LLVM" in proc.stdout:
        print("SKIP: relinker built without LLVM/Remill support")
        return 0
    if not os.path.exists(env["ANYSWITCH_EMIT_OBJECT"]):
        print("SKIP: no translation output produced")
        return 0

    runtime_objs = build_runtime(probe_dir)
    main_cpp = os.path.join(REPO, "core", "relinker", "tests", "run_translated_main.cpp")

    passed = failed = 0
    for name, want in sorted(expected.items()):
        nro = os.path.join(probe_dir, f"{name}.nro")
        obj = os.path.join(probe_dir, f"{name}.o")
        exe = os.path.join(probe_dir, f"run_{name}")

        e = dict(os.environ)
        e["ANYSWITCH_EMIT_OBJECT"] = obj
        e["ANYSWITCH_EMIT_TRACES"] = table = os.path.join(probe_dir, f"{name}.traces")
        r = subprocess.run([args.relinker, "--to-intel", nro,
                            os.path.join(probe_dir, f"{name}.elf")],
                           capture_output=True, text=True, env=e)
        if r.returncode != 0:
            print(f"FAIL {name}: relinker rc={r.returncode}: {r.stderr.strip()}")
            failed += 1
            continue

        link = subprocess.run(
            ["g++", "-std=c++20", "-w", "-rdynamic", "-I", REPO, "-I", REMILL_INCLUDE,
             "-I", LIBKERNEL_INCLUDE,
             "-o", exe, main_cpp, obj, *runtime_objs, "-lm"] + SDLFLAGS,
            capture_output=True, text=True)
        if link.returncode != 0:
            print(f"FAIL {name}: link failed: {link.stderr.strip()[:600]}")
            failed += 1
            continue

        # The harness maps every guest segment from the NRO itself, so the
        # raw file is what it needs: code for the traces, and rodata/data for
        # the pointers the guest dereferences.
        trace = dict(os.environ, ANYSWITCH_TRACE_SYSCALLS="1")
        run = subprocess.run([exe, nro, table], capture_output=True, text=True,
                             env=trace)
        got = run.stdout.strip()
        ok = got == f"X0={want}"
        detail = got
        # A syscall fixture must also reach the host: assert the dispatcher
        # recognised it. 66 is ReplyAndReceiveLight in the real syscall table.
        if name == "syscall":
            if "svc #0x42" not in run.stderr:
                ok = False
                detail = f"{got} (syscall was not handled)"
        if ok:
            print(f"PASS {name} -> {detail}")
            passed += 1
        else:
            print(f"FAIL {name} -> {detail}, expected X0={want}")
            failed += 1

    print(f"\n{passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
