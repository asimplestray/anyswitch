#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <algorithm>
#include <elf.h>

namespace Elfpatcher::Linux {

LinuxElfPatcher::LinuxElfPatcher(
    std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
    std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
    std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
    std::shared_ptr<Io::IByteWriter> byteWriter
)
    : _entryStubBuilder(std::move(entryStubBuilder))
    , _programHeaderLayoutBuilder(std::move(programHeaderLayoutBuilder))
    , _sectionHeaderTableBuilder(std::move(sectionHeaderTableBuilder))
    , _byteWriter(std::move(byteWriter)) {}

void LinuxElfPatcher::_appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) {
    _byteWriter->AppendU64(buf, static_cast<std::uint64_t>(tag));
    _byteWriter->AppendU64(buf, val);
}

void LinuxElfPatcher::_appendTrampoline(std::vector<std::uint8_t>& /*buf*/, const Codegen::RelocationFixup& /*fixup*/) {
    // Trampolines for long jumps when translated code is too far from originals
    // Implementation will handle x86-64 rel32 range checks
}

std::vector<std::uint8_t> LinuxElfPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynSection,
    std::uint64_t originalPltGotVaddr,
    const std::string& runPath,
    bool lazyBinding,
    bool dependencyDiagnostics,
    const std::vector<Codegen::RelocationFixup>& /*fixups*/)
{
    if (dependencyDiagnostics) {
        // Linux target doesn't support dependency diagnostics
        std::vector<std::uint8_t> dummy;
        return dummy;
    }

    std::vector<std::uint8_t> buf = sourceElf;

    // Set ELF type to ET_DYN (shared object/executable)
    buf[kEhdrTypeOffset] = 3;     // ET_DYN = 3
    buf[kEhdrTypeOffset + 1] = 0;

    // Clear section header table (will rebuild)
    _byteWriter->WriteU64(buf, kEhdrShOffOffset, 0);
    _byteWriter->WriteU16(buf, kEhdrShNumOffset, 0);
    _byteWriter->WriteU16(buf, kEhdrShStrNdxOffset, 0);

    // Build extra block: dynstr, dynsym, rela, jmprel, dynamic segment, entry stub, interp
    const auto extraBlockOff = static_cast<std::uint64_t>(buf.size());

    // .dynstr
    std::uint64_t dynStrOff = buf.size();
    for (std::uint8_t b : dynSection.DynStrData) buf.push_back(b);
    // Append runpath
    std::uint64_t runPathStrOff = buf.size() - dynStrOff;
    for (char c : runPath) buf.push_back(static_cast<std::uint8_t>(c));
    buf.push_back(0);
    // Align
    while (buf.size() % kDynStrAlignment) buf.push_back(0);

    // .dynsym
    std::uint64_t dynSymOff = buf.size();
    for (std::uint8_t b : dynSection.DynSymData) buf.push_back(b);
    while (buf.size() % kDynSymAlignment) buf.push_back(0);

    // .rela
    std::uint64_t relaOff = buf.size();
    for (std::uint8_t b : dynSection.RelaData) buf.push_back(b);
    while (buf.size() % kRelaAlignment) buf.push_back(0);

    // .rela.plt
    std::uint64_t relaPltOff = buf.size();
    for (std::uint8_t b : dynSection.RelaPltData) buf.push_back(b);
    while (buf.size() % kRelaPltAlignment) buf.push_back(0);

    // Compute virtual addresses
    const auto extraBlockVaddr = _programHeaderLayoutBuilder->ComputeExtraBlockVaddr(originalHeaders, extraBlockOff);
    auto vaddrOf = [extraBlockOff, extraBlockVaddr](std::uint64_t fileOffset) -> std::uint64_t {
        if (fileOffset < extraBlockOff)
            throw Domain::RelinkerException("Offset before extra block", fileOffset);
        return extraBlockVaddr + (fileOffset - extraBlockOff);
    };

    // Build .dynamic segment
    std::vector<std::uint8_t> dynSegBuf;
    for (std::uint8_t b : dynSection.DynamicSegmentData) dynSegBuf.push_back(b);
    _appendDynEntry(dynSegBuf, DT_STRTAB, vaddrOf(dynStrOff));
    _appendDynEntry(dynSegBuf, DT_STRSZ, dynSection.DynStrData.size());
    _appendDynEntry(dynSegBuf, DT_SYMTAB, vaddrOf(dynSymOff));
    _appendDynEntry(dynSegBuf, DT_SYMENT, kSymEntrySize);
    if (!dynSection.RelaData.empty()) {
        _appendDynEntry(dynSegBuf, DT_RELA, vaddrOf(relaOff));
        _appendDynEntry(dynSegBuf, DT_RELASZ, dynSection.RelaData.size());
        _appendDynEntry(dynSegBuf, DT_RELAENT, kRelaEntrySize);
    }
    if (!dynSection.RelaPltData.empty()) {
        _appendDynEntry(dynSegBuf, DT_JMPREL, vaddrOf(relaPltOff));
        _appendDynEntry(dynSegBuf, DT_PLTRELSZ, dynSection.RelaPltData.size());
        _appendDynEntry(dynSegBuf, DT_PLTREL, DT_RELA);
        _appendDynEntry(dynSegBuf, 0x61000027, originalPltGotVaddr); // DT_OS_PLTGOT
    }
    if (!lazyBinding)
        _appendDynEntry(dynSegBuf, 5, DF_BIND_NOW); // DT_FLAGS
    _appendDynEntry(dynSegBuf, 0x1d, runPathStrOff); // DT_RUNPATH
    _appendDynEntry(dynSegBuf, 0, 0); // DT_NULL

    std::uint64_t dynSegOff = buf.size();
    for (std::uint8_t b : dynSegBuf) buf.push_back(b);

    // Entry stub
    std::uint64_t realEntryVaddr = *reinterpret_cast<const std::uint64_t*>(buf.data() + kEhdrEntryOffset);
    std::uint64_t stubOff = buf.size();
    std::uint64_t stubVaddr = vaddrOf(stubOff);
    auto stubBytes = _entryStubBuilder->BuildEntryStub(stubVaddr, realEntryVaddr);
    for (std::uint8_t b : stubBytes) buf.push_back(b);
    _byteWriter->WriteU64(buf, kEhdrEntryOffset, stubVaddr);

    // Interpreter
    static constexpr char kInterp[] = "/lib64/ld-linux-x86-64.so.2";
    std::uint64_t interpOff = buf.size();
    for (char c : kInterp) buf.push_back(static_cast<std::uint8_t>(c));

    std::uint64_t extraBlockSize = buf.size() - extraBlockOff;

    // Program header layout
    ProgramHeaderLayoutRequest layoutReq{};
    layoutReq.PhOff = *reinterpret_cast<const std::uint64_t*>(buf.data() + kEhdrPhOffOffset);
    layoutReq.PhEntSize = *reinterpret_cast<const std::uint16_t*>(buf.data() + kEhdrPhEntSizeOffset);
    layoutReq.PhNum = *reinterpret_cast<const std::uint16_t*>(buf.data() + kEhdrPhNumOffset);
    layoutReq.OriginalHeaders = originalHeaders;
    layoutReq.ExtraBlockOffset = extraBlockOff;
    layoutReq.ExtraBlockVaddr = extraBlockVaddr;
    layoutReq.ExtraBlockSize = extraBlockSize;
    layoutReq.DynamicSegmentOffset = dynSegOff;
    layoutReq.DynamicSegmentSize = dynSegBuf.size();
    layoutReq.InterpOffset = interpOff;
    layoutReq.InterpSize = sizeof(kInterp);

    std::uint16_t writtenPh = _programHeaderLayoutBuilder->WriteLayout(buf, layoutReq);
    _byteWriter->WriteU16(buf, kEhdrPhNumOffset, writtenPh);

    // Section headers
    SectionHeaderTableRequest sectionReq{};
    sectionReq.DynStrOffset = dynStrOff;
    sectionReq.DynStrSize = dynSection.DynStrData.size();
    sectionReq.DynSymOffset = dynSymOff;
    sectionReq.DynSymSize = dynSection.DynSymData.size();
    sectionReq.DynamicSegmentOffset = dynSegOff;
    sectionReq.DynamicSegmentSize = dynSegBuf.size();
    sectionReq.StubOffset = stubOff;
    sectionReq.StubSize = stubBytes.size();

    _sectionHeaderTableBuilder->WriteTable(buf, sectionReq);

    return buf;
}

} // namespace Elfpatcher::Linux
