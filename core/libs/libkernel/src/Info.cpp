// Process identity and diagnostics: the syscalls that report who the guest is
// and that let it tell the host something happened.

#include "libkernel/Handlers.hpp"

#include "libkernel/SyscallAbi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace libkernel {
namespace {

bool WriteU64(Memory& mem, std::uint64_t addr, std::uint64_t value) {
    if (addr + sizeof(value) > mem.size)
        return false;
    std::memcpy(mem.data + addr, &value, sizeof(value));
    return true;
}

} // namespace

namespace {

std::uint64_t InfoValue(std::uint32_t type) {
    // Handle 0 is invalid on the console, so libnx stores the main-thread
    // handle it is handed here and uses it for every subsequent thread
    // operation. Returning 0 made those calls fail deep inside init.
    constexpr std::uint64_t kMainThreadHandle = 0xFFFF8001ull;

    switch (type) {
        case 0:  // AllowedCpuIdBitmask
        case 1:  // AllowedThreadPrioBitmask
            return 0xFFFFFFFFFFFFFFFFull;
        case 2:  // AliasRegionAddress
        case 3:  // AliasRegionSize
            return 0;
        case 4:  // HeapRegionAddress
            return HeapRegionAddress();
        case 5:  // HeapRegionSize
            return HeapRegionSize();
        case 6:  // TotalMemoryAvailable
            return 0x10000000ull;
        case 7:  // TotalMemoryUsage
            return 0x10000ull;
        case 8:  // UsedMemorySize
            return 0;
        case 9:  // InitialProgramId
            return 1;
        case 10: // InitialMainThreadStackSize
            return 0x100000ull;
        case 11: // SystemResourceSize
            return 0x2000000ull;
        case 12: // InitialMainThreadHandle
            return kMainThreadHandle;
        case 13: // InitialMainThreadPriority
            return 0x2Cull;
        case 14: // InitialMainThreadCoreNum
            return 0;
        default:
            return 0;
    }
}

} // namespace

// svcGetInfo(uint64_t* out, InfoType type, Handle handle, uint64_t subtype)
//          -> result, info in X1.
void SvGetInfo(SyscallArgs& args, Memory& mem) {
    const auto type = static_cast<std::uint32_t>(args.x[1]);
    if (std::getenv("ANYSWITCH_TRACE_GETINFO"))
        std::fprintf(stderr, "  GetInfo type=%u handle=%llu subtype=%llu -> %llu\n",
                     type, static_cast<unsigned long long>(args.x[2]),
                     static_cast<unsigned long long>(args.x[3]),
                     static_cast<unsigned long long>(InfoValue(type)));
    const auto value = InfoValue(type);
    WriteU64(mem, args.x[0], value);
    args.x[0] = kResultSuccess;
    args.x[1] = value;
}

// svcGetProcessId(uint64_t* out, Handle) -> result, id in X1
void SvGetProcessId(SyscallArgs& args, Memory& mem) {
    constexpr std::uint64_t kProcessId = 1;
    WriteU64(mem, args.x[0], kProcessId);
    args.x[0] = kResultSuccess;
    args.x[1] = kProcessId;
}

// svcGetThreadId(uint64_t* out, Handle) -> result, id in X1
void SvGetThreadId(SyscallArgs& args, Memory& mem) {
    constexpr std::uint64_t kThreadId = 1;
    WriteU64(mem, args.x[0], kThreadId);
    args.x[0] = kResultSuccess;
    args.x[1] = kThreadId;
}

// svcGetSystemTick() -> ticks
void SvGetSystemTick(SyscallArgs& args, Memory&) { args.x[0] = 1; }

// svcOutputDebugString(const char* str, size_t len) -> result.
//
// The one syscall that produces host-visible output from guest data, so it is
// worth treating carefully: the payload is a NUL-terminated string whose length
// is the smaller of `len` and the distance to the terminator.
// Implemented by the guest runtime, so libkernel stays free of SDL2 and of
// any windowing dependency. Declared at global scope: an anonymous namespace
// would give it internal linkage and the definition would not resolve.
extern "C" void PresentGuestText(const char* text, std::size_t len);


void SvOutputDebugString(SyscallArgs& args, Memory& mem) {
    const auto addr = args.x[0];
    const auto len = static_cast<std::size_t>(args.x[1]);
    if (addr + len <= mem.size) {
        std::size_t n = 0;
        while (n < len && mem.data[addr + n] != 0)
            ++n;
        const auto* text = reinterpret_cast<const char*>(mem.data + addr);
        // Both destinations: a terminal for logs, the window for the demo.
        std::fwrite(text, 1, n, stdout);
        std::fwrite("\n", 1, 1, stdout);
        std::fflush(stdout);
        PresentGuestText(text, n);
    }
    args.x[0] = kResultSuccess;
}

// svcBreak(BreakReason, uintptr_t, size)
//
// This is libnx's panic path, so its arguments are the diagnostic when the
// guest gives up. The reason and the value it points at are both worth
// showing; a usable message pointer is not available, so only the raw values
// are printed.
void SvBreak(SyscallArgs& args, Memory&) {
    std::fprintf(stderr, "anyswitch: guest svcBreak reason=%llu arg=0x%llx size=%llu\n",
                 static_cast<unsigned long long>(args.x[0]),
                 static_cast<unsigned long long>(args.x[1]),
                 static_cast<unsigned long long>(args.x[2]));
    args.x[0] = kResultSuccess;
}

// svcCallSecureMonitor(SecureMonitorArguments* args)
//
// No trustzone here. Nothing to dispatch, so the guest is left believing the
// call completed.
void SvCallSecureMonitor(SyscallArgs&, Memory&) {}

// svcExitProcess()
void SvExitProcess(SyscallArgs&, Memory&) {
    std::fflush(stdout);
    std::exit(0);
}

} // namespace libkernel
