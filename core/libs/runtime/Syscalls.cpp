#include "Syscalls.hpp"

#include "libkernel/Dispatch.hpp"
#include "remill/Arch/AArch64/Runtime/State.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Translates between the guest's register file and the libkernel ABI, and
// routes each syscall into the host implementation.
//
// libkernel owns *what* a syscall does; this file owns *how the guest's
// registers map onto its arguments*. Keeping them apart is what lets the
// library be tested without a translated binary.

namespace anyswitch {

namespace {

// Register-file layout measured against remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kGprOffset = 536;
constexpr std::size_t kRegStride = 16; // volatile uint64_t padding, then Reg
constexpr std::size_t kRegX0 = kGprOffset + 8;

std::uint64_t ReadX(const State& state, int n) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&state) + kRegX0 + n * kRegStride;
    std::uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

void WriteX(State& state, int n, std::uint64_t v) {
    auto* p = reinterpret_cast<std::uint8_t*>(&state) + kRegX0 + n * kRegStride;
    std::memcpy(p, &v, sizeof(v));
}

} // namespace

bool HandleSyscall(std::uint64_t svc, SyscallFrame& frame, State& state, Memory* mem) {
    if (!mem)
        return false;

    // The syscall number is the SVC immediate, and arguments start at X0.
    libkernel::SyscallArgs args{};
    for (int i = 0; i < 6; ++i) {
        args.x[i] = ReadX(state, i);
        args.w[i] = static_cast<std::uint32_t>(args.x[i]);
    }

    if (!libkernel::Dispatch(svc, args, *mem))
        return false;

    // X0 carries the result; X1 and beyond carry extra outputs. A handler only
    // ever writes the registers it changes, so read the untouched ones back
    // from the guest rather than clobbering them with whatever was in args.
    for (int i = 0; i < 6; ++i) {
        if (args.x[i] != ReadX(state, i))
            WriteX(state, i, args.x[i]);
    }
    return true;
}

} // namespace anyswitch
