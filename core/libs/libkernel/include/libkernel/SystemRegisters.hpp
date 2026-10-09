#pragma once

// Decoding of AArch64 system-register moves (MRS / MSR).
//
// Remill's AArch64 semantics leaves most of these unimplemented, so any guest
// that touches its thread pointer, the timer, or the CPU id arrives in the
// runtime as an "unlifted instruction". Deciding *what* a move means is pure
// logic and lives here so it can be unit-tested; acting on the guest's
// register file is the runtime's job, because that needs Remill's State.
//
// These three dominate real binaries: TPIDRRO_EL0 is how the guest reaches its
// TLS (172 and 188 sites in the two binaries tested), CNTPCT_EL0 is its clock,
// and MIDR_EL1 identifies the CPU.

#include <cstdint>

namespace libkernel {

// Which system register a move refers to. Registers not listed here are
// recognised but not modelled, so the runtime can report them rather than
// silently succeeding.
enum class SystemRegister {
    Unknown,
    ThreadPointer,        // TPIDR_EL0   (writable thread pointer)
    ThreadPointerReadOnly,// TPIDRRO_EL0 (read-only thread pointer)
    VirtualCount,         // CNTPCT_EL0  (virtual counter)
    MainId,               // MIDR_EL1    (processor ID)
};

enum class MoveKind {
    Unknown,
    Read,  // MRS Rt, sysreg
    Write, // MSR sysreg, Rt
};

struct SystemRegisterMove {
    bool valid = false;
    MoveKind kind = MoveKind::Unknown;
    SystemRegister reg = SystemRegister::Unknown;
    int rt = 0;    // general-purpose register involved
    bool is64 = false;
};

// Decodes one instruction. Returns an invalid move when the instruction is not
// a system-register move at all.
SystemRegisterMove DecodeSystemRegisterMove(std::uint32_t insn);

} // namespace libkernel
