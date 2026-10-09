// The syscall table: the one place a syscall number is bound to a handler.
//
// Adding a syscall means implementing the handler in one of the src/*.cpp
// files and adding exactly one line here. Nothing else in the project changes.

#include "libkernel/Dispatch.hpp"
#include "libkernel/Handlers.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace libkernel {
namespace {

struct Entry {
    std::uint64_t svc;
    SyscallHandler handler;
    const char* name;
};

// Numbers and names follow https://switchbrew.org/wiki/SVC
const Entry kTable[] = {
    // --- Memory ------------------------------------------------------------
    {0x01, SvSetHeapSize, "SetHeapSize"},
    {0x02, SvSetMemoryPermission, "SetMemoryPermission"},
    {0x03, SvSetMemoryAttribute, "SetMemoryAttribute"},
    {0x04, SvMapMemory, "MapMemory"},
    {0x05, SvUnmapMemory, "UnmapMemory"},
    {0x52, SvUnmapTransferMemory, "UnmapTransferMemory"},
    {0x06, SvQueryMemory, "QueryMemory"},

    // --- Handles, threads, synchronisation ---------------------------------
    {0x08, SvCreateThread, "CreateThread"},
    {0x09, SvStartThread, "StartThread"},
    {0x0A, SvExitThread, "ExitThread"},
    {0x0B, SvSleepThread, "SleepThread"},
    {0x10, SvGetCurrentProcessorNumber, "GetCurrentProcessorNumber"},
    {0x16, SvCloseHandle, "CloseHandle"},
    {0x17, SvResetSignal, "ResetSignal"},
    {0x18, SvWaitSynchronization, "WaitSynchronization"},
    {0x19, SvCancelSynchronization, "CancelSynchronization"},
    {0x45, SvCreateEvent, "CreateEvent"},
    {0x42, SvReplyAndReceiveLight, "ReplyAndReceiveLight"},

    // --- Identity and diagnostics ------------------------------------------
    {0x1E, SvGetSystemTick, "GetSystemTick"},
    {0x24, SvGetProcessId, "GetProcessId"},
    {0x25, SvGetThreadId, "GetThreadId"},
    {0x26, SvBreak, "Break"},
    {0x27, SvOutputDebugString, "OutputDebugString"},
    {0x29, SvGetInfo, "GetInfo"},
    {0x2A, nullptr, "FlushEntireDataCache"},
    {0x2B, SvSetMemoryAttribute, "FlushDataCache"},
    {0x7F, SvCallSecureMonitor, "CallSecureMonitor"},

    // --- Sessions and ports (IPC) ------------------------------------------
    {0x1F, SvConnectToNamedPort, "ConnectToNamedPort"},
    {0x20, SvSendSyncRequestLight, "SendSyncRequestLight"},
    {0x21, SvSendSyncRequest, "SendSyncRequest"},
    {0x22, SvSendSyncRequestWithUserBuffer, "SendSyncRequestWithUserBuffer"},
    {0x23, SvSendAsyncRequestWithUserBuffer, "SendAsyncRequestWithUserBuffer"},
    {0x40, SvCreateSession, "CreateSession"},
    {0x41, SvAcceptSession, "AcceptSession"},
    {0x42, SvReplyAndReceiveLight, "ReplyAndReceiveLight"},
    {0x43, SvReplyAndReceive, "ReplyAndReceive"},
    {0x70, SvCreatePort, "CreatePort"},
    {0x71, SvManageNamedPort, "ManageNamedPort"},
    {0x72, SvConnectToPort, "ConnectToPort"},
};

} // namespace

bool Dispatch(std::uint64_t svc, SyscallArgs& args, Memory& mem) {
    for (const auto& entry : kTable) {
        if (entry.svc != svc)
            continue;
        if (entry.handler == nullptr)
            return true; // recognised and intentionally a no-op
        // Trace of what the guest actually asked for. Opt-in so normal runs
        // stay quiet, but it is the diagnostic to reach for when a guest stops
        // making progress.
        if (std::getenv("ANYSWITCH_TRACE_SYSCALLS"))
            std::fprintf(stderr, "svc #0x%02llx %s\n",
                         static_cast<unsigned long long>(svc), entry.name);
        entry.handler(args, mem);
        return true;
    }
    return false;
}

} // namespace libkernel
