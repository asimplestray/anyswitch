#ifndef RELINKER_PIPELINE_RELINKERPIPELINE_HPP
#define RELINKER_PIPELINE_RELINKERPIPELINE_HPP

#include <relinker/domain/IRelinkerPipeline.hpp>
#include <relinker/domain/IElfReader.hpp>
#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>
#include <codegen/IArm64Translator.hpp>
#include <memory>
#include <vector>

namespace Relinker {

class RelinkerPipeline : public IRelinkerPipeline {
public:
    RelinkerPipeline(
        std::shared_ptr<IElfReader> elfReader,
        std::shared_ptr<Codegen::IArm64Translator> translator,
        std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder
    );

    RelinkResult Relink(const std::vector<std::uint8_t>& sourceElf) override;

    // Set NSO/NRO dynstr/dynsym data for symbol resolution
    void SetDynSymData(std::vector<std::uint8_t> dynStr, std::uint32_t dynStrSize,
                       std::vector<std::uint8_t> dynSym, std::uint32_t dynSymSize) {
        _dynStrData = std::move(dynStr);
        _dynStrSize = dynStrSize;
        _dynSymData = std::move(dynSym);
        _dynSymSize = dynSymSize;
    }

private:
    std::shared_ptr<IElfReader> _elfReader;
    std::shared_ptr<Codegen::IArm64Translator> _translator;
    std::shared_ptr<ISysVDynamicSectionBuilder> _dynamicSectionBuilder;
    RelinkResult _currentResult;

    // NSO/NRO dynstr/dynsym extents (populated when input is NSO/NRO)
    std::vector<std::uint8_t> _dynStrData;
    std::uint32_t _dynStrSize = 0;
    std::vector<std::uint8_t> _dynSymData;
    std::uint32_t _dynSymSize = 0;

    static constexpr std::uint32_t PT_LOAD = 1;
    static constexpr std::uint32_t PT_DYNAMIC = 2;
    static constexpr std::uint32_t PF_X = 0x1;

    void _extractRelocations(
        const std::vector<Domain::DynamicTag>& tags,
        std::vector<Domain::Relocation>& relocations,
        std::vector<std::string>& neededLibraries,
        const std::uint8_t* dynStr = nullptr,
        std::uint32_t dynStrSize = 0,
        const std::uint8_t* dynSym = nullptr,
        std::uint32_t dynSymSize = 0
    );
};

} // namespace Relinker

#endif
