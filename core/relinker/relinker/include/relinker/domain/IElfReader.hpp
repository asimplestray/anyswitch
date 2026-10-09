#ifndef RELINKER_DOMAIN_IElfReader_HPP
#define RELINKER_DOMAIN_IElfReader_HPP

#include <domain/Types.hpp>
#include <domain/GuestRuntime.hpp>
#include <vector>
#include <memory>

namespace Relinker {

using Domain::ElfHeader;
using Domain::ProgramHeader;
using Domain::SectionHeader;
using Domain::DynamicTag;
using Domain::FileByteOffset;
using Domain::VirtualAddress;

class IElfReader {
public:
    virtual ~IElfReader() = default;

    [[nodiscard]] virtual ElfHeader ReadHeader() const = 0;
    [[nodiscard]] virtual std::vector<ProgramHeader> ReadProgramHeaders() const = 0;
    [[nodiscard]] virtual std::vector<SectionHeader> ReadSectionHeaders() const = 0;
    [[nodiscard]] virtual std::vector<std::vector<DynamicTag>> ReadDynamicTags(const ProgramHeader& ph) const = 0;
    [[nodiscard]] virtual FileByteOffset TranslateVirtualAddress(VirtualAddress addr) const = 0;
    [[nodiscard]] virtual std::vector<std::uint8_t> ReadSection(const SectionHeader& hdr) const = 0;
    [[nodiscard]] virtual std::vector<std::uint8_t> ReadSegment(const ProgramHeader& ph) const = 0;
    [[nodiscard]] virtual std::uint64_t GetFileSize() const = 0;
    [[nodiscard]] virtual const std::vector<std::uint8_t>& GetRawBytes() const = 0;
};

} // namespace Relinker

#endif
