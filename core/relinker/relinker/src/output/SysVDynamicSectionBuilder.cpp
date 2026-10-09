#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/output/CallRegistryWriter.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <set>

namespace Relinker {

Domain::SysVDynamicSection SysVDynamicSectionBuilder::BuildDynamicSection(
    const std::vector<Domain::Relocation>& relocations,
    const std::vector<std::string>& neededLibraries,
    Domain::FileByteOffset /*jmpRelOffset*/,
    std::uint32_t /*pltSlotCount*/
) {
    Domain::SysVDynamicSection result;

    // Build .dynstr
    result.DynStrData.push_back(0);
    auto addString = [&](const std::string& s) -> std::uint32_t {
        std::uint32_t offset = static_cast<std::uint32_t>(result.DynStrData.size());
        for (char c : s) result.DynStrData.push_back(static_cast<std::uint8_t>(c));
        result.DynStrData.push_back(0);
        return offset;
    };

    std::vector<std::uint32_t> neededOffsets;
    for (const auto& lib : neededLibraries)
        neededOffsets.push_back(addString(lib));

    // Build .dynsym (entry 0 is reserved)
    result.DynSymData.resize(24, 0); // index 0 = null symbol
    std::vector<std::uint32_t> importOffsets;
    std::set<std::string> seenImports;
    for (const auto& rel : relocations) {
        if (seenImports.insert(rel.ImportName).second) {
            importOffsets.push_back(addString(rel.ImportName));
        }
    }
    for (std::uint32_t offset : importOffsets)
        _appendDynSymEntry(result.DynSymData, std::string(), 1, 0, 0, 0);

    // Build .dynamic segment
    auto appendDyn = [&](std::int64_t tag, std::uint64_t val) {
        result.DynamicSegmentData.resize(result.DynamicSegmentData.size() + 16);
        std::memcpy(result.DynamicSegmentData.data() + result.DynamicSegmentData.size() - 16, &tag, 8);
        std::memcpy(result.DynamicSegmentData.data() + result.DynamicSegmentData.size() - 8, &val, 8);
    };
    for (std::uint32_t offset : neededOffsets)
        appendDyn(1, offset); // DT_NEEDED
    appendDyn(0, 0); // DT_NULL

    result.NeededLibraries = neededLibraries;
    return result;
}

void SysVDynamicSectionBuilder::_appendDynSymEntry(
    std::vector<std::uint8_t>& buf, const std::string& /*name*/,
    std::uint8_t /*bind*/, std::uint8_t /*type*/,
    std::uint64_t /*value*/, std::uint64_t /*size*/
) {
    // Write Elf64_Sym (24 bytes): st_name, st_info, st_other, st_shndx, st_value, st_size
    std::size_t pos = buf.size();
    buf.resize(pos + 24, 0);
}

std::unique_ptr<ISysVDynamicSectionBuilder> MakeSysVDynamicSectionBuilder() {
    return std::make_unique<SysVDynamicSectionBuilder>();
}

} // namespace Relinker
