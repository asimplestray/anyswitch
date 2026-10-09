// Execution harness: runs translated ARM64 code on the host and reports X0.
//
// Links against the object emitted by the relinker (ANYSWITCH_EMIT_OBJECT)
// plus the native runtime, allocates the guest State and Memory, calls the
// entry trace, and prints the resulting X0. The guest program is expected to
// place its result in X0 before RET.
//
// There is no interpreter here: the code being called was already translated
// to x86-64 ahead of time. This only provides the register file, the guest
// address space, and the hypercall seam Remill's semantics call into.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/libs/runtime/AnyswitchRuntime.hpp"

// The entry trace, produced by the relinker and named asw_trace_<guest addr>.
extern "C" void* asw_trace_0(void* state, unsigned long pc, void* memory);

namespace {

// Register-file offsets measured from remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kStateGprOffset = 536;
constexpr std::size_t kGprX0Offset = 8;

std::uint64_t ReadX0(const void* state) {
    const auto* gpr = static_cast<const std::uint8_t*>(state) + kStateGprOffset;
    std::uint64_t v = 0;
    std::memcpy(&v, gpr + kGprX0Offset, sizeof(v));
    return v;
}

} // namespace

int main(int argc, char** argv) {
    // The guest image (.text) is mapped at base 0 so the runtime can read
    // instructions from the PC it is handed - that is how an SVC is
    // recognised and dispatched. argv[1] optionally overrides the image.
    // A guest address space big enough for the image, its heap and a working
    // set. The console gives a process far more than this, but it is the order
    // of magnitude a small homebrew needs.
    // The reported heap region sits at 256 MiB, so the address space has to
    // cover it: image + heap + working set.
    const std::size_t memSize = 512u << 20; // 512 MiB
    anyswitch::GuestMemory mem(memSize, 0);

    std::vector<std::uint8_t> image;
    if (argc > 1) {
        std::FILE* f = std::fopen(argv[1], "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END);
            const long n = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            image.resize(static_cast<std::size_t>(n));
            const std::size_t got = std::fread(image.data(), 1, image.size(), f);
            image.resize(got);
            std::fclose(f);
        }
    }

    // Map the guest image at base 0 so the runtime can read instructions from
    // the PCs it is handed; that is how an SVC is recognised and dispatched.
    if (!image.empty() && !mem.Write(0, image.data(), image.size()))
        std::fprintf(stderr, "warning: guest image does not fit the address space\n");

    // Remill's State is a padded register file; zero it so flags start clean.
    std::vector<std::uint8_t> stateBytes(1200 + 64, 0);

    auto* state = stateBytes.data();
    auto* result = asw_trace_0(state, 0, mem.Handle());

    std::printf("X0=%llu\n", static_cast<unsigned long long>(ReadX0(state)));
    if (!result)
        std::fprintf(stderr, "warning: trace returned null memory handle\n");
    return 0;
}
