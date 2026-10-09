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
#include <relinker/parsing/NroReader.hpp>
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

                // NSO/NRO dynstr/dynsym extents for symbol resolution
        std::vector<std::uint8_t> dynStrData, dynSymData;
        std::uint32_t dynStrSize = 0, dynSymSize = 0;
        const std::uint8_t* rodataPtr = nullptr;
        std::uint32_t roDataSize = 0;

        if (Relinker::NsoReader::IsNso(sourceBytes)) {
            auto nso = Relinker::NsoReader::Parse(sourceBytes);
            std::cout << "NSO detected: text=" << nso.text.bytes.size()
                      << " rodata=" << nso.rodata.bytes.size()
                      << " data=" << nso.data.bytes.size()
                      << " bss=" << nso.bssSize << "\n";
            // Extract dynstr/dynsym from rodata
            if (nso.dynStrSize > 0 && nso.dynStrOffset + nso.dynStrSize <= nso.rodata.bytes.size()) {
                dynStrData.assign(nso.rodata.bytes.begin() + nso.dynStrOffset,
                                  nso.rodata.bytes.begin() + nso.dynStrOffset + nso.dynStrSize);
                dynStrSize = nso.dynStrSize;
            }
            if (nso.dynSymSize > 0 && nso.dynSymOffset + nso.dynSymSize <= nso.rodata.bytes.size()) {
                dynSymData.assign(nso.rodata.bytes.begin() + nso.dynSymOffset,
                                  nso.rodata.bytes.begin() + nso.dynSymOffset + nso.dynSymSize);
                dynSymSize = nso.dynSymSize;
            }
            sourceBytes = Relinker::NsoReader::ConvertToElf(nso);
        } else if (Relinker::NroReader::IsNro(sourceBytes)) {
            auto nro = Relinker::NroReader::Parse(sourceBytes);
            std::cout << "NRO detected: text=" << nro.text.bytes.size()
                      << " rodata=" << nro.rodata.bytes.size()
                      << " data=" << nro.data.bytes.size()
                      << " bss=" << nro.bssSize << "\n";
            // Extract dynstr/dynsym from rodata
            if (nro.dynStrSize > 0 && nro.dynStrOffset + nro.dynStrSize <= nro.rodata.bytes.size()) {
                dynStrData.assign(nro.rodata.bytes.begin() + nro.dynStrOffset,
                                  nro.rodata.bytes.begin() + nro.dynStrOffset + nro.dynStrSize);
                dynStrSize = nro.dynStrSize;
            }
            if (nro.dynSymSize > 0 && nro.dynSymOffset + nro.dynSymSize <= nro.rodata.bytes.size()) {
                dynSymData.assign(nro.rodata.bytes.begin() + nro.dynSymOffset,
                                  nro.rodata.bytes.begin() + nro.dynSymOffset + nro.dynSymSize);
                dynSymSize = nro.dynSymSize;
            }
            sourceBytes = Relinker::NroReader::ConvertToElf(nro);
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

        // Pass dynstr/dynsym data to pipeline for symbol resolution
        pipeline->SetDynSymData(std::move(dynStrData), dynStrSize,
                                std::move(dynSymData), dynSymSize);

        auto result = pipeline->Relink(sourceBytes);

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
            result.Translated.Fixups,
            result.Translated
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
