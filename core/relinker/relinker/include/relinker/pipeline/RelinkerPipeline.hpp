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

private:
    std::shared_ptr<IElfReader> _elfReader;
    std::shared_ptr<Codegen::IArm64Translator> _translator;
    std::shared_ptr<ISysVDynamicSectionBuilder> _dynamicSectionBuilder;
    RelinkResult _currentResult;

    static constexpr std::uint32_t PT_LOAD = 1;
    static constexpr std::uint32_t PT_DYNAMIC = 2;
    static constexpr std::uint32_t PF_X = 0x1;

    void _extractRelocations(
        const std::vector<Domain::DynamicTag>& tags,
        std::vector<Domain::Relocation>& relocations,
        std::vector<std::string>& neededLibraries
    );
};

} // namespace Relinker

#endif
