# AGENTS.md — AnySwitch Project Guide

## Build Commands

```bash
# Configure (Linux)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# Build everything
cmake --build build --parallel

# Build just the relinker
cmake --build build --target relinker

# Build system libraries
cmake --build build --target libnx libc libkernel

# Run tests
ctest --test-dir build --output-on-failure

# Lint/typecheck (if clangd available)
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Architecture Overview

The project has 4 main parts:
1. **core/relinker/** — The main converter (ARM64 ELF → x86-64 ELF/PE)
2. **core/libs/** — Host reimplementations of Switch system libraries (.prx)
3. **core/shader/recompiler/** — Maxwell GPU shader → SPIR-V compiler
4. **3rdparty/** — Remill (ARM64→x86-64), glslang, SPIRV-Tools

## Code Style

- C++20, GCC 12+ / Clang 15+ / MSVC 2022
- Use `std::unique_ptr` for ownership, `std::shared_ptr` for shared ownership
- Prefer `override`, `final`, `constexpr`, `noexcept`
- No trailing whitespace; files end with a newline
- Use `#pragma once` or include guards consistently
- 4-space indentation

## Testing

- Smoke tests live in `Testing/` (run without games, keys, or firmware)
- C++ unit tests: `core/libs/tests/`
- New features must include tests; run `ctest` to verify

## Key Files

- `core/relinker/main.cpp` — entry point
- `core/relinker/relinker/src/pipeline/RelinkerPipeline.cpp` — core translation pipeline
- `core/relinker/codegen/src/RemillArm64Translator.cpp` — ARM64→x86-64 via Remill
- `core/relinker/elfpatcher/src/linux/LinuxElfPatcher.cpp` — ELF rewriting
- `core/shader/recompiler/Recompiler.cpp` — shader compiler entry point
