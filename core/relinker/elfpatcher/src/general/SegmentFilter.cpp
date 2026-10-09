#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <algorithm>

namespace Elfpatcher {

std::vector<Domain::ProgramHeader> SegmentFilter::Filter(const std::vector<Domain::ProgramHeader>& headers) {
    std::vector<Domain::ProgramHeader> result;
    for (const auto& ph : headers) {
        // Keep LOAD, DYNAMIC, PHDR, INTERP segments; filter others not needed after relink
        if (ph.Type == PT_LOAD || ph.Type == PT_DYNAMIC || ph.Type == 6 || ph.Type == PT_INTERP)
            result.push_back(ph);
    }
    return result;
}

}
