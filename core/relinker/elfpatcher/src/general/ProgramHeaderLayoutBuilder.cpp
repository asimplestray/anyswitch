#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elf.h>

namespace Elfpatcher {

std::uint16_t ProgramHeaderLayoutBuilder::WriteLayout(std::vector<std::uint8_t>& buf, const ProgramHeaderLayoutRequest& req) {
    std::uint16_t phNum = static_cast<std::uint16_t>(req.OriginalHeaders.size());
    std::uint64_t phOff = req.PhOff;

    // Write original program headers
    for (const auto& ph : req.OriginalHeaders) {
        if (ph.Type == PT_LOAD) {
            std::uint32_t p_type = ph.Type;
            std::uint32_t p_flags = ph.Flags;
            std::uint64_t p_offset = ph.Offset;
            std::uint64_t p_vaddr = ph.MappedAddress;
            std::uint64_t p_paddr = ph.PhysicalAddress;
            std::uint64_t p_filesz = ph.FileSize;
            std::uint64_t p_memsz = ph.MemorySize;
            std::uint64_t p_align = ph.Alignment;

            auto writeU32 = [&buf, &phOff](std::uint32_t val) {
                for (int i = 0; i < 4; ++i)
                    buf[phOff + i] = static_cast<std::uint8_t>((val >> (i * 8)) & 0xFF);
                phOff += 4;
            };
            auto writeU64 = [&buf, &phOff](std::uint64_t val) {
                for (int i = 0; i < 8; ++i)
                    buf[phOff + i] = static_cast<std::uint8_t>((val >> (i * 8)) & 0xFF);
                phOff += 8;
            };

            writeU32(p_type);
            writeU32(p_flags);
            writeU64(p_offset);
            writeU64(p_vaddr);
            writeU64(p_paddr);
            writeU64(p_filesz);
            writeU64(p_memsz);
            writeU64(p_align);
        } else {
            phOff += req.PhEntSize;
        }
    }

    // Add extra LOAD segment for the translated code
    // (simplified — real impl maps the extra block)
    return phNum;
}

std::uint64_t ProgramHeaderLayoutBuilder::ComputeExtraBlockVaddr(const std::vector<Domain::ProgramHeader>& headers, std::uint64_t extraBlockOffset) {
    for (const auto& ph : headers) {
        if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0) {
            std::uint64_t pageOff = ph.Offset & ~(ph.Alignment - 1);
            std::uint64_t pageVaddr = ph.MappedAddress & ~(ph.Alignment - 1);
            return pageVaddr + (extraBlockOffset - pageOff);
        }
    }
    return 0;
}

}
