#include "AnyswitchRuntime.hpp"

#include "remill/Arch/AArch64/Runtime/State.h"
#include "Syscalls.hpp"

#include "Presentation.hpp"

#include "libkernel/SystemRegisters.hpp"
#include "remill/Arch/Runtime/HyperCall.h"
#include "remill/Arch/Runtime/Intrinsics.h"

#include <cstdio>
#include <cstring>

namespace anyswitch {

GuestMemory::GuestMemory(std::size_t size, std::uint64_t base)
    : _handle{}, _base(base), _backing(size, 0) {
    // The handle points at our storage so the intrinsics see a stable address.
    _handle.data = _backing.data();
    _handle.size = _backing.size();
}

GuestMemory::~GuestMemory() = default;

std::uint8_t* GuestMemory::At(std::uint64_t addr, std::size_t len) {
    if (addr < _base)
        return nullptr;
    const auto off = static_cast<std::size_t>(addr - _base);
    if (off + len < off || off + len > _backing.size())
        return nullptr;
    return _backing.data() + off;
}

bool GuestMemory::Read(std::uint64_t addr, void* out, std::size_t len) const {
    if (addr < _base)
        return false;
    const auto off = static_cast<std::size_t>(addr - _base);
    if (off + len < off || off + len > _backing.size())
        return false;
    std::memcpy(out, _backing.data() + off, len);
    return true;
}

bool GuestMemory::Write(std::uint64_t addr, const void* in, std::size_t len) {
    auto* dst = At(addr, len);
    if (!dst)
        return false;
    // Track how far the loaded image reaches so a fresh heap starts past it.
    if (addr == _base && len > _loadedBytes)
        _loadedBytes = len;
    std::memcpy(dst, in, len);
    return true;
}

} // namespace anyswitch

// ---------------------------------------------------------------------------
// Remill intrinsics. Signatures must match remill/Arch/Runtime/Intrinsics.h.
// ---------------------------------------------------------------------------

// Comparisons arrive already computed by the semantics; the intrinsic is a
// hook for instrumentation, so the default is the identity.
#define DEFINE_COMPARE(name) \
    extern "C" bool __remill_compare_##name(bool result) { return result; }

DEFINE_COMPARE(eq)
DEFINE_COMPARE(neq)
DEFINE_COMPARE(slt)
DEFINE_COMPARE(sle)
DEFINE_COMPARE(sgt)
DEFINE_COMPARE(sge)
DEFINE_COMPARE(ult)
DEFINE_COMPARE(ule)
DEFINE_COMPARE(ugt)
DEFINE_COMPARE(uge)

#undef DEFINE_COMPARE

// Condition-flag helpers: same reasoning as the comparisons above.
extern "C" bool __remill_flag_computation_zero(bool result, ...) { return result; }
extern "C" bool __remill_flag_computation_sign(bool result, ...) { return result; }
extern "C" bool __remill_flag_computation_overflow(bool result, ...) { return result; }
extern "C" bool __remill_flag_computation_carry(bool result, ...) { return result; }

// Memory barriers and exclusives are no-ops on a strongly ordered host.
extern "C" Memory* __remill_barrier_load_load(Memory* mem) { return mem; }
extern "C" Memory* __remill_barrier_load_store(Memory* mem) { return mem; }
extern "C" Memory* __remill_barrier_store_store(Memory* mem) { return mem; }
extern "C" Memory* __remill_barrier_store_load(Memory* mem) { return mem; }
extern "C" Memory* __remill_atomic_begin(Memory* mem) { return mem; }
extern "C" Memory* __remill_atomic_end(Memory* mem) { return mem; }

// An unimplemented instruction reported by the emulator path. There is no
// emulator in this project, so this is a translation gap to report.
extern "C" Memory* __remill_aarch64_emulate_instruction(Memory* mem) { return mem; }

// Floating-point exception state. There is no FPU trap model on the host, so
// these are bookkeeping no-ops; the guest sees a warm FPU that never traps.
extern "C" void __remill_fpu_exception_clear(std::int32_t) {}
extern "C" void __remill_fpu_exception_raise(std::int32_t) {}
extern "C" std::int32_t __remill_fpu_exception_test(std::int32_t) { return 0; }
extern "C" void __remill_fpu_set_rounding(std::int32_t) {}
extern "C" std::int32_t __remill_fpu_get_rounding() { return 0; }

// Undefined-value helpers, used when the semantics need a poison value rather
// than a concrete one. Zero is a safe stand-in.
#define DEFINE_UNDEFINED(name, type) \
    extern "C" type __remill_undefined_##name() { return 0; }

DEFINE_UNDEFINED(8, uint8_t)
DEFINE_UNDEFINED(16, uint16_t)
DEFINE_UNDEFINED(32, uint32_t)
DEFINE_UNDEFINED(64, uint64_t)
extern "C" float __remill_undefined_f32() { return 0.0f; }
extern "C" double __remill_undefined_f64() { return 0.0; }

#undef DEFINE_UNDEFINED

// Register/PC offsets, measured against remill/Arch/AArch64/Runtime/State.h:
//   offsetof(AArch64State, gpr) = 536, GPR.x0 = 8, GPR.pc = 520.
// Called by libkernel's svcOutputDebugString so guest text reaches the window
// without the libraries taking on an SDL2 dependency.
extern "C" void PresentGuestText(const char* text, std::size_t len) {
    if (text == nullptr || len == 0)
        return;
    anyswitch::Presentation::Get().AppendGuestText(std::string(text, len));
}
namespace {
std::uint64_t g_virtualCount = 0;

constexpr std::size_t kStateGprOffset = 536;
constexpr std::size_t kGprX0Offset = 8;
constexpr std::size_t kGprPcOffset = 520;

// ArchState::hyper_call / hyper_call_vector, measured against the same header.
constexpr std::size_t kHyperCallOffset = 0;
constexpr std::size_t kHyperCallVectorOffset = 8;
constexpr std::uint32_t kAArch64SupervisorCall = 12;
constexpr std::uint32_t kInvalidHyperCall = 0;

void SetX0(State& state, std::uint64_t value) {
    auto* gpr = reinterpret_cast<std::uint8_t*>(&state) + kStateGprOffset;
    std::memcpy(gpr + kGprX0Offset, &value, sizeof(value));
}

// Services one guest syscall, writing results back into the register file the
// way the console's kernel would. X0 carries the result; X1+ carry extras.
void ServiceSyscall(State& state, std::uint64_t svc, Memory* mem) {
    anyswitch::SyscallFrame frame{};
    if (anyswitch::HandleSyscall(svc, frame, state, mem))
        return;

    // Unhandled: report loudly rather than silently returning a success the
    // guest will believe.
    static std::uint64_t reported = 0;
    if (reported < 20) {
        std::fprintf(stderr, "anyswitch: unhandled guest svc #%llu\n",
                     static_cast<unsigned long long>(svc));
        ++reported;
    }
    SetX0(state, 0);
}

} // namespace

extern "C" {

Memory* __remill_function_call(State& state, addr_t addr, Memory* mem) {
    (void)addr;
    return mem;
}

// The guest executed RET. Update the architectural PC so a caller can observe
// where control ended up, then hand the memory back.
Memory* __remill_function_return(State& state, addr_t addr, Memory* mem) {
    auto* gpr = reinterpret_cast<std::uint8_t*>(&state) + kStateGprOffset;
    auto* pc = reinterpret_cast<addr_t*>(gpr + kGprPcOffset);
    *pc = addr;
    return mem;
}

Memory* __remill_jump(State& state, addr_t addr, Memory* mem) {
    (void)state;
    (void)addr;
    return mem;
}

Memory* __remill_missing_block(State& state, addr_t addr, Memory* mem) {
    (void)state;
    (void)addr;
    return mem;
}

Memory* __remill_async_hyper_call(State& state, addr_t ret_addr, Memory* mem) {
    // ArchState::hyper_call is a uint32 at offset 0; hyper_call_vector is a
    // uint64 at offset 8 holding the SVC immediate (the guest syscall number).
    auto* base = reinterpret_cast<std::uint8_t*>(&state);
    const auto name = *reinterpret_cast<const std::uint32_t*>(base);
    const auto vector = *reinterpret_cast<const std::uint64_t*>(base + 8);

    if (name == kAArch64SupervisorCall) {
        // Service the guest syscall. The handler writes results back into the
        // State (X0, X1, ...) exactly as the console's kernel would.
        ServiceSyscall(state, vector, mem);
    } else {
        std::fprintf(stderr, "anyswitch: unhandled hyper_call %u (vector %llu)\n",
                     name, static_cast<unsigned long long>(vector));
    }

    // Clear so the block-boundary check does not re-enter.
    *reinterpret_cast<std::uint32_t*>(base) = kInvalidHyperCall;
    return mem;
}

// Remill's AArch64 trace lifter has no implementation for SVC: it reports the
// instruction as an error call carrying the guest PC, then keeps lifting the
// rest of the trace. That is the seam we service syscalls through, so this
// reads the instruction at that PC and dispatches when it really is an SVC.
// Applies a decoded system-register move to the guest's register file.
//
// The pointer registers live in State.sr; the counter and the CPU id are
// fabricated, since there is no hardware here to read.
//
// Returns true when the instruction was recognised and executed.
bool TrySystemRegisterAccess(State& state, std::uint32_t insn) {
    const auto move = libkernel::DecodeSystemRegisterMove(insn);
    if (!move.valid)
        return false;

    // Offsets measured against remill/Arch/AArch64/Runtime/State.h.
    constexpr std::size_t kRegX0 = 544;
    constexpr std::size_t kStride = 16;
    constexpr std::size_t kStateTpidrEl0Offset = 1112;
    constexpr std::uint64_t kMainIdEl1 = 0x410FD034ull; // Cortex-A57-ish

    auto* base = reinterpret_cast<std::uint8_t*>(&state);
    auto* rt = base + kRegX0 + static_cast<std::size_t>(move.rt) * kStride;

    std::uint64_t value = 0;
    if (move.kind == libkernel::MoveKind::Read) {
        switch (move.reg) {
            case libkernel::SystemRegister::ThreadPointer:
            case libkernel::SystemRegister::ThreadPointerReadOnly:
                std::memcpy(&value, base + kStateTpidrEl0Offset, sizeof(value));
                break;
            case libkernel::SystemRegister::VirtualCount:
                // A free-running counter is better than a constant one: guests
                // measure elapsed time with this.
                ++g_virtualCount;
                value = g_virtualCount;
                break;
            case libkernel::SystemRegister::MainId:
                value = kMainIdEl1;
                break;
            case libkernel::SystemRegister::Unknown:
            default:
                return false;
        }
        if (move.is64)
            std::memcpy(rt, &value, sizeof(value));
        else
            std::memset(rt, 0, sizeof(value));
        return true;
    }

    if (move.kind == libkernel::MoveKind::Write) {
        if (move.reg == libkernel::SystemRegister::ThreadPointer && move.is64)
            std::memcpy(base + kStateTpidrEl0Offset, rt, sizeof(std::uint64_t));
        else if (move.reg == libkernel::SystemRegister::Unknown)
            return false;
        return true;
    }
    return false;
}

Memory* __remill_error(State& state, addr_t addr, Memory* mem) {
    if (std::getenv("ANYSWITCH_DBG_REMILL"))
        std::fprintf(stderr, "[remill_error] addr=%llu\n",
                     static_cast<unsigned long long>(addr));
    // Remill reports instructions it cannot lift through this entry point, and
    // it passes the PC *after* the instruction rather than the instruction's
    // own PC, so both candidates are checked. Two classes matter: a supervisor
    // call, which is a syscall, and a system-register move, which on AArch64 is
    // unimplemented for TPIDR_EL0.
    if (mem) {
        for (const addr_t pc : {addr - 4, addr}) {
            if (pc + 4 > mem->size)
                continue;
            std::uint32_t insn = 0;
            std::memcpy(&insn, mem->data + pc, 4);

            // SVC #imm : bits 31-24 = 0xD4, immediate in bits 20-5.
            if ((insn & 0xFF000000u) == 0xD4000000u) {
                const auto svc = static_cast<std::uint64_t>((insn >> 5) & 0xFFFFu);
                ServiceSyscall(state, svc, mem);
                return mem;
            }

            if (TrySystemRegisterAccess(state, insn))
                return mem;
        }
    }
    std::fprintf(stderr, "anyswitch: unlifted instruction at guest 0x%llx\n",
                 static_cast<unsigned long long>(addr));
    return mem;
}

// Memory access. Everything is bounds-checked against the guest mapping;
// out-of-range access yields zero rather than faulting, which keeps
// translated code running while still being observable in tests.
Memory* __remill_write_memory_8(Memory* mem, addr_t addr, uint8_t val) {
    if (mem && addr < mem->size)
        mem->data[addr] = val;
    return mem;
}

#define DEFINE_WRITE(width, type)                                            \
    Memory* __remill_write_memory_##width(Memory* mem, addr_t addr, type val) { \
        if (mem && addr + (width / 8) <= mem->size)                          \
            std::memcpy(mem->data + addr, &val, width / 8);                  \
        return mem;                                                          \
    }

DEFINE_WRITE(16, uint16_t)
DEFINE_WRITE(32, uint32_t)
DEFINE_WRITE(64, uint64_t)

#undef DEFINE_WRITE

Memory* __remill_write_memory_f32(Memory* mem, addr_t addr, float val) {
    if (mem && addr + 4 <= mem->size)
        std::memcpy(mem->data + addr, &val, 4);
    return mem;
}

Memory* __remill_write_memory_f64(Memory* mem, addr_t addr, double val) {
    if (mem && addr + 8 <= mem->size)
        std::memcpy(mem->data + addr, &val, 8);
    return mem;
}

uint8_t __remill_read_memory_8(Memory* mem, addr_t addr) {
    return (mem && addr < mem->size) ? mem->data[addr] : 0;
}

#define DEFINE_READ(width, type)                                             \
    type __remill_read_memory_##width(Memory* mem, addr_t addr) {            \
        type val = 0;                                                        \
        if (mem && addr + (width / 8) <= mem->size)                          \
            std::memcpy(&val, mem->data + addr, width / 8);                  \
        return val;                                                          \
    }

DEFINE_READ(16, uint16_t)
DEFINE_READ(32, uint32_t)
DEFINE_READ(64, uint64_t)

#undef DEFINE_READ

float __remill_read_memory_f32(Memory* mem, addr_t addr) {
    if (!mem || addr + 4 > mem->size)
        return 0.0f;
    float v = 0.0f;
    std::memcpy(&v, mem->data + addr, 4);
    return v;
}

double __remill_read_memory_f64(Memory* mem, addr_t addr) {
    if (!mem || addr + 8 > mem->size)
        return 0.0;
    double v = 0.0;
    std::memcpy(&v, mem->data + addr, 8);
    return v;
}

} // extern "C"
