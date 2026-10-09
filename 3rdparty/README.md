# 3rdparty — optional vendored dependencies

All entries here are **optional**. A clean checkout builds without them;
translation features simply disable gracefully with a CMake status message.

| Directory | Upstream | Purpose | Enabled by |
|---|---|---|---|
| `remill/` | https://github.com/trailofbits/remill | ARM64 → LLVM IR lifting | `ANYSWITCH_ENABLE_LLVM=ON` + vendored dir or `ANYSWITCH_USE_SYSTEM_REMILL=ON` |
| `glslang/` | https://github.com/KhronosGroup/glslang | GLSL → SPIR-V (shader recompiler) | `ANYSWITCH_BUILD_SHADER_RECOMPILER=ON` |

To vendor Remill:

```sh
git clone https://github.com/trailofbits/remill 3rdparty/remill
cmake -S . -B build -DANYSWITCH_ENABLE_LLVM=ON
```

To vendor glslang:

```sh
git clone https://github.com/KhronosGroup/glslang 3rdparty/glslang
cmake -S . -B build -DANYSWITCH_BUILD_SHADER_RECOMPILER=ON
```

Do not commit build output. See root `.gitignore`.
