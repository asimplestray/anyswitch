#ifndef RELINKER_DOMAIN_IRELINKRESULT_HPP
#define RELINKER_DOMAIN_IRELINKRESULT_HPP

#include <domain/Types.hpp>
#include <vector>

namespace Relinker {

struct RelinkPatch {
    Domain::FileByteOffset Offset;
    std::vector<std::uint8_t> Bytes;
};

struct RelinkResult {
    std::vector<std::string> RegistryEntries;
    std::vector<Domain::ProgramHeader> OriginalHeaders;
    Domain::SysVDynamicSection DynamicSection;
    Domain::VirtualAddress OriginalPltGotVaddr;
    std::vector<RelinkPatch> Patches;
    std::vector<std::uint8_t> TranslatedCode;
    std::vector<Domain::VirtualAddress> CodeInfo;
};

}

#endif
