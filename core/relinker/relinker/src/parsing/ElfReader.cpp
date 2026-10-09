#include <relinker/parsing/ElfReader.hpp>
#include <elf.h>
#include <cstring>

namespace Relinker {

ElfReader::ElfReader(std::vector<std::uint8_t> buffer)
    : _buffer(std::move(buffer)) {}

ElfHeader ElfReader::ReadHeader() const {
    if (_buffer.size() < sizeof(Elf64_Ehdr))
        throw Domain::RelinkerException("ELF file too small");
    if (_buffer[0] != 0x7F || _buffer[1] != 'E' || _buffer[2] != 'L' || _buffer[3] != 'F')
        throw Domain::RelinkerException("Invalid ELF magic");
    if (_buffer[4] != 2)
        throw Domain::RelinkerException("Not ELF64");

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(_buffer.data());
    return ElfHeader{
        ehdr->e_machine, ehdr->e_type, _buffer[7], _buffer[8],
        ehdr->e_entry, ehdr->e_phoff, ehdr->e_shoff,
        ehdr->e_phentsize, ehdr->e_phnum,
        ehdr->e_shentsize, ehdr->e_shnum, ehdr->e_shstrndx
    };
}

std::vector<ProgramHeader> ElfReader::ReadProgramHeaders() const {
    const auto hdr = ReadHeader();
    if (hdr.ProgramHeaderOffset == 0) return {};

    std::vector<ProgramHeader> result;
    result.reserve(hdr.ProgramHeaderCount);

    for (std::uint16_t i = 0; i < hdr.ProgramHeaderCount; ++i) {
        const auto offset = hdr.ProgramHeaderOffset + i * hdr.ProgramHeaderEntrySize;
        if (offset + sizeof(Elf64_Phdr) > _buffer.size())
            throw Domain::RelinkerException("Program header out of bounds", offset);
        const auto* ph = reinterpret_cast<const Elf64_Phdr*>(_buffer.data() + offset);
        result.push_back({
            ph->p_type, ph->p_flags, ph->p_offset,
            ph->p_vaddr, ph->p_paddr, ph->p_filesz, ph->p_memsz, ph->p_align
        });
    }
    return result;
}

std::vector<SectionHeader> ElfReader::ReadSectionHeaders() const {
    const auto hdr = ReadHeader();
    if (hdr.SectionHeaderOffset == 0) return {};

    std::vector<SectionHeader> result;
    result.reserve(hdr.SectionHeaderCount);

    for (std::uint16_t i = 0; i < hdr.SectionHeaderCount; ++i) {
        const auto offset = hdr.SectionHeaderOffset + i * hdr.SectionHeaderEntrySize;
        if (offset + sizeof(Elf64_Shdr) > _buffer.size())
            throw Domain::RelinkerException("Section header out of bounds", offset);
        const auto* sh = reinterpret_cast<const Elf64_Shdr*>(_buffer.data() + offset);
        result.push_back({
            _resolveShdrName(sh->sh_name, hdr), sh->sh_type, sh->sh_flags,
            sh->sh_addr, sh->sh_offset, sh->sh_size, sh->sh_link, sh->sh_info, sh->sh_entsize
        });
    }
    return result;
}

std::vector<std::vector<DynamicTag>> ElfReader::ReadDynamicTags(const ProgramHeader& ph) const {
    if (ph.Type != PT_DYNAMIC) return {};

    std::vector<DynamicTag> tags;
    const auto offset = TranslateVirtualAddress(ph.MappedAddress);
    if (offset == 0) throw Domain::RelinkerException("Cannot translate dynamic segment");

    for (std::size_t i = 0; offset + i * sizeof(Elf64_Dyn) + sizeof(Elf64_Dyn) <= _buffer.size(); ++i) {
        const auto* dyn = reinterpret_cast<const Elf64_Dyn*>(_buffer.data() + offset + i * sizeof(Elf64_Dyn));
        if (dyn->d_tag == DT_NULL) break;
        tags.push_back({static_cast<std::int64_t>(dyn->d_tag), dyn->d_un.d_val});
    }
    return {tags};
}

Domain::FileByteOffset ElfReader::TranslateVirtualAddress(VirtualAddress addr) const {
    // Walk program headers to find the LOAD segment containing this address
    const auto headers = ReadProgramHeaders();
    for (const auto& ph : headers) {
        if (ph.Type != PT_LOAD) continue;
        if (addr >= ph.MappedAddress && addr < ph.MappedAddress + ph.FileSize)
            return ph.Offset + (addr - ph.MappedAddress);
    }
    return 0;
}

std::vector<std::uint8_t> ElfReader::ReadSection(const SectionHeader& hdr) const {
    if (hdr.Offset + hdr.SectionSize > _buffer.size())
        throw Domain::RelinkerException("Section out of bounds", hdr.Offset);
    return std::vector<std::uint8_t>(_buffer.begin() + hdr.Offset,
                                      _buffer.begin() + hdr.Offset + hdr.SectionSize);
}

std::vector<std::uint8_t> ElfReader::ReadSegment(const ProgramHeader& ph) const {
    if (ph.Offset + ph.FileSize > _buffer.size())
        throw Domain::RelinkerException("Segment out of bounds", ph.Offset);
    return std::vector<std::uint8_t>(_buffer.begin() + ph.Offset,
                                      _buffer.begin() + ph.Offset + ph.FileSize);
}

std::uint64_t ElfReader::GetFileSize() const { return _buffer.size(); }
const std::vector<std::uint8_t>& ElfReader::GetRawBytes() const { return _buffer; }

template<typename T>
T ElfReader::_readAt(Domain::FileByteOffset offset) const {
    if (offset + sizeof(T) > _buffer.size())
        throw Domain::RelinkerException("Read out of bounds", offset);
    T value;
    std::memcpy(&value, _buffer.data() + offset, sizeof(T));
    return value;
}

std::string ElfReader::_readCStr(FileByteOffset offset) const {
    std::string result;
    while (offset < _buffer.size() && _buffer[offset] != 0)
        result.push_back(static_cast<char>(_buffer[offset++]));
    return result;
}

std::string ElfReader::_resolveShdrName(std::uint32_t offset, const ElfHeader& hdr) const {
    if (hdr.SectionHeaderStringIndex >= hdr.SectionHeaderCount) return {};
    const auto sections = ReadSectionHeaders();
    const auto& shstrtab = sections[hdr.SectionHeaderStringIndex];
    if (shstrtab.Offset + offset >= _buffer.size()) return {};
    return _readCStr(shstrtab.Offset + offset);
}

} // namespace Relinker
