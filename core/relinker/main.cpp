#include <Cli.hpp>
#include <domain/Types.hpp>
#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <codegen/IArm64Translator.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <relinker/parsing/NsoReader.hpp>
#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <filesystem>
#include <iostream>
#include <memory>

int main(const int argc, char* argv[]) {
    Cli::Args args;
    try {
        args = Cli::ParseArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }

    try {
        Io::FileReader fileReader;
        Io::FileWriter fileWriter;
        auto sourceBytes = fileReader.Read(args.inputPath);

        if (Relinker::NsoReader::IsNso(sourceBytes)) {
            auto nso = Relinker::NsoReader::Parse(sourceBytes);
            std::cout << "NSO detected: text=" << nso.text.bytes.size()
                      << " rodata=" << nso.rodata.bytes.size()
                      << " data=" << nso.data.bytes.size()
                      << " bss=" << nso.bssSize << "\n";
            sourceBytes = Relinker::NsoReader::ConvertToElf(nso);
        }

        auto elfReader = std::make_shared<Relinker::ElfReader>(sourceBytes);

        // Build ARM64 → x86-64 translator (uses Remill + LLVM)
        std::shared_ptr<Codegen::IArm64Translator> translator;
        if (args.toIntel) {
#ifdef ANYSWITCH_HAS_LLVM
            translator.reset(Codegen::MakeRemillArm64Translator().release());
#else
            throw Domain::RelinkerException("Intel output requires LLVM/Remill support (not built)");
#endif
        }

        auto dynBuilder = std::make_shared<Relinker::SysVDynamicSectionBuilder>();

        auto pipeline = std::make_shared<Relinker::RelinkerPipeline>(
            elfReader, translator, dynBuilder
        );

        auto result = pipeline->Relink(sourceBytes);

        // Apply patches to source
        for (const auto& patch : result.Patches) {
            if (patch.Offset > sourceBytes.size())
                throw Domain::RelinkerException("Patch exceeds source image", patch.Offset);
            for (std::size_t i = 0; i < patch.Bytes.size() && patch.Offset + i < sourceBytes.size(); ++i)
                sourceBytes[patch.Offset + i] = patch.Bytes[i];
        }

        // Replace code section with translated code
        if (!result.TranslatedCode.empty()) {
            // In a full impl, the translated code replaces original ARM64 code
            // and offsets are adjusted
        }

        auto byteWriter = std::make_shared<Io::ByteWriter>();
        std::shared_ptr<Elfpatcher::IElfPatcher> patcher;

        if (args.toWindows) {
            throw Domain::RelinkerException("Windows output not yet implemented");
        } else {
            patcher = std::make_shared<Elfpatcher::Linux::LinuxElfPatcher>(
                std::make_shared<Elfpatcher::EntryStubBuilder>(),
                std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(),
                std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(),
                byteWriter
            );
        }

        auto executableBytes = patcher->Patch(
            sourceBytes,
            result.OriginalHeaders,
            result.DynamicSection,
            result.OriginalPltGotVaddr,
            args.runPath,
            args.lazyBinding,
            args.windowsDiagnostics,
            {}
        );

        const auto absPath = std::filesystem::absolute(args.outputPath).string();
        fileWriter.Write(absPath, executableBytes);

        std::cout << "External library references: " << result.RegistryEntries.size() << "\n";
        std::cout << "Output file: " << absPath << "\n";

    } catch (const Domain::RelinkerException& e) {
        std::cerr << "FAIL: " << e.what();
        if (e.FailureOffset != 0) std::cerr << " (offset 0x" << std::hex << e.FailureOffset << ")";
        std::cerr << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 2;
    }

    return 0;
}
