#include "AnyswitchRuntime.hpp"

#include "remill/Arch/AArch64/Runtime/State.h"
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
    std::memcpy(dst, in, len);
    return true;
}

} // namespace anyswitch

// ---------------------------------------------------------------------------
// Remill intrinsics. Signatures must match remill/Arch/Runtime/Intrinsics.h.
// ---------------------------------------------------------------------------

// The comparisons arrive already computed by the semantics; the intrinsic is
// a hook for instrumentation, so the default is the identity.
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

// Register/PC offsets, measured against remill/Arch/AArch64/Runtime/State.h:
//   offsetof(AArch64State, gpr) = 536, GPR.x0 = 8, GPR.pc = 520.
namespace {
constexpr std::size_t kStateGprOffset = 536;
constexpr std::size_t kGprX0Offset = 8;
constexpr std::size_t kGprPcOffset = 520;
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
    (void)state;
    (void)ret_addr;
    return mem;
}

Memory* __remill_error(State& state, addr_t addr, Memory* mem) {
    (void)state;
    (void)addr;
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

} // extern "C"
