#include <Cli.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace Cli {

Args ParseArgs(int argc, char* argv[]) {
    Args args;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--windows") args.toWindows = true;
        else if (arg == "--windows-gui") args.windowsGui = true;
        else if (arg == "--windows-diagnostics") args.windowsDiagnostics = true;
        else if (arg == "--to-intel") args.toIntel = true;
        else if (arg == "--lazy-binding") args.lazyBinding = true;
        else if (arg == "--registry") args.writeRegistry = true;
        else if (arg == "--autorun") args.autorun = true;
        else if (arg == "--skip-syscall-check") args.skipSyscallCheck = true;
        else if (arg == "--skip-guest-module") args.skipGuestModule = true;
        else if (arg.starts_with("--rpath=")) args.runPath = std::string(arg.substr(8));
        else if (arg == "--rpath" && i + 1 < argc) args.runPath = std::string(argv[++i]);
        else if (arg.starts_with("unused-filter=")) {
            args.unusedFilterLevel = std::stoi(std::string(arg.substr(13)));
            if (args.unusedFilterLevel < 0 || args.unusedFilterLevel > 2)
                throw std::runtime_error("unused-filter must be 0, 1 or 2");
        }
        else if (arg == "--exclude-module" && i + 1 < argc) args.excludedModules.insert(argv[++i]);
        else if (arg.starts_with("--")) throw std::runtime_error("Unknown option: " + std::string(arg));
        else positional.push_back(std::string(arg));
    }

    if (positional.size() != 2) throw std::runtime_error("Usage: relinker [options] <input.nso> <output>");

    args.inputPath = positional[0];
    args.outputPath = positional[1];
    args.runPath = args.runPath.empty() ? "$ORIGIN/libs" : args.runPath;
    return args;
}

int Autorun(const std::string& path, bool windows) {
    if (!windows) {
#ifdef __linux__
        std::filesystem::permissions(path,
            std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec |
            std::filesystem::perms::owner_read | std::filesystem::perms::group_read |
            std::filesystem::perms::others_read);
#endif
    }
    std::string cmd = windows ? ("cmd /c " + path) : ("./" + path);
    return std::system(cmd.c_str());
}

} // namespace Cli
