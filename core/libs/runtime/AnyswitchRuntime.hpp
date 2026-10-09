#pragma once

// AnySwitch guest runtime — the native glue that makes translated code run.
//
// Remill's lifted functions have the shape:
//     Memory* trace(State* state, uint64_t pc, Memory* memory);
// and call out to `__remill_*` intrinsics for anything the semantics cannot
// express natively. Those intrinsics are undefined in every Remill bitcode
// file: providing them is the integrator's job, and this is that layer.
//
// There is no emulator here — no interpretation, no JIT, no dispatch loop.
// The guest's code was already translated ahead of time; this is just the
// memory model, register file and hypercall seam it crosses.
//
// Note on namespaces: Remill's runtime headers define State, Memory and
// friends in the *global* namespace, not under remill::.

#include <cstddef>
#include <cstdint>
#include <vector>

// Memory is defined once, in libkernel/Memory.hpp, so the host libraries and
// the runtime share one type. Remill forward-declares it and never touches it;
// the read/write intrinsics that do are ours.
#include "libkernel/Memory.hpp"

namespace anyswitch {

// Guest address space: a flat, bounds-checked byte array.
class GuestMemory {
public:
    explicit GuestMemory(std::size_t size, std::uint64_t base = 0);
    ~GuestMemory();

    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    Memory* Handle() { return &_handle; }

    // Loads/store relative to the guest base. Returns false if out of range.
    bool Read(std::uint64_t addr, void* out, std::size_t len) const;
    bool Write(std::uint64_t addr, const void* in, std::size_t len);

    std::uint64_t Base() const { return _base; }
    std::size_t Size() const { return _handle.size; }

    // Bytes of guest image currently mapped, i.e. where a fresh heap can start.
    static std::size_t LoadedBytes() { return _loadedBytes; }

private:
    std::uint8_t* At(std::uint64_t addr, std::size_t len);

    Memory _handle;
    std::uint64_t _base;
    std::vector<std::uint8_t> _backing;
    static inline std::size_t _loadedBytes = 0;
};

} // namespace anyswitch
