// Execution harness with presentation: runs translated ARM64 code and shows
// what the guest produces.
//
// The guest is linked as a PNG surface alongside the runtime, so this drives
// the whole thing: load the trace table, chain traces, service syscalls, and
// put guest output on screen.

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
#include "core/libs/runtime/Syscalls.hpp"
#include "core/libs/runtime/Presentation.hpp"
#include "libkernel/SyscallAbi.hpp"

namespace {

// Register-file offsets measured against remill/Arch/AArch64/Runtime/State.h.
constexpr std::size_t kStateGprOffset = 536;
constexpr std::size_t kGprX0Offset = 8;
constexpr std::size_t kStateX0Offset = kStateGprOffset + kGprX0Offset;

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

    const std::size_t memSize = 512u << 20;
    anyswitch::GuestMemory mem(memSize, 0);

    // The guest image maps every segment at its guest offset: the code the
    // traces call, and the rodata/data the guest reads through pointers.
    // Mapping .text alone left literals zero, so a guest writing a string from
    // rodata produced an empty line instead of the string.
    if (imagePath != nullptr) {
        std::FILE* f = std::fopen(imagePath, "rb");
        if (f == nullptr) {
            std::fprintf(stderr, "harness: cannot open guest image %s\n", imagePath);
            return 2;
        }
        std::vector<std::uint8_t> nro;
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        nro.resize(static_cast<std::size_t>(sz));
        const auto got = std::fread(nro.data(), 1, nro.size(), f);
        nro.resize(got);
        std::fclose(f);

        // NRO header: segments are contiguous from offset 0x80, in the order
        // text, rodata, data, at their memory offsets.
        std::size_t maxMapped = 0;
        std::size_t fileCursor = 0x80;
        for (int seg = 0; seg < 3; ++seg) {
            const auto fieldOff = 0x20 + seg * 8; // (memOffset, size) pairs at 0x20 / 0x28 / 0x30
            if (fieldOff + 8 > nro.size())
                break;
            std::uint32_t memOff = 0;
            std::uint32_t segSize = 0;
            std::memcpy(&memOff, nro.data() + fieldOff, 4);
            std::memcpy(&segSize, nro.data() + fieldOff + 4, 4);
            if (fileCursor + segSize > nro.size())
                break;
            if (segSize > 0) {
                if (!mem.Write(memOff, nro.data() + fileCursor, segSize)) {
                    std::fprintf(stderr, "harness: segment %d does not fit\n", seg);
                    return 2;
                }
                maxMapped = std::max<std::size_t>(maxMapped,
                                                  static_cast<std::size_t>(memOff) + segSize);
            }
            fileCursor += segSize;
        }
        if (maxMapped > 0)
            libkernel::SetLoadedImageBytes(maxMapped);
    }

    // The kernel hands a process its stack, its TLS block, and a thread
    // pointer. Offsets measured against the Remill runtime header.
    constexpr std::uint64_t kTcbAddress = 0x8000000ull;
    constexpr std::uint64_t kStackTop = 0x10000000ull;
    constexpr std::size_t kStateTpidrEl0Offset = 1112;
    constexpr std::size_t kStateSpOffset = 1040;
    constexpr std::size_t kStateLrOffset = 1024;

    std::vector<std::uint8_t> stateBytes(1200 + 64, 0);
    std::memcpy(stateBytes.data() + kStateTpidrEl0Offset, &kTcbAddress,
                sizeof(kTcbAddress));
    std::memcpy(stateBytes.data() + kStateSpOffset, &kStackTop, sizeof(kStackTop));
    const std::uint64_t lr = 0;
    std::memcpy(stateBytes.data() + kStateLrOffset, &lr, sizeof(lr));
    auto* state = reinterpret_cast<State*>(stateBytes.data());

    // A window so guest output is visible, not just logged. ANYSWITCH_WINDOW=0
    // keeps a headless run (CI) quiet.
    bool showWindow = std::getenv("ANYSWITCH_WINDOW") != nullptr &&
                      std::string(std::getenv("ANYSWITCH_WINDOW")) != "0";
    if (showWindow) {
        anyswitch::Presentation::Config cfg;
        if (!anyswitch::Presentation::Get().Init(cfg))
            showWindow = false; // no display (CI, headless box): keep going
    }

    SymbolTraceMap traces(tablePath);
    if (traces.Size() == 0) {
        std::fprintf(stderr, "harness: no traces resolved from %s\n", tablePath);
        return 2;
    }

    anyswitch::DriveLimits limits;
    if (const char* env = std::getenv("ANYSWITCH_MAX_TRACES"))
        limits.maxTraces = std::strtoull(env, nullptr, 10);

    // libkernel services the syscall; the driver just hands it over.
    auto onSyscall = [](State& st, std::uint64_t svc, Memory* m) -> bool {
        anyswitch::SyscallFrame frame{};
        return anyswitch::HandleSyscall(svc, frame, st, m);
    };

    std::uint64_t exitPc = 0;
    const auto finished = anyswitch::Drive(*state, mem.Handle(), 0, traces, limits,
                                           exitPc, onSyscall);

    std::uint64_t x0 = 0;
    std::memcpy(&x0, stateBytes.data() + kStateX0Offset, sizeof(x0));
    std::printf("X0=%llu\n", static_cast<unsigned long long>(x0));
    if (showWindow)
        anyswitch::Presentation::Get().PollEvents();
    if (!finished)
        std::fprintf(stderr, "harness: stopped at guest PC 0x%llx (%zu traces mapped)\n",
                     static_cast<unsigned long long>(exitPc), traces.Size());
    return 0;
}
