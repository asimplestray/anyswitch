#ifndef RELINKER_OUTPUT_SYSVDYNAMICSECTIONBUILDER_HPP
#define RELINKER_OUTPUT_SYSVDYNAMICSECTIONBUILDER_HPP

#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>
#include <io/ByteWriter.hpp>
#include <vector>

namespace Relinker {

class SysVDynamicSectionBuilder : public ISysVDynamicSectionBuilder {
public:
    Domain::SysVDynamicSection BuildDynamicSection(
        const std::vector<Domain::Relocation>& relocations,
        const std::vector<std::string>& neededLibraries,
        Domain::FileByteOffset jmpRelOffset,
        std::uint32_t pltSlotCount
    ) override;

private:
    void _appendDynSymEntry(std::vector<std::uint8_t>& buf, const std::string& name, std::uint8_t bind, std::uint8_t type, std::uint64_t value, std::uint64_t size);
};

}

#endif
