# Progress log

A running record of what has actually been verified, and what stands between
here and a homebrew that produces visible output.

## Verified

| Milestone | Evidence |
|---|---|
| ARM64 → x86-64 translation | 13,678 instructions from an 86 KB homebrew `.text` |
| Control flow | multi-block traces with taken/untaken conditional branches and backward branches |
| Execution with correct results | 7 hand-built fixtures assert the guest's X0 |
| Guest syscalls reach the host | `SVC` is dispatched and the vector printed |
| Two homebrew binaries execute | `rainbow.nro` and a 376 KB controller tester both run their startup, then stop at the same wall |

`rainbow.nro` is not committed (homebrew binaries are gitignored). Fetch it with:

```sh
curl -LO https://github.com/bward-dev1/rainbow-nro/releases/download/rainbow-v1/rainbow.nro
```

## Where the wall is now

The remaining failure is control reaching zero-word regions inside `.text`
(0x48cc and friends), which means the guest is branching into data. That is
consistent with indirect-call targets never being resolved: the relinker
records each BLR/BR site and emits a fixup, but the fixup's target is still
zero. Making those resolve is the next blocker.

## What the earlier run showed

Running it prints the syscalls it issues, in order:

```
svc #1   SetHeapSize          heap setup
svc #2   SetMemoryPermission   memory setup
svc #3   SetMemoryAttribute
svc #41  WaitSynchronization   the main loop, repeatedly
svc #38  CloseHandle
svc #6   ExitProcess
```

Two kinds of gap appear alongside that:

1. **Unlifted instructions.** 19 distinct PCs out of 13,678 discovered — roughly
   99.9% coverage of what discovery reached. These are forms Remill's AArch64
   semantics do not implement (`MRS`/`MSR` of system registers, some SIMD
   loads/stores) plus data that a straight-line scan read as code.
2. **The runtime only ran one basic block.** A lifted trace is one block: it
   runs, updates the guest's PC, and returns. Nothing read that PC back and ran
   the next trace, so "it executes" previously meant exactly one block. The
   runtime now has a driver that chains them, fed by a trace table the
   relinker emits (ANYSWITCH_EMIT_TRACES). Two homebrew binaries now run
   thousands of traces instead of one.

3. **Every TLS access failed.** Remill's AArch64 semantics do not implement
   `MRS`/`MSR`, and TPIDRRO_EL0 is how both binaries reach their thread-local
   storage - 172 and 188 sites respectively. The runtime now decodes and
   performs these moves (TPIDRRO_EL0, TPIDR_EL0, CNTPCT_EL0, MIDR_EL1), which
   removed the most frequent failure in the whole run.

4. **Syscalls were stubs.** Now implemented against the SwitchBrew table:
   30+ handlers, arguments read from the guest's X0-X5, results written back. A
   heap is allocated inside the guest address space rather than at a fabricated
   base (returning a base outside the mapping made every allocation silently
   disappear). Diagnostics are behind `ANYSWITCH_TRACE_SYSCALLS`.

   With that, `rainbow.nro` runs this far:

   ```
   SetMemoryPermission, SetMemoryAttribute
   GetInfo x10
   SetHeapSize
   svcBreak (Panic)          <- stops here
   QueryMemory x2
   ConnectToNamedPort
   svcBreak (Panic) x more
   ```

   A second binary (a 376 KB controller tester) stops at exactly the same
   place, so this is a common libnx-init gap rather than anything app-specific.

   Adding the link register to the svcBreak diagnostic put its LR at 0x1b9ac,
   which is a zero word inside .text - not an instruction. So the guest is
   reaching an address that holds data. Either it really is calling into a data
   table, or X30 has been clobbered by the time we read it. Both point at the
   same next investigation, not at a missing syscall.

## What visible output still needs

1. **Real syscalls.** Memory mapping (`svc #1`/`#2`/`#3`), `QueryMemory`, and the
   synchronization primitives the main loop depends on. Without these the guest
   runs against a fiction.
2. **libnx console.** `consoleInit` / `printf` / `consoleUpdate` are stubs today.
3. **A presentation path.** libnx's console rasterises glyphs into a framebuffer;
   nothing in this project puts a framebuffer on screen yet. This homebrew also
   talks to `/dev/nvhost-ctrl` and `/dev/nvmap`, i.e. the GPU, so a text-only
   binary would be the cheaper first target.
