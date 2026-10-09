# 3rdparty — optional vendored dependencies

Pinned as **shallow git submodules**: our repo stores only commit hashes,
never upstream source. A fresh clone downloads nothing extra.

| Directory | Upstream | Purpose | Enabled by |
|---|---|---|---|
| `remill/` | https://github.com/lifting-bits/remill | ARM64 → LLVM IR lifting | `ANYSWITCH_ENABLE_LLVM=ON` + system Remill, or vendored source build |
| `glslang/` | https://github.com/KhronosGroup/glslang | GLSL → SPIR-V (shader recompiler) | `ANYSWITCH_BUILD_SHADER_RECOMPILER=ON` |

## Fetching (only if you need translation/shaders)

```sh
# Everything at once (shallow):
git submodule update --init --depth 1

# Or individually:
git submodule update --init --depth 1 3rdparty/remill
git submodule update --init --depth 1 3rdparty/glslang
```

## Building with them

LLVM itself is **never vendored** (gigabytes of source, hours to build).
Install it from your distro (`llvm-dev`) and let CMake find it:

```sh
# Preferred: system Remill + system LLVM (Remill ships packages / build
# instructions upstream; its own source build uses a superbuild, see
# 3rdparty/remill/docs/DEPENDENCIES.md).
cmake -S . -B build -DANYSWITCH_ENABLE_LLVM=ON -DANYSWITCH_USE_SYSTEM_REMILL=ON

# Best-effort vendored source build (needs remill's superbuild deps):
cmake -S . -B build -DANYSWITCH_ENABLE_LLVM=ON

# Local Remill build against system LLVM (Arch: pacman -S llvm clang;
# no sudo needed, ~10 min for XED/glog/gtest — skips building LLVM):
cmake -G Ninja -S 3rdparty/remill/dependencies -B 3rdparty/remill/dependencies/build \
  -DUSE_EXTERNAL_LLVM=ON
cmake --build 3rdparty/remill/dependencies/build
cmake -G Ninja -S 3rdparty/remill -B 3rdparty/remill/build -DCMAKE_BUILD_TYPE=Release \
  -DREMILL_FETCH_SLEIGH=OFF \
  -DCMAKE_PREFIX_PATH="$(pwd)/3rdparty/remill/dependencies/install"
cmake --build 3rdparty/remill/build
# Then point AnySwitch at it:
cmake -S . -B build -DANYSWITCH_ENABLE_LLVM=ON \
  -DCMAKE_PREFIX_PATH="$(pwd)/3rdparty/remill/build;$(pwd)/3rdparty/remill/dependencies/install"

# Shader recompiler (needs the glslang submodule above):
cmake -S . -B build -DANYSWITCH_BUILD_SHADER_RECOMPILER=ON
```

Without LLVM/Remill/glslang the project still configures and builds;
translation features disable gracefully with a CMake status message.

## Pinning updates

```sh
cd 3rdparty/remill && git fetch --depth 1 origin && git checkout <sha>
cd ../.. && git add 3rdparty/remill && git commit -m "Pin remill to <sha>"
```

Do not commit build output. See root `.gitignore`.
