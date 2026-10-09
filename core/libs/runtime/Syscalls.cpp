#include "Syscalls.hpp"

#include "AnyswitchRuntime.hpp"
#include "remill/Arch/AArch64/Runtime/State.h"

#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <cstring>

namespace anyswitch {

namespace {

// Register-file layout measured against remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kGprOffset = 536;
constexpr std::size_t kRegStride = 16; // volatile uint64_t padding + Reg
constexpr std::size_t kRegX0 = kGprOffset + 8;

std::uint64_t ReadX(State& state, int n) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&state) + kRegX0 + n * kRegStride;
    std::uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

void WriteX(State& state, int n, std::uint64_t v) {
    auto* p = reinterpret_cast<std::uint8_t*>(&state) + kRegX0 + n * kRegStride;
    std::memcpy(p, &v, sizeof(v));
}

// Read/write guest memory through the Memory handle. Both are bounds-checked
// and simply fail closed, which keeps translated code running.
bool ReadGuest(Memory* mem, std::uint64_t addr, void* out, std::size_t len) {
    if (!mem || addr + len > mem->size)
        return false;
    std::memcpy(out, mem->data + addr, len);
    return true;
}

bool WriteGuest(Memory* mem, std::uint64_t addr, const void* in, std::size_t len) {
    if (!mem || addr + len > mem->size)
        return false;
    std::memcpy(mem->data + addr, in, len);
    return true;
}

bool WriteU64To(Memory* mem, std::uint64_t addr, std::uint64_t v) {
    return WriteGuest(mem, addr, &v, sizeof(v));
}

bool WriteU32To(Memory* mem, std::uint64_t addr, std::uint32_t v) {
    return WriteGuest(mem, addr, &v, sizeof(v));
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

// MemoryState / MemoryPermission values from the SwitchBrew SVC page.
constexpr std::uint32_t kMemStateNormal = 0;
constexpr std::uint32_t kMemPermReadWrite = 3;

// A fake but consistent handle space. Switch uses 32-bit handles where the low
// 16 bits index a table; real sessions are not modelled yet, so every handle
// this runtime invents is reserved and never confused with a guest's own.
constexpr std::uint32_t kFirstFakeHandle = 0x80000000u;
std::uint32_t g_nextHandle = kFirstFakeHandle;

std::uint32_t AllocHandle() { return g_nextHandle++; }

// --- Handlers ---------------------------------------------------------------

// svcSetHeapSize(uintptr_t* out, size_t size) -> result, heap address in X1.
void SvSetHeapSize(SyscallFrame& f, State&, Memory* mem) {
    // The kernel picks the base. It has to land inside the guest address space
    // the runtime actually owns, or every allocation the guest makes from it
    // would be silently dropped. Put it just past the loaded image, on a 2 MiB
    // boundary, which is the alignment SetHeapSize requires of the size.
    const auto imageEnd = mem ? GuestMemory::LoadedBytes() : 0;
    const auto heapBase = (imageEnd + 0x1FFFFFull) & ~0x1FFFFFull;
    if (imageEnd == 0 || heapBase >= (mem ? mem->size : 0)) {
        f.x[0] = 0xCA01; // invalid size
        return;
    }
    WriteU64To(mem, f.x[0], heapBase);
    f.x[0] = kResultSuccess;
    f.x[1] = heapBase;
}

// svcQueryMemory(MemoryInfo* out, PageInfo* page, uintptr_t addr)
//             -> result, PageInfo in W1.
void SvQueryMemory(SyscallFrame& f, State&, Memory* mem) {
    MemoryInfoLayout info{};
    info.addr = 0;
    info.size = mem ? mem->size : 0;
    info.state = kMemStateNormal;
    info.perm = kMemPermReadWrite;
    WriteGuest(mem, f.x[0], &info, sizeof(info));

    f.x[0] = kResultSuccess;
    // PageInfo: flags in W1, with bit0 set for "the query succeeded".
    f.x[1] = 1;
}

// svcOutputDebugString(const char* str, size_t len) -> result.
void SvOutputDebugString(SyscallFrame& f, State&, Memory* mem) {
    const auto addr = f.x[0];
    const auto len = static_cast<std::size_t>(f.x[1]);
    if (mem && addr + len <= mem->size) {
        // The payload is a NUL-terminated string; stop early if it is shorter.
        std::size_t n = 0;
        while (n < len && mem->data[addr + n] != 0)
            ++n;
        std::fwrite(mem->data + addr, 1, n, stdout);
        std::fwrite("\n", 1, 1, stdout);
        std::fflush(stdout);
    }
    f.x[0] = kResultSuccess;
}

// svcExitProcess()
void SvExitProcess(SyscallFrame&, State&, Memory*) {
    std::fflush(stdout);
    std::exit(0);
}

// svcGetInfo(uint64_t* out, InfoType type, Handle handle, uint64_t subtype)
//          -> result, info in X1.
void SvGetInfo(SyscallFrame& f, State& state, Memory* mem) {
    const auto type = static_cast<std::uint32_t>(f.x[1]);
    std::uint64_t value = 0;
    switch (type) {
        case 0:  // AllowedCpuIdBitmask
        case 1:  // AllowedThreadPrioBitmask
            value = 0xFFFFFFFFFFFFFFFFull;
            break;
        case 2:  // AliasRegionAddress
        case 3:  // AliasRegionSize
        case 4:  // HeapRegionAddress
        case 5:  // HeapRegionSize
            value = 0x10000000ull;
            break;
        case 6:  // TotalMemoryAvailable
        case 7:  // TotalMemoryUsage
            value = 0x1000000ull;
            break;
        case 8:  // UsedMemorySize
            value = 0;
            break;
        case 10: // InitialMainThreadStackSize
            value = 0x100000ull;
            break;
        default:
            value = 0;
            break;
    }
    WriteU64To(mem, f.x[0], value);
    f.x[0] = kResultSuccess;
    f.x[1] = value;
}

// svcWaitSynchronization(int32_t* outIndex, const Handle* handles,
//                        int32 numHandles, int64 timeout)
void SvWaitSynchronization(SyscallFrame& f, State&, Memory* mem) {
    // Nothing is modelled as signalled, so report the first handle as ready.
    // That keeps the guest's main loop spinning instead of deadlocking.
    const auto indexAddr = f.x[0];
    WriteU32To(mem, indexAddr, 0);
    f.x[0] = kResultSuccess;
    f.x[1] = 0; // handle index
}

// svcConnectToNamedPort(Handle* out, const char* name) -> result, handle in W1.
void SvConnectToNamedPort(SyscallFrame& f, State&, Memory* mem) {
    const auto handle = AllocHandle();
    WriteU32To(mem, f.x[0], handle);
    f.x[0] = kResultSuccess;
    f.x[1] = handle;
}

using Handler = void (*)(SyscallFrame&, State&, Memory*);

struct Entry {
    std::uint64_t svc;
    Handler handler;
    const char* name;
};

const Entry kHandlers[] = {
    {0x01, SvSetHeapSize, "SetHeapSize"},
    {0x02, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "SetMemoryPermission"},
    {0x03, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "SetMemoryAttribute"},
    {0x05, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "UnmapMemory"},
    {0x06, SvQueryMemory, "QueryMemory"},
    {0x07, SvExitProcess, "ExitProcess"},
    {0x08, [](SyscallFrame& f, State&, Memory* mem) {
         const auto handle = AllocHandle();
         WriteU32To(mem, f.x[0], handle);
         f.x[0] = kResultSuccess;
         f.x[1] = handle;
     }, "CreateThread"},
    {0x0A, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "ExitThread"},
    {0x0B, [](SyscallFrame&, State&, Memory*) {}, "SleepThread"},
    {0x10, [](SyscallFrame& f, State&, Memory*) { f.x[0] = 0; }, "GetCurrentProcessorNumber"},
    {0x16, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "CloseHandle"},
    {0x17, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "ResetSignal"},
    {0x18, SvWaitSynchronization, "WaitSynchronization"},
    {0x19, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "CancelSynchronization"},
    {0x1E, [](SyscallFrame& f, State&, Memory*) { f.x[0] = 1; }, "GetSystemTick"},
    {0x1F, SvConnectToNamedPort, "ConnectToNamedPort"},
    {0x20, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "SendSyncRequestLight"},
    {0x21, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "SendSyncRequest"},
    {0x22, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "SendSyncRequestWithUserBuffer"},
    {0x25, [](SyscallFrame& f, State&, Memory* mem) {
         const auto id = 1ull;
         WriteU64To(mem, f.x[0], id);
         f.x[0] = kResultSuccess;
         f.x[1] = id;
     }, "GetThreadId"},
    {0x26, [](SyscallFrame& f, State&, Memory* mem) {
         // svcBreak is how libnx reports a panic or a failed assertion. The
         // reason and the value it points at are the diagnostic, so surface
         // them instead of swallowing them. A usable message pointer is not
         // available here, so only the raw values are shown.
         std::fprintf(stderr, "anyswitch: guest svcBreak reason=%llu arg=0x%llx size=%llu\n",
                      static_cast<unsigned long long>(f.x[0]),
                      static_cast<unsigned long long>(f.x[1]),
                      static_cast<unsigned long long>(f.x[2]));
         (void)mem;
         f.x[0] = kResultSuccess;
     }, "Break"},
    {0x27, SvOutputDebugString, "OutputDebugString"},
    {0x29, SvGetInfo, "GetInfo"},
    {0x2A, [](SyscallFrame&, State&, Memory*) {}, "FlushEntireDataCache"},
    {0x2B, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "FlushDataCache"},
    {0x42, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "ReplyAndReceiveLight"},
    {0x45, [](SyscallFrame& f, State&, Memory* mem) {
         const auto h1 = AllocHandle();
         const auto h2 = AllocHandle();
         WriteU32To(mem, f.x[0], h1);
         WriteU32To(mem, f.x[1], h2);
         f.x[0] = kResultSuccess;
         f.x[1] = h1;
         f.x[2] = h2;
     }, "CreateEvent"},
    {0x52, [](SyscallFrame& f, State&, Memory*) { f.x[0] = kResultSuccess; }, "UnmapTransferMemory"},
    {0x7F, [](SyscallFrame&, State&, Memory*) {}, "CallSecureMonitor"},
};

} // namespace

bool HandleSyscall(std::uint64_t svc, SyscallFrame& frame, State& state, Memory* mem) {
    for (const auto& entry : kHandlers) {
        if (entry.svc != svc)
            continue;
        // Load the guest's argument registers into the frame, run the handler,
        // then write the result registers back.
        for (int i = 0; i < 6; ++i)
            frame.x[i] = ReadX(state, i);
        entry.handler(frame, state, mem);
        for (int i = 0; i < 6; ++i)
            WriteX(state, i, frame.x[i]);
        if (std::getenv("ANYSWITCH_TRACE_SYSCALLS"))
            std::fprintf(stderr, "svc #0x%02llx %s\n",
                         static_cast<unsigned long long>(svc), entry.name);
        return true;
    }
    return false;
}

} // namespace anyswitch
