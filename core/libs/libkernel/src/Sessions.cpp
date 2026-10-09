// IPC and session syscalls: how the guest talks to system services.
//
// Nothing here understands a service protocol yet. These handlers hand back a
// synthetic session so the guest can proceed with its startup; the reply data
// is empty, which is the next thing that has to become real.

#include "libkernel/Handlers.hpp"

#include "libkernel/SyscallAbi.hpp"

#include <cstdint>
#include <cstring>

namespace libkernel {
namespace {

// Session and port handles. Nothing is modelled yet, so every handle this
// runtime invents is reserved at the top of the space and can never collide
// with one the guest opens itself.
constexpr std::uint32_t kFirstFakeHandle = 0x80000000u;
std::uint32_t g_nextHandle = kFirstFakeHandle;

bool WriteU32(Memory& mem, std::uint64_t addr, std::uint32_t value) {
    if (addr + sizeof(value) > mem.size)
        return false;
    std::memcpy(mem.data + addr, &value, sizeof(value));
    return true;
}

std::uint32_t AllocHandle() { return g_nextHandle++; }

void Succeed(SyscallArgs& args) { args.x[0] = kResultSuccess; }

} // namespace

// Resets the synthetic handle space. Tests call this for deterministic handles.
void ResetHandleSpace() { g_nextHandle = kFirstFakeHandle; }

// svcConnectToNamedPort(Handle* out, const char* name) -> result, handle in W1.
void SvConnectToNamedPort(SyscallArgs& args, Memory& mem) {
    const auto handle = AllocHandle();
    WriteU32(mem, args.x[0], handle);
    Succeed(args);
    args.x[1] = handle;
}

// svcConnectToPort(Handle* out, Handle port) -> result, handle in W1.
void SvConnectToPort(SyscallArgs& args, Memory& mem) {
    const auto handle = AllocHandle();
    WriteU32(mem, args.x[0], handle);
    Succeed(args);
    args.x[1] = handle;
}

// svcSendSyncRequest(Handle session) -> result.
//
// The service would normally fill in a reply; with an empty one the guest sees
    // a malformed response, so this is where the wall sits until a real service
// lands.
void SvSendSyncRequest(SyscallArgs& args, Memory&) { Succeed(args); }

// svcSendSyncRequestLight(Handle session) -> result.
void SvSendSyncRequestLight(SyscallArgs& args, Memory&) { Succeed(args); }

// svcSendSyncRequestWithUserBuffer(uintptr_t, size, Handle) -> result.
void SvSendSyncRequestWithUserBuffer(SyscallArgs& args, Memory&) { Succeed(args); }

// svcSendAsyncRequestWithUserBuffer(Handle* out, uintptr_t, size, Handle)
//                                    -> result, event handle in W1.
void SvSendAsyncRequestWithUserBuffer(SyscallArgs& args, Memory& mem) {
    const auto handle = AllocHandle();
    WriteU32(mem, args.x[0], handle);
    Succeed(args);
    args.x[1] = handle;
}

// svcCreateSession(Handle* s, Handle* c, bool light, uintptr_t) -> result.
void SvCreateSession(SyscallArgs& args, Memory& mem) {
    const auto s = AllocHandle();
    const auto c = AllocHandle();
    WriteU32(mem, args.x[0], s);
    WriteU32(mem, args.x[1], c);
    Succeed(args);
    args.x[1] = s;
    args.x[2] = c;
}

// svcAcceptSession(Handle* out, Handle port) -> result, handle in W1.
void SvAcceptSession(SyscallArgs& args, Memory& mem) {
    const auto handle = AllocHandle();
    WriteU32(mem, args.x[0], handle);
    Succeed(args);
    args.x[1] = handle;
}

// svcCreatePort(Handle* s, Handle* c, int32 max, bool light, uintptr_t) -> result.
void SvCreatePort(SyscallArgs& args, Memory& mem) {
    const auto s = AllocHandle();
    const auto c = AllocHandle();
    WriteU32(mem, args.x[0], s);
    WriteU32(mem, args.x[1], c);
    Succeed(args);
    args.x[1] = s;
    args.x[2] = c;
}

// svcManageNamedPort(Handle* out, const char* name, int32 max) -> result, handle in W1.
void SvManageNamedPort(SyscallArgs& args, Memory& mem) {
    const auto handle = AllocHandle();
    WriteU32(mem, args.x[0], handle);
    Succeed(args);
    args.x[1] = handle;
}

// svcReplyAndReceive(int32* out, const Handle*, int32, Handle, int64) -> result.
void SvReplyAndReceive(SyscallArgs& args, Memory& mem) {
    WriteU32(mem, args.x[0], 0);
    Succeed(args);
    args.x[1] = 0;
}

// svcReplyAndReceiveLight(Handle) -> result.
void SvReplyAndReceiveLight(SyscallArgs& args, Memory&) { Succeed(args); }

} // namespace libkernel
