#ifndef ELFPATCHER_GENERAL_IELFPATCHER_HPP
#define ELFPATCHER_GENERAL_IELFPATCHER_HPP

#include <domain/Types.hpp>
#include <codegen/IArm64Translator.hpp>
#include <vector>
#include <string>

namespace Elfpatcher {

class IElfPatcher {
public:
    virtual ~IElfPatcher() = default;
    virtual std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics,
        const std::vector<Codegen::RelocationFixup>& fixups
    ) = 0;
};

}

#endif
