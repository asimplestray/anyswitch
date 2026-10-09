# AnySwitch

Static binary translation for Nintendo Switch games (ARM64) to Linux and Windows (x86-64). No emulation, no separate runtime process: the relinker translates executable code and rewrites the binary to link against host reimplementations of system libraries.

> **Status: Alpha.** The ELF pipeline, host libraries, and shader stub build today. ARM64→x86-64 codegen requires LLVM + Remill; Windows PE output and the Maxwell shader compiler are still stubs. See [Roadmap](#roadmap).

## How it works

```
decrypted main.nso (ARM64)
  → ElfReader (headers, PT_DYNAMIC, DT_RELA / DT_JMPREL)
  → Remill + LLVM (ARM64 → x86-64, optional)
  → SysVDynamicSectionBuilder (.dynsym / .dynstr / .dynamic)
  → LinuxElfPatcher (new program headers, entry stub, RPATH)
  → host-native game.elf + libs/*.prx + app0/ assets
```

| Component | Input | Output |
|---|---|---|
| `core/relinker/` | ARM64 NSO/ELF | x86-64 ELF (Linux today, PE stubbed) |
| `core/libs/prx/libc`, `libkernel`, `libnx` | Switch symbols | Host `.prx` shared libraries |
| `core/shader/recompiler/` | Maxwell (Tegra X1) bytecode | SPIR-V for Vulkan (stub) |

Switch imports resolve by **symbol name** (unlike hash-based NIDs on other platforms), so the relinker rewrites `.dynsym`/`.dynstr` to point at the host `.prx` implementations.

## Quick start

Prerequisites (Ubuntu):

```sh
sudo apt install cmake g++ ninja-build
# Optional, enables ARM64→x86-64 translation:
sudo apt install llvm-dev
```

Build:

```sh
git clone https://github.com/anyswitch/anyswitch.git
cd anyswitch
# Optional: translation/shader deps stay empty unless you run:
#   git submodule update --init --depth 1
# (see 3rdparty/README.md; LLVM is a system package, never vendored)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build targets:

```sh
cmake --build build --target relinker        # main converter
cmake --build build --target libs            # libc.prx + libkernel.prx + libnx.prx
cmake --build build --target libnx libc libkernel
```

Convert a game you decrypted yourself:

```sh
./build/core/relinker/relinker \
  --input game/exefs/main.nso \
  --output converted_game/game.elf \
  --rpath '$ORIGIN/libs'
cp build/core/libs/prx/*/*.prx converted_game/libs/ 2>/dev/null || true
cp -r game/romfs converted_game/app0/
./converted_game/game.elf
```

Full walkthrough: [docs/user/USAGE.md](docs/user/USAGE.md). Developer build notes: [docs/dev/BUILD.md](docs/dev/BUILD.md). Architecture: [docs/dev/Architecture.md](docs/dev/Architecture.md).

## Optional dependencies

| Feature | Requirement | CMake flag |
|---|---|---|
| ARM64 → x86-64 translation | LLVM + Remill | default when found; `-DANYSWITCH_ENABLE_LLVM=ON`, `-DANYSWITCH_USE_SYSTEM_REMILL=ON` for system Remill |
| Shader recompiler | glslang | `-DANYSWITCH_BUILD_SHADER_RECOMPILER=ON` |
| SPIR-V tools | SPIRV-Tools | `-DANYSWITCH_ENABLE_SPIRV_TOOLS=ON` |

Without LLVM/Remill the project still configures and builds; codegen is disabled with a status message. See [3rdparty/README.md](3rdparty/README.md).

## Roadmap

- [x] NSO0 loader (uncompressed + LZ4) with synthetic ELF/NSO e2e fixtures
- [x] NRO0 loader (homebrew) validated on real Checkpoint.nro (5 MB)
- [x] ARM64→x86-64 lifting via Remill+LLVM (recursive descent, O2,
  object extract; Checkpoint.nro: 110,768 instrs → 6.8 MB x86-64 in ~70 s)
- [x] Splice translated code into the output ELF (patcher admits the code
  blob as an extra LOAD segment and retargets the entry stub to it)
- [x] Parse NSO/NRO `.dynstr`/`.dynsym` to resolve import names
- [x] Indirect-call fixups (BLR/BR) recorded and emitted as trampolines
- [ ] Dispatch trampolines through PLT/GOT instead of direct stubs
- [ ] Link-time symbol resolution for real dynamic linking
- [ ] Windows PE backend (`WindowsElfPatcher.cpp` stub)
- [ ] Maxwell decoder → IR → SPIR-V stages (`core/shader/recompiler/`)
- [ ] Expand `libnx`/`libkernel` coverage with integration tests

Good first issues are labeled `good first issue`. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Legal

Interoperability and preservation only. This project ships no games, keys, firmware, or Nintendo code. It consumes user-provided decrypted binaries; the decryption process is outside this project's scope. All system libraries are clean-room host reimplementations against publicly documented interfaces.

## License

MIT. See [LICENSE](LICENSE).

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Report vulnerabilities via [SECURITY.md](SECURITY.md).
