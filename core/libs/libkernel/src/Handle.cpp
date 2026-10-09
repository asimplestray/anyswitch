// Handle and process-lifecycle syscalls: creating, closing and waiting on
// kernel objects.
//
// Nothing is modelled yet - these handlers only need to give the guest
// plausible answers so it can keep running. A synthetic handle space keeps
// every handle this runtime invents distinct from one the guest opens itself.

#include "libkernel/Handlers.hpp"

#include "libkernel/SyscallAbi.hpp"

#include <cstdint>
#include <cstring>

namespace libkernel {
namespace {

// Switch handles are 32-bit with the low bits indexing a table. Real objects
// are not modelled yet, so every handle invented here is reserved at the top
// and can never be confused with a guest's own.
constexpr std::uint32_t kFirstFakeHandle = 0x80000000u;
std::uint32_t g_nextHandle = kFirstFakeHandle;

std::uint32_t AllocHandle() { return g_nextHandle++; }

bool WriteU32(Memory& mem, std::uint64_t addr, std::uint32_t value) {
    if (addr + sizeof(value) > mem.size)
        return false;
    std::memcpy(mem.data + addr, &value, sizeof(value));
    return true;
}

void Succeed(SyscallArgs& args) { args.x[0] = kResultSuccess; }

} // namespace

// svcCloseHandle(Handle handle) -> result
void SvCloseHandle(SyscallArgs& args, Memory&) { Succeed(args); }

// svcResetSignal(Handle handle) -> result
void SvResetSignal(SyscallArgs& args, Memory&) { Succeed(args); }

// svcWaitSynchronization(int32_t* outIndex, const Handle* handles,
//                        int32 numHandles, int64 timeout) -> result
void SvWaitSynchronization(SyscallArgs& args, Memory& mem) {
    // Nothing is modelled as signalled, so report the first handle as ready.
    // That keeps the guest's main loop spinning instead of deadlocking.
    WriteU32(mem, args.x[0], 0);
    Succeed(args);
    args.x[1] = 0; // handle index
}

// svcCancelSynchronization(Handle thread_handle) -> result
void SvCancelSynchronization(SyscallArgs& args, Memory&) { Succeed(args); }

// svcCreateThread(...) -> result, handle in W1
void SvCreateThread(SyscallArgs& args, Memory& mem) {
    const auto handle = AllocHandle();
    WriteU32(mem, args.x[0], handle);
    Succeed(args);
    args.x[1] = handle;
}

// svcStartThread(Handle) ; svcExitThread ; svcSleepThread — no thread model.
void SvStartThread(SyscallArgs& args, Memory&) { Succeed(args); }
void SvExitThread(SyscallArgs& args, Memory&) { Succeed(args); }
void SvSleepThread(SyscallArgs&, Memory&) {}

// svcCreateEvent(Handle* w, Handle* r) -> result, two handles in W1/W2.
void SvCreateEvent(SyscallArgs& args, Memory& mem) {
    const auto w = AllocHandle();
    const auto r = AllocHandle();
    WriteU32(mem, args.x[0], w);
    WriteU32(mem, args.x[1], r);
    Succeed(args);
    args.x[1] = w;
    args.x[2] = r;
}

// svcGetCurrentProcessorNumber() -> cpu id
void SvGetCurrentProcessorNumber(SyscallArgs& args, Memory&) { args.x[0] = 0; }

} // namespace libkernel
