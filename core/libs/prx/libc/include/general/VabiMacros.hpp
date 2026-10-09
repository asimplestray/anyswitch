#pragma once

// Host ABI for functions called from translated guest code.
// Translated ARM64 (AAPCS64) calls land here; on Windows hosts we force
// SysV so translated code uses a single calling convention, on Linux/macOS
// the native ABI already matches.
#ifdef _WIN32
#define ANYSWITCH_HOST_ABI __attribute__((sysv_abi))
#else
#define ANYSWITCH_HOST_ABI
#endif
