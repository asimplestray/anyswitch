#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <cstring>

namespace Elfpatcher {

std::vector<std::uint8_t> EntryStubBuilder::BuildEntryStub(std::uint64_t stubVaddr, std::uint64_t entryVaddr) {
        // x86-64 entry stub: ljmp to original entry point
        // movabs rax, <entry> ; jmp rax
        std::vector<std::uint8_t> stub;
        stub.push_back(0x48); // REX.W
        stub.push_back(0xB8); // mov rax, imm64
        std::uint64_t entry = entryVaddr;
        for (int i = 0; i < 8; ++i) stub.push_back(static_cast<std::uint8_t>((entry >> (i * 8)) & 0xFF));
        stub.push_back(0xFF); // jmp rax
        stub.push_back(0xE0);
        return stub;
}

}
