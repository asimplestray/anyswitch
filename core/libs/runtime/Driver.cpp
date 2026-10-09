#include "Driver.hpp"

#include "remill/Arch/AArch64/Runtime/State.h"

#include <cstdint>
#include <cstring>

namespace anyswitch {

namespace {

// Register-file layout measured against remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kGprOffset = 536;
constexpr std::size_t kGprPcOffset = 520;
constexpr std::size_t kStatePcOffset = kGprOffset + kGprPcOffset; // 1056

std::uint64_t ReadPc(const State& state) {
    std::uint64_t pc = 0;
    std::memcpy(&pc, reinterpret_cast<const std::uint8_t*>(&state) + kStatePcOffset,
                sizeof(pc));
    return pc;
}

} // namespace

bool Drive(State& state, Memory* mem, std::uint64_t entryPc,
           const TraceMap& traces, const DriveLimits& limits,
           std::uint64_t& exitPc) {
    std::uint64_t pc = entryPc;
    std::uint64_t executed = 0;

    while (executed++ < limits.maxTraces) {
        auto* trace = traces.Lookup(pc);
        if (trace == nullptr) {
            exitPc = pc;
            return false; // no trace at this PC: a translation gap
        }

        mem = trace(&state, pc, mem);
        if (mem == nullptr) {
            exitPc = pc;
            return false;
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
