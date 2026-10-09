// Memory-management syscalls: heap sizing and address-space queries.
//
// To add one, write a handler here following the existing shape and register
// it in Dispatch.cpp. Each handler reads arguments from `args.x` / `args.w`
// and writes results back, using Memory for anything that crosses into the
// guest address space.

#include "libkernel/Handlers.hpp"

#include "libkernel/SyscallAbi.hpp"

#include <cstdint>
#include <cstring>

namespace libkernel {
namespace {

// Heap region the runtime reports through svcGetInfo. Must stay inside the
// address space the runtime actually maps, or the guest's allocator refuses to
// start. The console places it far above 0; we describe our own mapping.
constexpr std::uint64_t kHeapRegionAddress = 0x10000000ull; // 256 MiB
constexpr std::uint64_t kHeapRegionSize = 0x10000000ull;    // 256 MiB

// Set by the runtime once the guest image is mapped, so a fresh heap starts
// past it rather than through the code.
inline std::size_t g_loadedBytes = 0;

bool WriteU64(Memory& mem, std::uint64_t value, std::uint64_t addr) {
    if (addr + sizeof(value) > mem.size)
        return false;
    std::memcpy(mem.data + addr, &value, sizeof(value));
    return true;
}

bool WriteStruct(Memory& mem, std::uint64_t addr, const void* in, std::size_t len) {
    if (addr + len > mem.size)
        return false;
    std::memcpy(mem.data + addr, in, len);
    return true;
}

// arch::MemoryInfo as written back by svcQueryMemory.
struct MemoryInfoLayout {
    std::uint64_t addr;
    std::uint64_t size;
    std::uint32_t state;
    std::uint32_t attr;
    std::uint32_t perm;
    std::uint32_t share_count;
    std::uint32_t device_refcount;
    std::uint32_t ipc_refcount;
};

constexpr std::uint32_t kMemStateNormal = 0;
constexpr std::uint32_t kMemPermReadWrite = 3;

} // namespace

// Records how far the loaded image reaches. The runtime calls this after
// mapping the guest, so SvSetHeapSize knows where a heap may start.
void SetLoadedImageBytes(std::size_t bytes) { g_loadedBytes = bytes; }

std::size_t LoadedImageBytes() { return g_loadedBytes; }
std::uint64_t HeapRegionAddress() { return kHeapRegionAddress; }
std::uint64_t HeapRegionSize() { return kHeapRegionSize; }

// svcSetHeapSize(uintptr_t* out, size_t size) -> result, heap base in X1.
void SvSetHeapSize(SyscallArgs& args, Memory& mem) {
    // The kernel picks the base, and it is the start of the heap region: the
    // console fixes that address, and reporting it is what keeps GetInfo and
    // SetHeapSize describing the same memory. It has to land inside the guest
    // address space the runtime owns, or every allocation the guest makes from
    // it would be silently dropped.
    const auto heapBase = kHeapRegionAddress;
    if (g_loadedBytes == 0 || g_loadedBytes >= heapBase || heapBase >= mem.size) {
        args.x[0] = 0xCA01; // invalid size
        return;
    }
    WriteU64(mem, heapBase, static_cast<std::uint64_t>(args.x[0]));
    args.x[0] = kResultSuccess;
    args.x[1] = heapBase;
}

// svcQueryMemory(MemoryInfo* out, PageInfo* page, uintptr_t addr)
//             -> result, PageInfo in W1.
void SvQueryMemory(SyscallArgs& args, Memory& mem) {
    // Report the whole mapped range as one normal read/write mapping. That is
    // enough for a guest allocator to believe it has memory to work with.
    MemoryInfoLayout info{};
    info.addr = 0;
    info.size = mem.size;
    info.state = kMemStateNormal;
    info.perm = kMemPermReadWrite;
    WriteStruct(mem, args.x[0], &info, sizeof(info));

    args.x[0] = kResultSuccess;
    // PageInfo: flags in W1, bit0 set meaning "the query succeeded".
    args.x[1] = 1;
}

// svcSetMemoryPermission / svcSetMemoryAttribute — no-ops on a host whose
// pages are already writable and uncached.
void SvSetMemoryPermission(SyscallArgs& args, Memory&) {
    args.x[0] = kResultSuccess;
}

void SvSetMemoryAttribute(SyscallArgs& args, Memory&) {
    args.x[0] = kResultSuccess;
}

// svcUnmapTransferMemory(Handle, uintptr_t, size) -> result
void SvUnmapTransferMemory(SyscallArgs& args, Memory&) { args.x[0] = kResultSuccess; }

// svcMapMemory / svcUnmapMemory — the runtime owns a flat address space, so
// remapping is already satisfied.
void SvMapMemory(SyscallArgs& args, Memory&) {
    args.x[0] = kResultSuccess;
}

void SvUnmapMemory(SyscallArgs& args, Memory&) {
    args.x[0] = kResultSuccess;
}

} // namespace libkernel
