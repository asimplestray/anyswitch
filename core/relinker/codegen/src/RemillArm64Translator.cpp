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
#include <remill/BC/TraceLifter.h>
#include <remill/BC/Util.h>
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
#include <filesystem>
#include <fstream>
#include <unordered_map>
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

void WriteObjectToFile(const llvm::Module& mod, const char* path) {
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
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec);
    if (ec)
        throw std::runtime_error("Cannot open object output: " + ec.message());
    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, llvm::CodeGenFileType::ObjectFile))
        throw std::runtime_error("TargetMachine cannot emit object files");
    auto& mutableMod = const_cast<llvm::Module&>(mod);
    mutableMod.setTargetTriple(llvm::Triple(kX86Triple));
    mutableMod.setDataLayout(tm->createDataLayout());
    pm.run(mutableMod);
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
    // Internalize everything except our trace functions so GlobalDCE can
    // actually drop the dead 99%. Traces stay external as the roots.
    for (auto& func : mod.functions()) {
        if (!func.isDeclaration() && !func.getName().starts_with("asw_trace_"))
            func.setLinkage(llvm::GlobalValue::InternalLinkage);
    }
    mpm.addPass(llvm::GlobalDCEPass());
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
                                      std::map<std::string, std::uint64_t>& outAddrs,
                                      std::vector<std::uint8_t>* objOut) {
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

    if (objOut)
        objOut->assign(objBytes.begin(), objBytes.end());

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

// Trace manager backed by the guest code buffer. Gives Remill byte-level
// access for decoding and records every lifted trace.
class CodeTraceManager : public remill::TraceManager {
public:
    CodeTraceManager(const std::vector<std::uint8_t>& code, Domain::VirtualAddress base)
        : _code(code), _base(base) {}

    // Name traces predictably so the fixup pass can find them in the object
    // symbol table: asw_trace_<guest addr in hex>.
    std::string TraceName(std::uint64_t addr) override {
        std::ostringstream os;
        os << "asw_trace_" << std::hex << addr;
        return os.str();
    }

    void SetLiftedTraceDefinition(std::uint64_t addr, llvm::Function* lifted) override {
        _traces[addr] = lifted;
    }

    llvm::Function* GetLiftedTraceDeclaration(std::uint64_t addr) override {
        auto it = _traces.find(addr);
        return it == _traces.end() ? nullptr : it->second;
    }

    llvm::Function* GetLiftedTraceDefinition(std::uint64_t addr) override {
        return GetLiftedTraceDeclaration(addr);
    }

    bool TryReadExecutableByte(std::uint64_t addr, std::uint8_t* byte) override {
        if (addr < _base)
            return false;
        const auto off = static_cast<std::size_t>(addr - _base);
        if (off >= _code.size())
            return false;
        *byte = _code[off];
        return true;
    }

    std::size_t LiftedCount() const { return _traces.size(); }

private:
    const std::vector<std::uint8_t>& _code;
    Domain::VirtualAddress _base;
    std::unordered_map<std::uint64_t, llvm::Function*> _traces;
};

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

        // Build without the global cache so each Translate gets a clean arch.
        auto arch = remill::Arch::Build(&context, remill::kOSLinux,
                                        remill::kArchAArch64LittleEndian);
        if (!arch)
            throw std::runtime_error("Remill has no AArch64 backend");

        // LoadArchSemantics also calls PrepareModule + InitFromSemanticsModule,
        // which is what populates the arch's intrinsic table. Skipping that was
        // the cause of the old segfaults on re-decode.
        auto sem = SemanticsDir();
        std::vector<std::filesystem::path> semDirs;
        if (!sem.empty())
            semDirs.emplace_back(sem);
        mod = remill::LoadArchSemantics(arch.get(), semDirs);
        if (!mod)
            throw std::runtime_error("Cannot load AArch64 semantics bitcode");

        // Link the intrinsics runtime so the pure helpers (__remill_compare_*)
        // resolve and get inlined instead of staying undefined externs.
        if (const char* ic = std::getenv("ANYSWITCH_REMILL_INTRINSICS_BC")) {
            if (*ic) {
                llvm::SMDiagnostic err;
                auto intr = llvm::parseIRFile(ic, err, context);
                if (!intr) {
                    std::string msg;
                    llvm::raw_string_ostream os(msg);
                    err.print("anyswitch", os);
                    throw std::runtime_error("Cannot parse intrinsics bitcode: " + msg);
                }
                if (llvm::Linker::linkModules(*mod, std::move(intr)))
                    throw std::runtime_error("Cannot link AArch64 intrinsics");
            }
        }

        CodeTraceManager manager(arm64Code, baseVAddr);
        remill::TraceLifter traceLifter(arch.get(), manager);

        // Pass 1 — discovery. Walk control flow from the entry point to find
        // every reachable instruction and the set of trace heads (the entry
        // plus every direct branch/call target). TraceLifter itself only
        // follows block-external edges it is told about, so we seed it.
        const auto codeEnd = baseVAddr + arm64Code.size();
        auto inRange = [&](std::uint64_t pc) {
            return pc >= baseVAddr && pc + kAArch64InstrSize <= codeEnd &&
                   (pc % kAArch64InstrSize) == 0;
        };
        std::set<std::uint64_t> visited;
        std::vector<std::uint64_t> worklist{baseVAddr};
        // Trace heads, in discovery order, deduplicated.
        std::set<std::uint64_t> traceHeads{baseVAddr};
        // Indirect call/jump sites (guest addr, is_call) for the fixup pass.
        std::vector<std::pair<std::uint64_t, bool>> indirectSites;

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

            result.CodeOffsets.push_back(pc);

            if (inst.IsIndirectControlFlow()) {
                indirectSites.emplace_back(pc, inst.IsFunctionCall());
                continue; // target unknown statically; ends the trace
            }

            if (inst.IsDirectControlFlow()) {
                traceHeads.insert(inst.branch_taken_pc);
                worklist.push_back(inst.branch_taken_pc);
                if (!inst.IsConditionalBranch() && !inst.IsFunctionCall()) {
                    // Unconditional jump: fallthrough is unreachable.
                    continue;
                }
            }
            if (!inst.IsFunctionReturn())
                worklist.push_back(inst.next_pc);
        }

        // Pass 2 — lift every trace head with real control flow. Each call
        // produces one function with proper blocks and branch edges; traces
        // calling each other compile to direct calls via the manager.
        for (const auto head : traceHeads) {
            if (!inRange(head))
                continue;
            traceLifter.Lift(head);
        }

        if (manager.LiftedCount() == 0) {
            result.CodeOffsets.clear();
            return result;
        }

        // The semantics bitcode carries @llvm.compiler.used (900+ entries) to
        // defeat DCE at Remill's own build time. We want the opposite: drop it
        // so only what our traces call survives.
        for (const char* name : {"llvm.compiler.used", "llvm.used"}) {
            if (auto* used = mod->getGlobalVariable(name, true); used && used->use_empty())
                used->eraseFromParent();
        }

        // The linked semantics bring ~500 functions; only a handful are called.
        // Internalize everything except our trace functions so GlobalDCE can
        // actually drop the dead 99%. Traces stay external as the roots.
        for (auto& func : mod->functions()) {
            if (!func.isDeclaration() && !func.getName().starts_with("asw_trace_"))
                func.setLinkage(llvm::GlobalValue::InternalLinkage);
        }

        StripExpectIntrinsics(*mod);

        std::string verifyErr;
        llvm::raw_string_ostream verifyOs(verifyErr);
        if (llvm::verifyModule(*mod, &verifyOs))
            throw std::runtime_error("Lifted module failed verification: " + verifyErr);

        Optimize(*mod);

        std::map<std::string, std::uint64_t> symAddrs;
        std::vector<std::uint8_t> objectBytes;
        result.MachineCode = EmitObject(*mod, symAddrs, &objectBytes);

        // Emit fixups for indirect calls/jumps (BLR/BR) — target unknown
        // statically. Sites were collected during discovery because re-decoding
        // after the optimizer would touch freed intrinsics.
        for (const auto& [guestAddr, isCall] : indirectSites) {
            const auto it = symAddrs.find(TraceSymbolName(guestAddr));
            if (it == symAddrs.end())
                continue; // trace failed to lift
            result.Fixups.push_back({
                it->second,
                0,
                isCall ? 0x1u : 0x2u, // 1=call, 2=jump
                true,                // relative
                false                // not PLT
            });
        }

        // Debug aid (env-gated, never in default output):
        //   ANYSWITCH_DUMP_LIFTED=sym -> print symbol table
        //   ANYSWITCH_DUMP_LIFTED=bin -> write .text to /tmp/asw_text.bin
        //   ANYSWITCH_DUMP_LIFTED=bc  -> write pre-emit module to /tmp/asw_preemit.bc
        //   ANYSWITCH_EMIT_OBJECT=<p> -> write the relocatable object to <p>
        if (const char* obj = std::getenv("ANYSWITCH_EMIT_OBJECT")) {
            if (*obj) {
                std::ofstream ofs(obj, std::ios::binary);
                if (!ofs)
                    throw std::runtime_error("Cannot open object output");
                ofs.write(reinterpret_cast<const char*>(objectBytes.data()),
                          static_cast<std::streamsize>(objectBytes.size()));
            }
        }        if (const char* dump = std::getenv("ANYSWITCH_DUMP_LIFTED")) {
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
    // Directory holding the aarch64.bc semantics. LoadArchSemantics searches
    // these dirs for the arch-named bitcode.
    static std::string SemanticsDir() {
        const std::string bc = SemanticsPath();
        if (bc.empty())
            return {};
        const auto slash = bc.find_last_of('/');
        if (slash == std::string::npos)
            return {};
        return bc.substr(0, slash);
    }

    static std::string TraceSymbolName(std::uint64_t guestAddr) {
        std::ostringstream os;
        os << "asw_trace_" << std::hex << guestAddr;
        return os.str();
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
