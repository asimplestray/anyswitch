// Execution harness: runs translated ARM64 code on the host and reports X0.
//
// Links against the object emitted by the relinker (ANYSWITCH_EMIT_OBJECT),
// resolves each lifted trace by symbol, and drives them through the runtime's
// trace-chaining loop rather than calling one block and stopping.
//
// There is no interpreter here: the code being called was already translated
// to x86-64 ahead of time. This only provides the register file, the guest
// address space, the hypercall seam, and the loop that threads control between
// translated blocks.
//
// ANYSWITCH_EMIT_TRACES supplies the guest-PC-to-symbol table the driver needs.

#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/libs/runtime/AnyswitchRuntime.hpp"
#include "core/libs/runtime/Driver.hpp"
#include "libkernel/SyscallAbi.hpp"

namespace {

// Register-file offsets measured against remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kStateGprOffset = 536;
constexpr std::size_t kGprX0Offset = 8;
constexpr std::size_t kStateX0Offset = kStateGprOffset + kGprX0Offset;

// Loads the PC -> symbol table the relinker emits.
class SymbolTraceMap : public anyswitch::TraceMap {
public:
    SymbolTraceMap(const char* tablePath) {
        std::ifstream in(tablePath);
        if (!in) {
            std::fprintf(stderr, "harness: cannot read trace table %s\n", tablePath);
            return;
        }
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            std::uint64_t pc = 0;
            std::string symbol;
            if (ls >> std::hex >> pc >> symbol) {
                // The object is linked into this executable with -rdynamic, so
                // its symbols are resolvable through the default handle.
                auto* fn = reinterpret_cast<anyswitch::TraceFunction>(
                    dlsym(RTLD_DEFAULT, symbol.c_str()));
                if (fn != nullptr)
                    _map.emplace(pc, fn);
            }
        }
    }

    std::size_t Size() const { return _map.size(); }

    anyswitch::TraceFunction Lookup(std::uint64_t guestPc) const override {
        const auto it = _map.find(guestPc);
        return it == _map.end() ? nullptr : it->second;
    }

private:
    std::unordered_map<std::uint64_t, anyswitch::TraceFunction> _map;
};

std::uint64_t ReadX0(const void* state) {
    std::uint64_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(state) + kStateX0Offset, sizeof(v));
    return v;
}

} // namespace

int main(int argc, char** argv) {
    const char* imagePath = nullptr;
    const char* tablePath = std::getenv("ANYSWITCH_TRACE_TABLE");
    if (argc > 1)
        imagePath = argv[1];
    if (argc > 2)
        tablePath = argv[2];

    if (tablePath == nullptr) {
        std::fprintf(stderr, "harness: no trace table (set ANYSWITCH_TRACE_TABLE)\n");
        return 2;
    }

    // A guest address space big enough for the image, its heap and a working
    // set. The console gives a process far more than this, but it is the order
    // of magnitude a small homebrew needs.
    const std::size_t memSize = 512u << 20; // 512 MiB
    anyswitch::GuestMemory mem(memSize, 0);

    std::vector<std::uint8_t> image;
    if (imagePath != nullptr) {
        std::FILE* f = std::fopen(imagePath, "rb");
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
        std::fprintf(stderr, "harness: guest image does not fit the address space\n");
    else if (!image.empty())
        libkernel::SetLoadedImageBytes(image.size());

    // The kernel hands a thread its TLS block through TPIDR_EL0. Both homebrew
    // binaries read it on every TLS access, so leaving it zero means every
    // derived pointer is null. Point it at a zeroed area the heap does not
    // overlap.
    // The kernel hands a process its stack, its TLS block, and a thread
    // pointer. The console sets all three before jumping to the entry point;
    // leaving them zero makes every stack and TLS access compute a negative
    // address, which is exactly the shape of the panics the guest was hitting.
    constexpr std::uint64_t kTcbAddress = 0x8000000ull;        // 128 MiB
    constexpr std::uint64_t kStackTop = 0x10000000ull;         // 256 MiB
    constexpr std::size_t kStateTpidrEl0Offset = 1112;
    // Measured against remill/Arch/AArch64/Runtime/State.h.
    constexpr std::size_t kStateSpOffset = 1040;
    constexpr std::size_t kStateLrOffset = 1024;               // GPR.x30

    // Remill's State is a padded register file; zero it so flags start clean.
    std::vector<std::uint8_t> stateBytes(1200 + 64, 0);
    auto* state = reinterpret_cast<State*>(stateBytes.data());
    std::memcpy(stateBytes.data() + kStateTpidrEl0Offset, &kTcbAddress,
                sizeof(kTcbAddress));
    std::memcpy(stateBytes.data() + kStateSpOffset, &kStackTop, sizeof(kStackTop));
    const std::uint64_t lr = 0; // no return address: the entry point is the root
    std::memcpy(stateBytes.data() + kStateLrOffset, &lr, sizeof(lr));

    SymbolTraceMap traces(tablePath);
    if (traces.Size() == 0) {
        std::fprintf(stderr, "harness: no traces resolved from %s\n", tablePath);
        return 2;
    }

    anyswitch::DriveLimits limits;
    // A guest that never terminates should not run the machine dry; drop the
    // ceiling right down for a diagnostic run but leave it generous normally.
    if (const char* env = std::getenv("ANYSWITCH_MAX_TRACES"))
        limits.maxTraces = std::strtoull(env, nullptr, 10);

    std::uint64_t exitPc = 0;
    const auto finished = anyswitch::Drive(*state, mem.Handle(), 0, traces, limits, exitPc);

    std::printf("X0=%llu\n", static_cast<unsigned long long>(ReadX0(stateBytes.data())));
    if (!finished)
        std::fprintf(stderr, "harness: stopped at guest PC 0x%llx (%zu traces mapped)\n",
                     static_cast<unsigned long long>(exitPc), traces.Size());
    return 0;
}
