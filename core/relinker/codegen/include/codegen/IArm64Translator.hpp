#ifndef CODEGEN_IARM64TRANSLATOR_HPP
#define CODEGEN_IARM64TRANSLATOR_HPP

#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <vector>
#include <memory>

namespace Codegen {

struct RelocationFixup {
    Domain::FileByteOffset OffsetInOutput;
    std::int64_t Addend;
    std::uint32_t RelocationType;
    bool IsRelative;
    bool IsPlt;
};

struct TranslationResult {
    std::vector<std::uint8_t> MachineCode;
    std::vector<Domain::VirtualAddress> CodeOffsets;
    std::vector<RelocationFixup> Fixups;
};

class IArm64Translator {
public:
    virtual ~IArm64Translator() = default;
    virtual TranslationResult Translate(
        const std::vector<std::uint8_t>& arm64Code,
        Domain::VirtualAddress baseVAddr,
        const std::vector<std::pair<Domain::FileByteOffset, std::uint32_t>>& relocations
    ) = 0;
};

#if defined(ANYSWITCH_HAS_LLVM)
std::unique_ptr<IArm64Translator> MakeRemillArm64Translator();
#endif

} // namespace Codegen

#endif
