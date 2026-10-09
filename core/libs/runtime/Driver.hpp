#pragma once

// The runtime driver: what actually executes translated code.
//
// A lifted trace is one basic block. It runs, updates the guest's PC in State,
// and returns. Without something that reads that PC and runs the next trace,
// a translated program executes one block and stops - which is all the earlier
// harness was able to show.
//
// This driver closes that loop: run the trace at the entry PC, read the PC
// back out of State, look up the trace for it, run that, and repeat until
// control leaves the code or a handler ends it.

#include "libkernel/Memory.hpp"

#include <cstdint>

struct State;

namespace anyswitch {

// How a trace is invoked. Mirrors what Remill's lifting produces:
//   Memory* trace(State* state, uint64_t pc, Memory* memory)
using TraceFunction = Memory* (*)(State*, std::uint64_t, Memory*);

// Maps a guest PC to the host function that implements it. Built by the caller
// from the relinker's trace table.
class TraceMap {
public:
    virtual ~TraceMap() = default;
    virtual TraceFunction Lookup(std::uint64_t guestPc) const = 0;
};

struct DriveLimits {
    // Safety rails: a guest that never leaves the code region would otherwise
    // spin forever.
    std::uint64_t maxTraces = 100'000'000;
};

// Runs translated code from `entryPc`. Returns the PC control ended at, and
// false if it stopped for a reason worth reporting (unknown PC, limit hit).
bool Drive(State& state, Memory* mem, std::uint64_t entryPc,
           const TraceMap& traces, const DriveLimits& limits,
           std::uint64_t& exitPc);

} // namespace anyswitch
