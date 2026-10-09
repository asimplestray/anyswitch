#include <codegen/IArm64Translator.hpp>

// Remill-based ARM64 → x86-64 translator.
//
// Strategy (alpha): decode each 4-byte AArch64 instruction with Remill,
// lift it into its own LLVM function with InstructionLifter (semantics come
// from the aarch64.bc runtime built with Remill), run the LLVM O2 pipeline,
// emit an x86-64 object, and extract .text. One TranslationResult entry per
// lifted instruction; CodeOffsets holds guest virtual addresses.
//
// What this is NOT yet: there is no CFG recovery, no cross-block branch
// resolution, and relocations are recorded but not applied. The output is a
// faithful per-instruction lifting suitable for inspection and testing, not
// a runnable recompile. Those stages are tracked on the roadmap.

#ifdef ANYSWITCH_HAS_REMILL

#include <remill/Arch/Arch.h>
#include <remill/Arch/Name.h>
#include <remill/BC/ABI.h>
#include <remill/BC/InstructionLifter.h>
#include <remill/BC/IntrinsicTable.h>
#include <remill/OS/OS.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Codegen {

namespace {

constexpr std::size_t kAArch64InstrSize = 4;
constexpr std::size_t kMaxInstructions = 1024u * 1024u;
constexpr const char* kX86Triple = "x86_64-pc-linux-gnu";

std::string LiftedName(std::uint64_t guestAddr) {
    std::ostringstream os;
    os << "asw_lifted_" << std::hex << guestAddr;
    return os.str();
}

std::string SemanticsPath() {
    if (const char* env = std::getenv("ANYSWITCH_AARCH64_BC"))
        return env;
#ifdef ANYSWITCH_REMILL_AARCH64_BC
    return ANYSWITCH_REMILL_AARCH64_BC;
#else
    return {};
#endif
}

// llvm.expect is only a branch-prediction hint; LLVM 22's lowering pass
// crashes on Remill-shaped expect chains, so strip them pre-optimization.
void StripExpectIntrinsics(llvm::Module& mod) {
    std::vector<llvm::CallInst*> dead;
    for (auto& func : mod.functions()) {
        for (auto& block : func) {
            for (auto& instr : block) {
                auto* call = llvm::dyn_cast<llvm::CallInst>(&instr);
                if (!call || !call->getCalledFunction() ||
                    !call->getCalledFunction()->getName().starts_with("llvm.expect"))
                    continue;
                call->replaceAllUsesWith(call->getArgOperand(0));
                dead.push_back(call);
            }
        }
    }
    for (auto* call : dead)
        call->eraseFromParent();
}

void Optimize(llvm::Module& mod) {
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    llvm::PassBuilder pb;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    llvm::ModulePassManager mpm;
    // The semantics bitcode carries @llvm.compiler.used (900+ entries) to
    // defeat DCE at Remill's own build time. We want the opposite: drop it
    // so only what our lifted functions call survives.
    for (const char* name : {"llvm.compiler.used", "llvm.used"}) {
        if (auto* used = mod.getGlobalVariable(name, true); used && used->use_empty())
            used->eraseFromParent();
    }
    // The linked semantics bring ~500 functions; only a handful are called.
    // Internalize everything except our entry points so GlobalDCE can
    // actually drop the dead 99%.
    for (auto& func : mod.functions()) {
        if (!func.isDeclaration() && !func.getName().starts_with("asw_lifted_"))
            func.setLinkage(llvm::GlobalValue::InternalLinkage);
    }
    mpm.addPass(pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2));
    mpm.run(mod, mam);

    // ISEL_* dispatch tables pin every semantics function; after inlining,
    // the unused ones are dead weight.
    for (auto it = mod.global_begin(); it != mod.global_end();) {
        auto* g = &*it++;
        if (g->getName().starts_with("ISEL_") && g->use_empty())
            g->eraseFromParent();
    }
    llvm::ModulePassManager cleanup;
    cleanup.addPass(llvm::GlobalDCEPass());
    cleanup.run(mod, mam);
}

std::vector<std::uint8_t> EmitObject(llvm::Module& mod,
                                      std::map<std::string, std::uint64_t>& outAddrs) {
    std::string err;
    const auto* target = llvm::TargetRegistry::lookupTarget(kX86Triple, err);
    if (!target)
        throw std::runtime_error("Cannot find x86-64 target: " + err);

    llvm::TargetOptions opts;
    std::unique_ptr<llvm::TargetMachine> tm(target->createTargetMachine(
        llvm::Triple(kX86Triple), "generic", "", opts, std::nullopt, std::nullopt,
        llvm::CodeGenOptLevel::Aggressive));
    if (!tm)
        throw std::runtime_error("Cannot create x86-64 TargetMachine");

    mod.setTargetTriple(llvm::Triple(kX86Triple));
    mod.setDataLayout(tm->createDataLayout());

    llvm::SmallVector<char, 0> objBytes;
    llvm::raw_svector_ostream objStream(objBytes);
    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, objStream, nullptr, llvm::CodeGenFileType::ObjectFile))
        throw std::runtime_error("TargetMachine cannot emit object files");
    pm.run(mod);

    auto objOrErr = llvm::object::ObjectFile::createObjectFile(
        llvm::MemoryBufferRef(llvm::StringRef(objBytes.data(), objBytes.size()), "lifted"));
    if (!objOrErr)
        throw std::runtime_error("Cannot parse emitted object");
    auto& obj = *objOrErr;

    std::vector<std::uint8_t> text;
    std::uint64_t textAddr = 0;
    for (const auto& sec : obj->sections()) {
        if (!sec.isText())
            continue;
        auto contentsOrErr = sec.getContents();
        if (!contentsOrErr)
            continue;
        textAddr = sec.getAddress();
        const auto contents = *contentsOrErr;
        text.assign(contents.begin(), contents.end());
        break;
    }
    if (text.empty())
        throw std::runtime_error("Emitted object has no .text");

    for (const auto& sym : obj->symbols()) {
        auto nameOrErr = sym.getName();
        auto addrOrErr = sym.getAddress();
        if (!nameOrErr || !addrOrErr)
            continue;
        outAddrs[nameOrErr->str()] = *addrOrErr - textAddr;
    }
    return text;
}

} // namespace

class RemillArm64Translator : public IArm64Translator {
public:
    RemillArm64Translator() {
        static bool initialized = false;
        if (initialized)
            return;
        initialized = true;
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
    }

    TranslationResult Translate(
        const std::vector<std::uint8_t>& arm64Code,
        Domain::VirtualAddress baseVAddr,
        const std::vector<std::pair<Domain::FileByteOffset, std::uint32_t>>& relocations
    ) override {
        (void)relocations; // recorded by the pipeline; applied in a later stage
        TranslationResult result;
        if (arm64Code.empty())
            return result;

        llvm::LLVMContext context;
        auto mod = std::make_unique<llvm::Module>("arm64_translation", context);

        auto arch = remill::Arch::Get(context, remill::kOSLinux,
                                      remill::kArchAArch64LittleEndian);
        if (!arch)
            throw std::runtime_error("Remill has no AArch64 backend");

        LoadSemantics(*arch, *mod);
        arch->PrepareModule(mod.get());

        remill::IntrinsicTable intrinsics(mod.get());
        remill::InstructionLifter lifter(arch.get(), intrinsics);

        // Recursive descent: follow direct branches from the entry point so
        // data pockets (literal pools, jump tables) are never decoded as
        // code. Indirect targets are unknown statically and end a path.
        const auto codeEnd = baseVAddr + arm64Code.size();
        auto inRange = [&](std::uint64_t pc) {
            return pc >= baseVAddr && pc + kAArch64InstrSize <= codeEnd &&
                   (pc % kAArch64InstrSize) == 0;
        };
        std::set<std::uint64_t> visited;
        std::vector<std::uint64_t> worklist{baseVAddr};

        while (!worklist.empty() && visited.size() < kMaxInstructions) {
            const auto pc = worklist.back();
            worklist.pop_back();
            if (!inRange(pc) || !visited.insert(pc).second)
                continue;
            const auto offset = static_cast<std::size_t>(pc - baseVAddr);

            remill::Instruction inst;
            std::string_view bytes(reinterpret_cast<const char*>(arm64Code.data() + offset),
                                   kAArch64InstrSize);
            if (!arch->DecodeInstruction(
                    pc, bytes, inst, arch->CreateInitialContext()))
                continue; // data, not code

            auto* func = arch->DefineLiftedFunction(LiftedName(pc), mod.get());
            if (!func)
                throw std::runtime_error("Cannot define lifted function");
            if (func->getParent() != mod.get())
                throw std::runtime_error("Lifted function in wrong module");
            // DefineLiftedFunction pre-populates the entry block with state
            // scaffolding; append into it rather than creating a new block.
            auto* block = &func->getEntryBlock();
            auto* state = func->getArg(0);

            llvm::IRBuilder<> ir(block);
            auto [nextPcAddr, nextPcTy] =
                lifter.LoadRegAddress(block, state, remill::kNextPCVariableName);
            ir.CreateStore(llvm::ConstantInt::get(nextPcTy, pc), nextPcAddr);

            // Some forms decode but have no lifting support (e.g. MRS/MSR of
            // unknown system registers). Drop the stub and keep walking.
            if (lifter.LiftIntoBlock(inst, block, state) != remill::kLiftedInstruction) {
                func->eraseFromParent();
            } else {
                auto [memAddr, memTy] =
                    lifter.LoadRegAddress(block, state, remill::kMemoryVariableName);
                ir.SetInsertPoint(block);
                ir.CreateRet(ir.CreateLoad(memTy, memAddr));

                if (llvm::verifyFunction(*func, &llvm::errs()))
                    throw std::runtime_error("Lifted function failed verification");

                result.CodeOffsets.push_back(pc);
            }

            if (inst.IsDirectControlFlow())
                worklist.push_back(inst.branch_taken_pc);
            const bool uncondJump = inst.IsDirectControlFlow() &&
                                    !inst.IsConditionalBranch() && !inst.IsFunctionCall();
            const bool indirectNoReturn =
                inst.IsIndirectControlFlow() && !inst.IsFunctionCall();
            if (inst.IsConditionalBranch() ||
                (!uncondJump && !indirectNoReturn && !inst.IsFunctionReturn())) {
                // Fallthrough: normal flow, untaken conditional edge, call return.
                worklist.push_back(inst.next_pc);
            }
        }

        if (result.CodeOffsets.empty())
            return result;

        if (const char* pre = std::getenv("ANYSWITCH_DUMP_PRE")) {
            (void)pre;
            std::error_code ec;
            llvm::raw_fd_ostream bcOut("/tmp/asw_pre.bc", ec);
            if (!ec)
                llvm::WriteBitcodeToFile(*mod, bcOut);
        }

        if (std::getenv("ANYSWITCH_NO_STRIP"))
            std::cerr << "Strip disabled by env\n";
        else
            StripExpectIntrinsics(*mod);

        std::string verifyErr;
        llvm::raw_string_ostream verifyOs(verifyErr);
        if (llvm::verifyModule(*mod, &verifyOs))
            throw std::runtime_error("Lifted module failed verification: " + verifyErr);

        Optimize(*mod);

        std::map<std::string, std::uint64_t> symAddrs;
        result.MachineCode = EmitObject(*mod, symAddrs);

        // Debug aid (env-gated, never in default output):
        //   ANYSWITCH_DUMP_LIFTED=sym -> print symbol table
        //   ANYSWITCH_DUMP_LIFTED=bin -> write .text to /tmp/asw_text.bin
        //   ANYSWITCH_DUMP_LIFTED=bc  -> write pre-emit module to /tmp/asw_preemit.bc
        if (const char* dump = std::getenv("ANYSWITCH_DUMP_LIFTED")) {
            const std::string mode = dump;
            if (mode == "sym") {
                std::cout << "Lifted symbols:\n";
                for (const auto& [name, off] : symAddrs)
                    std::cout << "  +" << off << " " << name << "\n";
            } else if (mode == "bin") {
                FILE* f = std::fopen("/tmp/asw_text.bin", "wb");
                if (f) {
                    std::fwrite(result.MachineCode.data(), 1, result.MachineCode.size(), f);
                    std::fclose(f);
                }
            } else if (mode == "bc") {
                std::error_code ec;
                llvm::raw_fd_ostream bcOut("/tmp/asw_preemit.bc", ec);
                if (!ec)
                    llvm::WriteBitcodeToFile(*mod, bcOut);
            }
        }

        std::cout << "Lifted " << result.CodeOffsets.size() << " AArch64 instructions -> "
                  << result.MachineCode.size() << " bytes of x86-64\n";
        return result;
    }

private:
    static void LoadSemantics(const remill::Arch& arch, llvm::Module& mod) {
        const auto path = SemanticsPath();
        if (path.empty())
            throw std::runtime_error(
                "AArch64 semantics bitcode not configured (ANYSWITCH_AARCH64_BC)");
        llvm::SMDiagnostic err;
        auto sem = llvm::parseIRFile(path, err, mod.getContext());
        if (!sem) {
            std::string msg;
            llvm::raw_string_ostream os(msg);
            err.print("remill-translator", os);
            throw std::runtime_error("Cannot parse semantics bitcode: " + msg);
        }
        if (llvm::Linker::linkModules(mod, std::move(sem)))
            throw std::runtime_error("Cannot link AArch64 semantics");
        // Populates register tables and the arch intrinsic table; required
        // before any DecodeInstruction/LiftIntoBlock call.
        arch.InitFromSemanticsModule(&mod);
    }
};

std::unique_ptr<IArm64Translator> MakeRemillArm64Translator() {
    return std::make_unique<RemillArm64Translator>();
}

} // namespace Codegen

#else

namespace Codegen {

// Stub when Remill is not available.
std::unique_ptr<IArm64Translator> MakeRemillArm64Translator() {
    return nullptr;
}

} // namespace Codegen

#endif
