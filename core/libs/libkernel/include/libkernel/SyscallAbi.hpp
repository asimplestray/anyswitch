#pragma once

// The syscall ABI shared by the guest runtime and the host implementations.
//
// Deliberately Remill-free: implementations depend only on these plain types,
// which is what lets them be unit-tested without a translated binary. The
// runtime owns the translation between the guest's register file and this
// struct.
//
// Arguments arrive in the guest's X0-X5 and results are written back to X0
// (plus X1+ for syscalls with extra outputs). The syscall number is the
// immediate of the SVC instruction, not a register.
//
// Numbers and signatures follow the SwitchBrew SVC table:
// https://switchbrew.org/wiki/SVC

#include "libkernel/Memory.hpp"

#include <cstddef>
#include <cstdint>

namespace libkernel {

// Result codes the guest checks. Success is zero; anything else is a failure
// whose module and description are encoded, as on the console.
constexpr std::uint32_t kResultSuccess = 0;

// Guest register file, flattened. `x` holds the 64-bit view used for most
// arguments; `w` holds the 32-bit view used where a syscall takes a handle or
// an enum.
struct SyscallArgs {
    std::uint64_t x[6];
    std::uint32_t w[6];
};

// Handler signature. Returns nothing; results are written into `args`.
using SyscallHandler = void (*)(SyscallArgs& args, Memory& mem);

// Heap-region facts reported by svcGetInfo. Defined in src/Memory.cpp.
// ReportedHeapRegion must stay inside the address space the runtime maps, or
// the guest's allocator refuses to start.
std::uint64_t HeapRegionAddress();
std::uint64_t HeapRegionSize();

// Set once the guest image is mapped, so svcSetHeapSize can place a fresh heap
// past it rather than through the code.
void SetLoadedImageBytes(std::size_t bytes);
std::size_t LoadedImageBytes();

} // namespace libkernel
