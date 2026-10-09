#pragma once

// Syscall dispatch for the host system libraries.
//
// This is the single seam between translated guest code and the host
// implementations: a syscall number maps to exactly one handler here. The
// runtime fills `args` from the guest's registers, calls Dispatch, and writes
// the results back.
//
// To add a syscall: implement a handler in the matching src/*.cpp file, then
// add one line to the table in src/Dispatch.cpp. Nothing else changes.

#include "libkernel/SyscallAbi.hpp"

#include <cstdint>

namespace libkernel {

// Returns true when the syscall was recognised and handled. Unhandled numbers
// leave `args` untouched so the caller can report the gap.
bool Dispatch(std::uint64_t svc, SyscallArgs& args, Memory& mem);

} // namespace libkernel
