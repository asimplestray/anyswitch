#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>

namespace Cli {

struct Args {
    std::string inputPath;
    std::string outputPath;
    std::string runPath;
    bool toWindows = false;
    bool toIntel = false;
    bool lazyBinding = false;
    int unusedFilterLevel = 0;
    bool writeRegistry = false;
    bool autorun = false;
    bool windowsGui = false;
    bool windowsDiagnostics = false;
    bool skipSyscallCheck = false;
    bool skipGuestModule = false;
    std::set<std::string> excludedModules;
};

Args ParseArgs(int argc, char* argv[]);

}
