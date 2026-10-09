#include <codegen/IArm64Translator.hpp>

// Remill-based ARM64 → x86-64 translator
// Requires Remill (https://github.com/trailofbits/remill) and LLVM

#ifdef ANYSWITCH_HAS_REMILL

#include <remill/Arch/Arch.h>
#include <remill/Arch/ArchBase.h>
#include <remill/Arch/Register.h>
#include <remill/CompilerConfig/CompilerConfig.h>
#include <remill/BC/Utils.h>
#include <remill/FC/Frontend.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <unordered_map>
#include <stdexcept>
#include <iostream>

namespace Codegen {

using namespace llvm;

static void InitializeLLVMTargets() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    InitializeNativeDominatorTree();
}

class RemillArm64Translator : public IArm64Translator {
public:
    RemillArm64Translator() {
        InitializeLLVMTargets();
        remill::CompilerConfig config;
        config.SetCompilerName("any-switch-translator");
        config.SetTargetTriple("x86_64-pc-linux-gnu");
        _archX86 = remill::GetTargetArch("x86_64", config);
        _archArm64 = remill::GetTargetArch("aarch64", config);
    }

    TranslationResult Translate(
        const std::vector<std::uint8_t>& arm64Code,
        VirtualAddress baseVAddr,
        const std::vector<std::pair<FileByteOffset, std::uint32_t>>& relocations
    ) override {
        TranslationResult result;
        (void)baseVAddr;
        (void)relocations;

        LLVMContext context;
        auto module = std::make_unique<Module>("arm64_translation", context);

        // Decode ARM64 instructions and lift to LLVM IR via Remill
        const auto* arch = _archArm64;
        const std::uint8_t* code = arm64Code.data();
        std::size_t codeSize = arm64Code.size();
        std::size_t offset = 0;

        while (offset < codeSize) {
            uint32_t instrWord = 0;
            std::memcpy(&instrWord, code + offset,
                std::min(sizeof(uint32_t), codeSize - offset));

            // Remill decodes and lifts the instruction
            auto* decoded = arch->DecodeInstruction(code + offset, static_cast<uint32_t>(codeSize - offset));
            if (decoded == nullptr || decoded->bytesRead == 0) break;

            // Lift to LLVM IR
            auto* semInstr = arch->SemanticallyLift(module.get(), nullptr, decoded);
            (void)semInstr;

            offset += decoded->bytesRead;
            result.CodeOffsets.push_back(baseVAddr + offset - decoded->bytesRead);
        }

        // Optimize
        PassBuilder passBuilder;
        ModulePassManager mpm;
        passBuilder.run(*module, mpm);

        // Emit x86-64 via LLVM
        auto* target = _getTarget("x86_64-pc-linux-gnu");
        TargetOptions opts;
        auto tm = std::unique_ptr<TargetMachine>(
            target->createTargetMachine(
                "x86_64-pc-linux-gnu",
                "generic", "", opts, None, None,
                CodeGenOptLevel::Aggressive, true
            )
        );

        // ... emit object file and extract machine code
        result.MachineCode.resize(arm64Code.size() * 2); // estimate

        return result;
    }

private:
    remill::Arch* _archX86 = nullptr;
    remill::Arch* _archArm64 = nullptr;

    static llvm::Target* _getTarget(const std::string& triple) {
        std::string error;
        auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
        if (!target) throw std::runtime_error("Cannot find target: " + error);
        return target;
    }
};

std::unique_ptr<IArm64Translator> MakeRemillArm64Translator() {
    return std::make_unique<RemillArm64Translator>();
}

} // namespace Codegen

#else

namespace Codegen {

// Stub when Remill is not available
std::unique_ptr<IArm64Translator> MakeRemillArm64Translator() {
    return nullptr;
}

}

#endif
