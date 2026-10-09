# AnySwitch Architecture

## Overview

AnySwitch ports Nintendo Switch games from ARM64 (AArch64) to x86-64 Linux/Windows by performing static binary translation.

## Core Components

### 1. Relinker (core/relinker)

The main executable that converts a decrypted Switch NSO/ELF binary into a host-native executable.

**Pipeline:**
1. **ELF parsing** (`ElfReader.cpp`) — reads the ARM64 ELF headers, program headers, dynamic segment, and relocations
2. **Code translation** (`RemillArm64Translator.cpp`) — uses [Remill](https://github.com/trailofbits/remill) to lift ARM64 instructions → LLVM IR → optimize → emit x86-64 machine code
3. **Relocation processing** (`RelinkerPipeline.cpp`) — extracts `DT_RELA` and `DT_JMPREL` relocations (NRO symbols → host dynamic symbols), resolves call sites
4. **Dynamic section rebuild** (`SysVDynamicSectionBuilder.cpp`) — rebuilds `.dynstr`, `.dynsym`, `.dynamic` with new symbol references
5. **ELF/PE patching** (`LinuxElfPatcher.cpp` / `WindowsPePatcher.cpp`) — rewrites the ELF with x86-64 code, new program headers, entry stub, and interpreter

### 2. Binary Translation (codegen)

**ARM64 → x86-64 via Remill + LLVM:**
```
ARM64 instructions → [Remill Decoder] → LLVM IR → [LLVM Passes] → x86-64 machine code
```

- Remill decodes ARM64 instructions and lifts them to LLVM IR
- LLVM's x86-64 backend handles register allocation (32 ARM GPRs → 16 x86 GPRs)
- LLVM optimization passes handle instruction combining, dead code elimination, etc.
- Supports both static binary translation (whole function) and per-block translation

### 3. System Libraries (core/libs/prx)

Host implementations of Switch system libraries compiled as `.prx` shared libraries:

| Library | Implements | Host Backend |
|---------|-----------|--------------|
| `libc.prx` | C runtime (malloc, stdio, string, etc.) | Host libc + custom guest allocator |
| `libkernel.prx` | Kernel svc calls | POSIX (pthreads, sockets, file I/O) |
| `libnx.prx` | libnx runtime (appln, hid, audio, etc.) | Host reimplementations (SDL2 for input/audio, Vulkan for GPU) |

### 4. Shader Recompiler (core/shader/recompiler)

**Maxwell GPU bytecode → SPIR-V:**
```
NVN shader binary → [MaxwellDecoder] → [GraphBuilder] → IR → 
[SsaBuilder → ConstantFolder → DeadCodeEliminator] → 
[ResourceTracker → BindingAllocator] → [SpirvEmitter] → SPIR-V
```

### 5. Symbol Resolution

Switch games use dynamic linking with symbol names from `.nro`/`.nso` files. The AnySwitch approach:
- Reimplements libnx and host libraries with the same exported symbol names
- The relinker rewrites the `.dynsym`/`.dynstr` to point to these host implementations
- No keys or firmware needed — the game just links against host implementations

## Data Flow

```
User input: decrypted_game.nso + game_assets/

┌─────────────────┐    ┌──────────────────┐
│  NSO/ELF Parser  │    │ ARM64 Code Segs  │
└────────┬─────────┘    └────────┬──────────┘
         │                       │
         ▼                       ▼
┌─────────────────┐    ┌──────────────────┐
│ Remill Lift:     │    │ Relocations      │
│ ARM64 → LLVM IR  │←──→│ (DT_RELA/JMPREL) │
│ → x86-64 machine │    │ → Host Symbols   │
└────────┬─────────┘    └────────┬──────────┘
         │                       │
         ▼                       ▼
┌─────────────────┐    ┌──────────────────┐
│  Relinker        │    │ Dynamic Section  │
│  (rebuild ELF)   │    │ (new dynsym/str) │
└────────┬─────────┘    └────────┬──────────┘
         │                       │
         ▼                       ▼
┌──────────────────────────────────────────┐
│   ELF/PE Patcher (rewrite for host OS)   │
│   - New program headers                  │
│   - Entry stub                           │
│   - Interpreter (/lib64/ld-linux...)     │
│   - RPATH ($ORIGIN/libs)                 │
└────────┬─────────────────────────────────┘
         │
         ▼
┌──────────────────────────────────────────┐
│    Output: Host-native executable        │
│    + app0/ (game assets)                 │
│    + libs/ (libnx.prx, libc.prx, etc.)   │
└──────────────────────────────────────────┘
```
