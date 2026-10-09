#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elf.h>

namespace Elfpatcher {

namespace {

void WriteU32(std::vector<std::uint8_t>& buf, std::uint64_t& off, std::uint32_t v) {
    if (off + 4 > buf.size()) buf.resize(off + 4, 0);
    for (int i = 0; i < 4; ++i)
        buf[off++] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
}

void WriteU64(std::vector<std::uint8_t>& buf, std::uint64_t& off, std::uint64_t v) {
    if (off + 8 > buf.size()) buf.resize(off + 8, 0);
    for (int i = 0; i < 8; ++i)
        buf[off++] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
}

void WritePhdr(std::vector<std::uint8_t>& buf, std::uint64_t& off,
               std::uint32_t type, std::uint32_t flags,
               std::uint64_t offset, std::uint64_t vaddr,
               std::uint64_t filesz, std::uint64_t memsz,
               std::uint64_t align) {
    WriteU32(buf, off, type);
    WriteU32(buf, off, flags);
    WriteU64(buf, off, offset);
    WriteU64(buf, off, vaddr);
    WriteU64(buf, off, vaddr); // p_paddr
    WriteU64(buf, off, filesz);
    WriteU64(buf, off, memsz);
    WriteU64(buf, off, align);
}

} // namespace

std::uint16_t ProgramHeaderLayoutBuilder::WriteLayout(std::vector<std::uint8_t>& buf, const ProgramHeaderLayoutRequest& req) {
    std::uint16_t phNum = static_cast<std::uint16_t>(req.OriginalHeaders.size());
    std::uint64_t phOff = req.PhOff;

    // Write original program headers
    for (const auto& ph : req.OriginalHeaders) {
        if (ph.Type == PT_LOAD) {
            WritePhdr(buf, phOff, ph.Type, ph.Flags, ph.Offset, ph.MappedAddress,
                      ph.FileSize, ph.MemorySize, ph.Alignment);
        } else {
            phOff += req.PhEntSize;
        }
    }

    // Add extra LOAD segments for the extra block.
    // The extra block contains: [translated code] [dynstr] [dynsym] [rela] [rela.plt] [dynamic] [stub] [interp]
    // We split it into:
    //   1. RX segment: translated code (from extra block start to dynamic start)
    //   2. RW segment: dynamic section + dynstr/dynsym/rela/rela.plt
    //   3. RX segment: stub + interp (from dynamic end to extra block end)
    if (req.ExtraBlockSize > 0) {
        const std::uint64_t pageAlign = kDefaultLoadAlignment;

        const std::uint64_t rxOff = req.ExtraBlockOffset;
        const std::uint64_t rxVaddr = req.ExtraBlockVaddr;
        const std::uint64_t rwOff = req.DynamicSegmentOffset;
        const std::uint64_t rwVaddr = req.ExtraBlockVaddr + (rwOff - req.ExtraBlockOffset);

        // RX segment: translated code (from extra block start to dynamic start)
        const std::uint64_t rxFilesz = rwOff > rxOff ? rwOff - rxOff : 0;
        if (rxFilesz > 0) {
            WritePhdr(buf, phOff, PT_LOAD, PF_R | PF_X, rxOff, rxVaddr,
                      rxFilesz, rxFilesz, pageAlign);
            phNum++;
        }

        // RW segment (dynamic section + data)
        if (req.DynamicSegmentSize > 0) {
            WritePhdr(buf, phOff, PT_LOAD, PF_R | PF_W, rwOff, rwVaddr,
                      req.DynamicSegmentSize, req.DynamicSegmentSize, pageAlign);
            phNum++;
        }

        // RX segment: stub + interp (from dynamic end to extra block end)
        const std::uint64_t dynEndOff = rwOff + req.DynamicSegmentSize;
        const std::uint64_t extraEndOff = req.ExtraBlockOffset + req.ExtraBlockSize;
        const std::uint64_t stubInterpSz = extraEndOff > dynEndOff ? extraEndOff - dynEndOff : 0;
        if (stubInterpSz > 0) {
            const std::uint64_t stubInterpVaddr = req.ExtraBlockVaddr + (dynEndOff - req.ExtraBlockOffset);
            WritePhdr(buf, phOff, PT_LOAD, PF_R | PF_X, dynEndOff, stubInterpVaddr,
                      stubInterpSz, stubInterpSz, pageAlign);
            phNum++;
        }
    }

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

} // namespace Elfpatcher
