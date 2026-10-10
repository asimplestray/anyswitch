#include "Driver.hpp"

#include "remill/Arch/AArch64/Runtime/State.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace anyswitch {

namespace {

// Register-file layout measured against remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kGprOffset = 536;
constexpr std::size_t kGprPcOffset = 520;
constexpr std::size_t kStatePcOffset = kGprOffset + kGprPcOffset; // 1056

// ArchState::hyper_call, a uint32 at offset 0. AArch64 SVC leaves the call
// recorded here; the value is AsyncHyperCall::Name.
constexpr std::size_t kStateHyperCallOffset = 0;
constexpr std::uint32_t kAArch64SupervisorCall = 12;

bool HasPendingSyscall(const State& state) {
    std::uint32_t name = 0;
    std::memcpy(&name, reinterpret_cast<const std::uint8_t*>(&state) + kStateHyperCallOffset,
                sizeof(name));
    return name == kAArch64SupervisorCall;
}

// ArchState::hyper_call_vector, a uint64 at offset 8, holds the SVC immediate.
std::uint64_t ReadSyscallVector(const State& state) {
    std::uint64_t vector = 0;
    std::memcpy(&vector, reinterpret_cast<const std::uint8_t*>(&state) + 8, sizeof(vector));
    return vector;
}

void ClearPendingSyscall(State& state) {
    const std::uint32_t name = 0;
    std::memcpy(reinterpret_cast<std::uint8_t*>(&state) + kStateHyperCallOffset, &name,
                sizeof(name));
}

std::uint64_t ReadPc(const State& state) {
    std::uint64_t pc = 0;
    std::memcpy(&pc, reinterpret_cast<const std::uint8_t*>(&state) + kStatePcOffset,
                sizeof(pc));
    return pc;
}

} // namespace

bool Drive(State& state, Memory* mem, std::uint64_t entryPc,
           const TraceMap& traces, const DriveLimits& limits,
           std::uint64_t& exitPc, SyscallSink onSyscall) {
    std::uint64_t pc = entryPc;
    std::uint64_t executed = 0;

    const bool tracePc = std::getenv("ANYSWITCH_TRACE_PC") != nullptr;
    while (executed++ < limits.maxTraces) {
        auto* trace = traces.Lookup(pc);
        if (trace == nullptr) {
            exitPc = pc;
            return false; // no trace at this PC: a translation gap
        }
        if (tracePc)
            std::fprintf(stderr, "  trace 0x%llx\n",
                         static_cast<unsigned long long>(pc));

        mem = trace(&state, pc, mem);
        if (mem == nullptr) {
            exitPc = pc;
            return false;
        }

        // A syscall the guest recorded while running the trace.
        if (onSyscall != nullptr && HasPendingSyscall(state)) {
            const auto vector = ReadSyscallVector(state);
            ClearPendingSyscall(state);
            if (!onSyscall(state, vector, mem)) {
                exitPc = pc;
                return false; // handled and terminating, e.g. ExitProcess
            }
        }

        const auto next = ReadPc(state);
        if (next == pc) {
            // An instruction that does not advance, e.g. a handler that
            // returned without moving control on. Running it again would spin.
            exitPc = pc;
            return false;
        }
        pc = next;
    }

    exitPc = pc;
    return false; // limit reached
}

} // namespace anyswitch
