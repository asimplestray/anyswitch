#ifndef ELFPATCHER_LINUX_LINUX_ELF_PATCHER_HPP
#define ELFPATCHER_LINUX_LINUX_ELF_PATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <elfpatcher/general/IEntryStubBuilder.hpp>
#include <elfpatcher/general/IProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/ISectionHeaderTableBuilder.hpp>
#include <io/ByteWriter.hpp>
#include <codegen/IArm64Translator.hpp>
#include <memory>
#include <vector>

namespace Elfpatcher::Linux {

class LinuxElfPatcher : public IElfPatcher {
public:
    LinuxElfPatcher(
        std::shared_ptr<IEntryStubBuilder> entryStubBuilder,
        std::shared_ptr<IProgramHeaderLayoutBuilder> programHeaderLayoutBuilder,
        std::shared_ptr<ISectionHeaderTableBuilder> sectionHeaderTableBuilder,
        std::shared_ptr<Io::IByteWriter> byteWriter
    );

    std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics,
        const std::vector<Codegen::RelocationFixup>& fixups,
        const Relinker::TranslatedCodeInfo& translated
    ) override;

private:
    void _appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val);
    void _appendTrampoline(std::vector<std::uint8_t>& buf, const Codegen::RelocationFixup& fixup);

    std::shared_ptr<IEntryStubBuilder> _entryStubBuilder;
    std::shared_ptr<IProgramHeaderLayoutBuilder> _programHeaderLayoutBuilder;
    std::shared_ptr<ISectionHeaderTableBuilder> _sectionHeaderTableBuilder;
    std::shared_ptr<Io::IByteWriter> _byteWriter;
};

} // namespace Elfpatcher::Linux

#endif
