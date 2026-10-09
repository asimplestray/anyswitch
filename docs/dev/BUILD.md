# AnySwitch Development Guide

## Prerequisites

- x86-64 host machine
- CMake 3.24+
- C++20 compiler (GCC 12+, Clang 15+, MSVC 2022 17.3+)
- LLVM 15+ (for Remill integration)
- Git

## Build

```bash
# Clone (submodules are optional and stay empty unless initialized)
git clone https://github.com/anyswitch/anyswitch.git
cd anyswitch

# Only if you need ARM64 translation or shaders:
#   git submodule update --init --depth 1
# See 3rdparty/README.md. LLVM itself is never vendored:
#   sudo apt install llvm-dev
#   cmake -S . -B build -DANYSWITCH_ENABLE_LLVM=ON -DANYSWITCH_USE_SYSTEM_REMILL=ON

# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DANYSWITCH_ENABLE_SPIRV_TOOLS=ON

# Build everything
cmake --build build --parallel

# Build just the libraries
cmake --build build --target libnx libc libkernel --parallel

# Run tests
ctest --test-dir build --output-on-failure
```

End-to-end coverage (`Testing/relinker_end_to_end`) builds synthetic
fixtures at test time — no games, keys, or firmware needed:
`core/relinker/tests/make_synthetic_arm64_elf.py`,
`make_synthetic_nso.py` (plain + LZ4), and `test_end_to_end.py`
drive the relinker over ELF, uncompressed NSO, LZ4 NSO, an LZ4
match-vector, and a truncated-NSO rejection case.

## Architecture

```
AnySwitch/
├── CMakeLists.txt              # Root build (requires LLVM + Remill)
├── core/
│   ├── relinker/             # Main executable (ARM64→x86-64 translation + patching)
│   │   ├── codegen/          # Remill integration for ARM64→x86-64
│   │   ├── domain/           # Shared types and exceptions
│   │   ├── elfpatcher/       # ELF/PE binary patching (Linux + Windows)
│   │   ├── io/               # File I/O utilities
│   │   ├── relinker/         # ELF parsing + relinking pipeline
│   │   ├── cli/              # Command-line argument parsing
│   │   └── main.cpp          # Entry point
│   ├── libs/
│   │   ├── prx/              # Host system library implementations (.prx files)
│   │   │   ├── libc/         # Host libc compatibility
│   │   │   ├── libkernel/    # Kernel services (svc calls → POSIX)
│   │   │   ├── libnx/        # Nintendo libnx runtime (open source)
│   │   │   └── ...           # Other system libraries
│   │   └── SwitchTypes.hpp   # Switch-specific type definitions (SceTypes.hpp is a deprecated shim)
│   └── shader/
│       └── recompiler/       # Maxwell GPU shader → SPIR-V
└── 3rdparty/
    ├── remill/               # ARM64 → x86-64 translator (submodule)
    └── glslang/              # GLSL/SPIR-V compiler (submodule)
```

## How to Port a Switch Game

```bash
# 1. Decrypt your own game (user responsibility)
# Use hactool with your keyset to unpack .nsp/.xci → main.nso

# 2. Run the relinker
./build/core/relinker/relinker \
    --input game.nso \
    --output game.elf \
    --rpath '$ORIGIN/libs'

# 3. Copy built libraries
cp build/core/libs/libs/*.prx game_output/libs/

# 4. Copy game assets
cp -r game_assets/ game_output/app0/

# 5. Run!
./game_output/game.elf
```

## Legal

This project is for **interoperability and preservation only**. It:
- Does not include, distribute, or require copyrighted firmware
- Does not include cryptographic keys
- Requires users to decrypt their own legally-owned games
- Implements all system libraries from scratch using publicly documented interfaces

Licensed under the MIT License. See LICENSE.
