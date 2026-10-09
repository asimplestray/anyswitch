#ifndef RELINKER_OUTPUT_CALLREGISTRYWRITER_HPP
#define RELINKER_OUTPUT_CALLREGISTRYWRITER_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace Relinker {

struct CallRegistryEntry {
    std::string FunctionName;
    std::string Library;
    std::uint64_t Offset;
    std::string RelocationType;
};

class CallRegistryWriter {
public:
    std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& entries);
};

}

#endif
