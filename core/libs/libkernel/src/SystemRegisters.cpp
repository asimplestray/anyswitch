#include "libkernel/SystemRegisters.hpp"

#include <tuple>

namespace libkernel {

namespace {

// op0/op1/CRn/CRm/op2 -> register, for the ones that matter.
SystemRegister Identify(std::uint32_t op0, std::uint32_t op1, std::uint32_t crn,
                       std::uint32_t crm, std::uint32_t op2) {
    if (op0 == 3 && op1 == 3 && crn == 13 && crm == 0 && op2 == 2)
        return SystemRegister::ThreadPointer;
    if (op0 == 3 && op1 == 3 && crn == 13 && crm == 0 && op2 == 3)
        return SystemRegister::ThreadPointerReadOnly;
    if (op0 == 3 && op1 == 3 && crn == 14 && crm == 0 && op2 == 1)
        return SystemRegister::VirtualCount;
    if (op0 == 3 && op1 == 3 && crn == 0 && crm == 0 && op2 == 1)
        return SystemRegister::MainId;
    return SystemRegister::Unknown;
}

} // namespace

SystemRegisterMove DecodeSystemRegisterMove(std::uint32_t insn) {
    // MRS: bits 31-20 = 0xD53x, MSR (register): bits 31-20 = 0xD51x.
    // Bit 21 is the L bit and selects the operand width; bit 31 must be set.
    const auto top = insn & 0xFFF00000u;
    SystemRegisterMove move;
    if (!(insn & 0x80000000u))
        return move;
    if (top != 0xD5300000u && top != 0xD5100000u)
        return move;

    const auto op0 = (insn >> 19) & 3;
    const auto op1 = (insn >> 16) & 7;
    const auto crn = (insn >> 12) & 0xF;
    const auto crm = (insn >> 8) & 0xF;
    const auto op2 = (insn >> 5) & 7;

    move.valid = true;
    move.kind = (top == 0xD5300000u) ? MoveKind::Read : MoveKind::Write;
    move.reg = Identify(op0, op1, crn, crm, op2);
    move.rt = static_cast<int>(insn & 0x1F);
    move.is64 = (insn & (1u << 21)) != 0;
    return move;
}

} // namespace libkernel
