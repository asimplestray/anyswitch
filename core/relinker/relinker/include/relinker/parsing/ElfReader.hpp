#ifndef RELINKER_PARSING_ELFREADER_HPP
#define RELINKER_PARSING_ELFREADER_HPP

#include <relinker/domain/IElfReader.hpp>
#include <vector>
#include <cstdint>

namespace Relinker {

class ElfReader : public IElfReader {
public:
    explicit ElfReader(std::vector<std::uint8_t> buffer);

    ElfHeader ReadHeader() const override;
    std::vector<ProgramHeader> ReadProgramHeaders() const override;
    std::vector<SectionHeader> ReadSectionHeaders() const override;
    std::vector<std::vector<DynamicTag>> ReadDynamicTags(const ProgramHeader& ph) const override;
    FileByteOffset TranslateVirtualAddress(VirtualAddress addr) const override;
    std::vector<std::uint8_t> ReadSection(const SectionHeader& hdr) const override;
    std::vector<std::uint8_t> ReadSegment(const ProgramHeader& ph) const override;
    std::uint64_t GetFileSize() const override;
    const std::vector<std::uint8_t>& GetRawBytes() const override;

private:
    std::vector<std::uint8_t> _buffer;

    template<typename T>
    T _readAt(FileByteOffset offset) const;
    std::string _readCStr(FileByteOffset offset) const;
    std::string _resolveShdrName(std::uint32_t offset, const ElfHeader& hdr) const;
};

} // namespace Relinker

#endif
