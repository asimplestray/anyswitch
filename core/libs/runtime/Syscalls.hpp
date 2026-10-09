#pragma once

// Guest syscall handlers for the AnySwitch runtime.
//
// Arguments arrive in the guest's X0-X5 and results are written back to X0
// (plus X1 and beyond for syscalls with extra outputs). The syscall number is
// the immediate of the SVC instruction, not a register.
//
// Numbers and signatures follow the SwitchBrew SVC table.

#include <cstdint>

struct State;
struct Memory;

namespace anyswitch {

// Result codes the guest checks.
constexpr std::uint32_t kResultSuccess = 0;

// Registers used to pass syscall arguments and read results.
struct SyscallFrame {
    std::uint64_t x[6];
};

// Returns true when the syscall was recognised and handled. Handlers write
// their results into `frame`; unrecognised numbers leave it untouched and
// return false so the caller can report the gap.
bool HandleSyscall(std::uint64_t svc, SyscallFrame& frame, State& state, Memory* mem);

} // namespace anyswitch
