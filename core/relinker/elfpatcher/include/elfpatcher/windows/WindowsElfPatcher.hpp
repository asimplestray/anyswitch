#ifndef ELFPATCHER_WINDOWS_WINDOWS_ELF_PATCHER_HPP
#define ELFPATCHER_WINDOWS_WINDOWS_ELF_PATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <elfpatcher/windows/WindowsImportBuilder.hpp>
#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <elfpatcher/windows/WindowsTlsBuilder.hpp>
#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsDependencyStubBuilder.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <io/ByteWriter.hpp>
#include <memory>

namespace Elfpatcher::Windows {

class WindowsPePatcher : public IElfPatcher {
public:
    explicit WindowsPePatcher(bool gui);

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
    bool _gui;
    std::shared_ptr<Io::ByteWriter> _byteWriter;
    WindowsImportBuilder _importBuilder;
    WindowsRelocationBuilder _relocBuilder;
    WindowsTlsBuilder _tlsBuilder;
    WindowsTrampolineBuilder _trampolineBuilder;
    WindowsEntryStubBuilder _entryStubBuilder;
    WindowsLoadImage _loadImage;
    WindowsDependencyStubBuilder _depStubBuilder;
    WindowsPeWriter _peWriter;
};

} // namespace Elfpatcher::Windows

#endif
