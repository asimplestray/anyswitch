#include <relinker/output/CallRegistryWriter.hpp>
#include <sstream>

namespace Relinker {

std::string CallRegistryWriter::WriteCallRegistry(const std::vector<CallRegistryEntry>& entries) {
    std::ostringstream json;
    json << "{\n  \"imports\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        json << "    {\"function\": \"" << entries[i].FunctionName
             << "\", \"library\": \"" << entries[i].Library
             << "\", \"offset\": " << entries[i].Offset
             << ", \"type\": \"" << entries[i].RelocationType << "\""
             << "}";
        if (i + 1 < entries.size()) json << ",";
        json << "\n";
    }
    json << "  ]\n}\n";
    return json.str();
}

}
