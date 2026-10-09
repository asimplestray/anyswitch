#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace Relinker {

RelinkerPipeline::RelinkerPipeline(
    std::shared_ptr<IElfReader> elfReader,
    std::shared_ptr<Codegen::IArm64Translator> translator,
    std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder
)
    : _elfReader(std::move(elfReader))
    , _translator(std::move(translator))
    , _dynamicSectionBuilder(std::move(dynamicSectionBuilder)) {}

RelinkResult RelinkerPipeline::Relink(const std::vector<std::uint8_t>& sourceElf) {
    auto programHeaders = _elfReader->ReadProgramHeaders();

    // 1. Find executable code segments (PT_LOAD with execute permission)
    std::vector<Domain::ProgramHeader> codeSegments;
    for (const auto& ph : programHeaders) {
        if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0)
            codeSegments.push_back(ph);
    }

    if (codeSegments.empty())
        throw Domain::RelinkerException("No executable PT_LOAD segments found");

    // 2. Read dynamic segment
    std::vector<Domain::DynamicTag> dynTags;
    bool hasDynamic = false;
    for (const auto& ph : programHeaders) {
        if (ph.Type == PT_DYNAMIC) {
            hasDynamic = true;
            auto result = _elfReader->ReadDynamicTags(ph);
            if (!result.empty()) dynTags = result[0];
            break;
        }
    }
    if (!hasDynamic) throw Domain::RelinkerException("No PT_DYNAMIC segment");

    // 3. Extract relocations and needed libraries
    std::vector<Domain::Relocation> relocations;
    std::vector<std::string> neededLibraries;
    _extractRelocations(dynTags, relocations, neededLibraries);

    // 4. Translate ARM64 code to x86-64
    for (const auto& ph : codeSegments) {
        const auto arm64Code = _elfReader->ReadSegment(ph);

        // Collect relocations for this segment
        std::vector<std::pair<Domain::FileByteOffset, std::uint32_t>> relocRefs;
        for (const auto& r : relocations) {
            if (r.Offset >= ph.Offset && r.Offset < ph.Offset + ph.FileSize)
                relocRefs.push_back({r.Offset, r.Type});
        }

        if (!arm64Code.empty() && _translator) {
            auto translated = _translator->Translate(
                arm64Code, ph.MappedAddress, relocRefs
            );
            // Store translated code
            _currentResult.TranslatedCode = translated.MachineCode;
            _currentResult.CodeInfo = translated.CodeOffsets;
        }
    }

    // 5. Build new dynamic section
    auto dynSection = _dynamicSectionBuilder->BuildDynamicSection(
        relocations, neededLibraries, 0, 0
    );

    // 6. Build result
    RelinkResult result;
    result.OriginalHeaders = programHeaders;
    result.DynamicSection = std::move(dynSection);
    result.OriginalPltGotVaddr = 0;
    result.TranslatedCode = std::move(_currentResult.TranslatedCode);
    result.CodeInfo = std::move(_currentResult.CodeInfo);

    std::cout << "Dynamic symbol references: " << relocations.size() << "\n";
    std::cout << "Translated code: " << result.TranslatedCode.size() << " bytes\n";

    return result;
}

void RelinkerPipeline::_extractRelocations(
    const std::vector<Domain::DynamicTag>& tags,
    std::vector<Domain::Relocation>& relocations,
    std::vector<std::string>& neededLibraries
) {
    constexpr std::int64_t DT_RELA = 7;
    constexpr std::int64_t DT_RELASZ = 8;
    constexpr std::int64_t DT_JMPREL = 23;
    constexpr std::int64_t DT_PLTRELSZ = 2;
    constexpr std::int64_t DT_NEEDED = 1;

    Domain::FileByteOffset relaOffset = 0;
    Domain::ByteCount relaSize = 0;
    Domain::FileByteOffset jmpRelOffset = 0;
    Domain::ByteCount jmpRelSize = 0;

    for (const auto& tag : tags) {
        switch (tag.Tag) {
            case DT_RELA: relaOffset = _elfReader->TranslateVirtualAddress(static_cast<VirtualAddress>(tag.Value)); break;
            case DT_RELASZ: relaSize = tag.Value; break;
            case DT_JMPREL: jmpRelOffset = _elfReader->TranslateVirtualAddress(static_cast<VirtualAddress>(tag.Value)); break;
            case DT_PLTRELSZ: jmpRelSize = tag.Value; break;
            case DT_NEEDED: neededLibraries.push_back(""); break; // resolved below
        }
    }

    // Read needed library names
    const auto& raw = _elfReader->GetRawBytes();
    auto readCStr = [&](std::uint64_t offset) -> std::string {
        std::string result;
        while (offset < raw.size() && raw[offset] != 0)
            result.push_back(static_cast<char>(raw[offset++]));
        return result;
    };

    for (auto& tag : tags) {
        if (tag.Tag == DT_NEEDED) {
            auto it = std::find_if(neededLibraries.begin(), neededLibraries.end(),
                [](const std::string& s) { return s.empty(); });
            if (it != neededLibraries.end()) {
                *it = readCStr(tag.Value);
            }
        }
    }

    // Parse RELA entries (each is 24 bytes for AArch64)
    constexpr std::size_t RELA_ENT_SIZE = 24;
    auto parseRela = [&](Domain::FileByteOffset offset, Domain::ByteCount size) {
        for (Domain::ByteCount i = 0; i + RELA_ENT_SIZE <= size; i += RELA_ENT_SIZE) {
            const auto pos = offset + i;
            if (pos + RELA_ENT_SIZE > raw.size())
                throw Domain::RelinkerException("Relocation out of bounds", pos);

            Domain::Relocation rel;
            std::memcpy(&rel.Offset, raw.data() + pos, 8);
            std::uint64_t rInfo = 0;
            std::memcpy(&rInfo, raw.data() + pos + 8, 8);
            std::memcpy(&rel.Addend, raw.data() + pos + 16, 8);
            rel.Type = static_cast<std::uint32_t>(rInfo & 0xFFFFFFFF);
            rel.SymbolIndex = static_cast<std::int32_t>(rInfo >> 32);
            rel.ImportName = ""; // resolved from dynsym
            relocations.push_back(rel);
        }
    };

    if (relaOffset && relaSize) parseRela(relaOffset, relaSize);
    if (jmpRelOffset && jmpRelSize) parseRela(jmpRelOffset, jmpRelSize);
}

} // namespace Relinker
