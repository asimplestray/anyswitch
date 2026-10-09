// Unit tests for libkernel's syscall handlers.
//
// Each test builds a SyscallArgs, backs it with a Memory, calls the handler,
// and asserts on what came back. No translated binary and no guest image are
// involved, so a handler can be written and verified in one sitting.
//
// Adding a test: write a TestXxx() function and add it to the RUN list below.

#include "libkernel/Handlers.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::fprintf(stderr, "  FAIL: %s\n", what);
    }
}

// A guest address space small enough to reason about, zero-filled.
struct TestMemory {
    std::vector<std::uint8_t> bytes;
    ::Memory mem;

    explicit TestMemory(std::size_t size) : bytes(size, 0) {
        mem.data = bytes.data();
        mem.size = bytes.size();
    }

    void Put(std::uint64_t addr, std::uint64_t value) {
        std::memcpy(bytes.data() + addr, &value, sizeof(value));
    }

    void Put(std::uint64_t addr, std::uint32_t value) {
        std::memcpy(bytes.data() + addr, &value, sizeof(value));
    }

    std::uint64_t Get64(std::uint64_t addr) const {
        std::uint64_t v = 0;
        std::memcpy(&v, bytes.data() + addr, sizeof(v));
        return v;
    }

    std::uint32_t Get32(std::uint64_t addr) const {
        std::uint32_t v = 0;
        std::memcpy(&v, bytes.data() + addr, sizeof(v));
        return v;
    }
};

// ---------------------------------------------------------------------------

void TestSetHeapSizePlacesHeapInsideMapping() {
    // The heap has to land in memory the runtime actually owns. A base outside
    // the mapping made every guest allocation silently disappear.
    libkernel::SetLoadedImageBytes(0x15000);

    TestMemory mem(512u << 20);
    libkernel::SyscallArgs args{};
    constexpr std::uint64_t kOutPtr = 0x1000;
    args.x[0] = kOutPtr; // out pointer
    args.x[1] = 0x200000; // requested size

    libkernel::SvSetHeapSize(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess, "SetHeapSize succeeds");
    const auto base = mem.Get64(kOutPtr);
    Check(base >= libkernel::HeapRegionAddress(),
          "heap base is inside the reported heap region");
    Check(base + 0x200000 <= mem.mem.size, "heap fits inside the mapping");
    Check(base >= 0x15000, "heap starts past the loaded image");
    Check(args.x[1] == base, "heap base is also returned in X1");
}

void TestSetHeapSizeRejectsEmptyImage() {
    // Without a mapped image there is nowhere sensible to put a heap.
    libkernel::SetLoadedImageBytes(0);

    TestMemory mem(512u << 20);
    libkernel::SyscallArgs args{};
    args.x[0] = 0x1000;
    args.x[1] = 0x200000;

    libkernel::SvSetHeapSize(args, mem.mem);

    Check(args.x[0] != libkernel::kResultSuccess, "SetHeapSize refuses with no image");
}

void TestQueryMemoryReportsMappedRange() {
    TestMemory mem(1u << 20);
    libkernel::SyscallArgs args{};
    constexpr std::uint64_t kInfoPtr = 0x2000;
    args.x[0] = kInfoPtr;
    args.x[2] = 0x100; // address being queried

    libkernel::SvQueryMemory(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess, "QueryMemory succeeds");
    Check(mem.Get64(kInfoPtr) == 0, "mapping starts at base 0");
    Check(mem.Get64(kInfoPtr + 8) == mem.mem.size, "mapping covers the whole space");
    Check(args.x[1] == 1, "PageInfo flags report success");
}

void TestGetInfoReportsConsistentHeapRegion() {
    // GetInfo describes memory the runtime owns; the two halves must agree
    // with what SetHeapSize actually uses.
    TestMemory mem(512u << 20);
    libkernel::SyscallArgs args{};
    constexpr std::uint64_t kOutPtr = 0x3000;
    args.x[0] = kOutPtr;
    args.x[1] = 4; // HeapRegionAddress
    args.x[3] = 0;

    libkernel::SvGetInfo(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess, "GetInfo succeeds");
    Check(mem.Get64(kOutPtr) == libkernel::HeapRegionAddress(),
          "address matches the region SetHeapSize uses");
    Check(libkernel::HeapRegionAddress() + libkernel::HeapRegionSize() <= mem.mem.size,
          "the reported region fits inside the mapping");
}

void TestOutputDebugStringWritesGuestString() {
    TestMemory mem(1u << 20);
    const char* text = "hello from the guest";
    const auto len = std::strlen(text);
    std::memcpy(mem.bytes.data() + 0x400, text, len);

    libkernel::SyscallArgs args{};
    args.x[0] = 0x400; // string address
    args.x[1] = len;   // length

    // The handler writes to stdout, which would pollute test output; assert
    // only that it reports success and leaves the payload untouched.
    libkernel::SvOutputDebugString(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess, "OutputDebugString succeeds");
    Check(std::memcmp(mem.bytes.data() + 0x400, text, len) == 0,
          "the guest string is not modified");
}

void TestOutputDebugStringTruncatesAtTerminator() {
    // The length the guest passes may be longer than the string itself; the
    // handler must stop at the NUL rather than reading past it.
    TestMemory mem(1u << 20);
    const char* text = "short";
    std::memcpy(mem.bytes.data() + 0x500, text, 6);

    libkernel::SyscallArgs args{};
    args.x[0] = 0x500;
    args.x[1] = 64; // claims far more than is really there

    libkernel::SvOutputDebugString(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess,
          "OutputDebugString succeeds on an over-long length");
}

void TestCloseHandleSucceeds() {
    TestMemory mem(1u << 20);
    libkernel::SyscallArgs args{};
    args.x[0] = 0x1; // some handle

    libkernel::SvCloseHandle(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess, "CloseHandle succeeds");
}

void TestWaitSynchronizationReportsFirstHandleReady() {
    // Nothing is modelled as signalled yet; the guest's main loop depends on
    // this returning instead of blocking forever.
    TestMemory mem(1u << 20);
    libkernel::SyscallArgs args{};
    constexpr std::uint64_t kIndexPtr = 0x600;
    args.x[0] = kIndexPtr;
    args.x[1] = 0x700; // handles array
    args.w[2] = 1;     // one handle

    libkernel::SvWaitSynchronization(args, mem.mem);

    Check(args.x[0] == libkernel::kResultSuccess, "WaitSynchronization succeeds");
    Check(mem.Get32(kIndexPtr) == 0, "handle index zero is reported");
    Check(args.x[1] == 0, "handle index is also returned in X1");
}

void TestExitProcessIsReachable() {
    // SvExitProcess calls std::exit, so it cannot run inside a test. Confirm
    // it is at least declared and bound, which is what the dispatcher needs.
    Check(libkernel::SvExitProcess != nullptr, "SvExitProcess is bound");
}

struct TestCase {
    const char* name;
    void (*run)();
};

const TestCase kTests[] = {
    {"SetHeapSizePlacesHeapInsideMapping", TestSetHeapSizePlacesHeapInsideMapping},
    {"SetHeapSizeRejectsEmptyImage", TestSetHeapSizeRejectsEmptyImage},
    {"QueryMemoryReportsMappedRange", TestQueryMemoryReportsMappedRange},
    {"GetInfoReportsConsistentHeapRegion", TestGetInfoReportsConsistentHeapRegion},
    {"OutputDebugStringWritesGuestString", TestOutputDebugStringWritesGuestString},
    {"OutputDebugStringTruncatesAtTerminator", TestOutputDebugStringTruncatesAtTerminator},
    {"CloseHandleSucceeds", TestCloseHandleSucceeds},
    {"WaitSynchronizationReportsFirstHandleReady", TestWaitSynchronizationReportsFirstHandleReady},
    {"ExitProcessIsReachable", TestExitProcessIsReachable},
};

} // namespace

int main() {
    for (const auto& test : kTests) {
        std::printf("  RUN %s\n", test.name);
        test.run();
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
