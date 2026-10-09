#pragma once

// Guest syscall handlers for the AnySwitch runtime.
//
// Arguments arrive in the guest's X0-X5 and results are written back to X0
// (plus X1 and beyond for syscalls with extra outputs). The syscall number is
// the immediate of the SVC instruction, not a register.
//
// The handlers themselves live in core/libs/libkernel, so they can be built as
// a .prx for games and unit-tested in isolation. This header only declares the
// seam the runtime drives.

#include "libkernel/Memory.hpp"

#include <cstdint>

struct State;

namespace anyswitch {

// The guest's flattened argument registers, as filled by the runtime.
struct SyscallFrame {
    std::uint64_t x[6];
    std::uint32_t w[6];
};

// Returns true when libkernel recognised and handled the syscall.
bool HandleSyscall(std::uint64_t svc, SyscallFrame& frame, State& state, Memory* mem);

} // namespace anyswitch
