#ifndef RELINKER_DOMAIN_ISYSVDYNAMICSECTIONBUILDER_HPP
#define RELINKER_DOMAIN_ISYSVDYNAMICSECTIONBUILDER_HPP

#include <domain/Types.hpp>
#include <domain/GuestRuntime.hpp>
#include <vector>
#include <string>

namespace Relinker {

class ISysVDynamicSectionBuilder {
public:
    virtual ~ISysVDynamicSectionBuilder() = default;
    virtual Domain::SysVDynamicSection BuildDynamicSection(
        const std::vector<Domain::Relocation>& relocations,
        const std::vector<std::string>& neededLibraries,
        Domain::FileByteOffset jmpRelOffset,
        std::uint32_t pltSlotCount
    ) = 0;
};

std::unique_ptr<ISysVDynamicSectionBuilder> MakeSysVDynamicSectionBuilder();

}

#endif
