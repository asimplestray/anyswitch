#pragma once

// Declarations of every syscall handler.
//
// Handlers live in the src/*.cpp files; this header is the single place they
// are declared. Adding one means adding its declaration here and its line in
// the dispatch table - both in one PR, so the two never drift.

#include "libkernel/SyscallAbi.hpp"

namespace libkernel {

// src/Memory.cpp - heap and address-space queries
void SvSetHeapSize(SyscallArgs& args, Memory& mem);
void SvQueryMemory(SyscallArgs& args, Memory& mem);
void SvSetMemoryPermission(SyscallArgs& args, Memory& mem);
void SvSetMemoryAttribute(SyscallArgs& args, Memory& mem);
void SvMapMemory(SyscallArgs& args, Memory& mem);
void SvUnmapTransferMemory(SyscallArgs& args, Memory& mem);
void SvUnmapMemory(SyscallArgs& args, Memory& mem);

// src/Handle.cpp - handles, threads and synchronisation
void SvCreateThread(SyscallArgs& args, Memory& mem);
void SvStartThread(SyscallArgs& args, Memory& mem);
void SvExitThread(SyscallArgs& args, Memory& mem);
void SvSleepThread(SyscallArgs& args, Memory& mem);
void SvGetCurrentProcessorNumber(SyscallArgs& args, Memory& mem);
void SvCloseHandle(SyscallArgs& args, Memory& mem);
void SvResetSignal(SyscallArgs& args, Memory& mem);
void SvWaitSynchronization(SyscallArgs& args, Memory& mem);
void SvCancelSynchronization(SyscallArgs& args, Memory& mem);
void SvCreateEvent(SyscallArgs& args, Memory& mem);
void SvReplyAndReceiveLight(SyscallArgs& args, Memory& mem);

// src/Info.cpp - identity and diagnostics
void SvGetInfo(SyscallArgs& args, Memory& mem);
void SvGetProcessId(SyscallArgs& args, Memory& mem);
void SvGetThreadId(SyscallArgs& args, Memory& mem);
void SvGetSystemTick(SyscallArgs& args, Memory& mem);
void SvOutputDebugString(SyscallArgs& args, Memory& mem);
void SvBreak(SyscallArgs& args, Memory& mem);
void SvConnectToNamedPort(SyscallArgs& args, Memory& mem);
void SvConnectToPort(SyscallArgs& args, Memory& mem);
void SvSendSyncRequestLight(SyscallArgs& args, Memory& mem);
void SvSendSyncRequest(SyscallArgs& args, Memory& mem);
void SvSendSyncRequestWithUserBuffer(SyscallArgs& args, Memory& mem);
void SvSendAsyncRequestWithUserBuffer(SyscallArgs& args, Memory& mem);
void SvCreateSession(SyscallArgs& args, Memory& mem);
void SvAcceptSession(SyscallArgs& args, Memory& mem);
void SvCreatePort(SyscallArgs& args, Memory& mem);
void SvManageNamedPort(SyscallArgs& args, Memory& mem);
void SvReplyAndReceive(SyscallArgs& args, Memory& mem);
void SvReplyAndReceiveLight(SyscallArgs& args, Memory& mem);

void SvExitProcess(SyscallArgs& args, Memory& mem);
void SvCallSecureMonitor(SyscallArgs& args, Memory& mem);
void ResetHandleSpace();

} // namespace libkernel
