#ifndef ELFPATCHER_GENERAL_SECTIONHEADERTABLEBUILDER_HPP
#define ELFPATCHER_GENERAL_SECTIONHEADERTABLEBUILDER_HPP

#include <elfpatcher/general/ISectionHeaderTableBuilder.hpp>
#include <vector>
#include <cstdint>

namespace Elfpatcher {

struct SectionHeaderTableRequest {
    std::uint64_t DynStrOffset;
    std::size_t DynStrSize;
    std::uint64_t DynSymOffset;
    std::size_t DynSymSize;
    std::uint64_t DynamicSegmentOffset;
    std::size_t DynamicSegmentSize;
    std::uint64_t StubOffset;
    std::size_t StubSize;
};

class SectionHeaderTableBuilder : public ISectionHeaderTableBuilder {
public:
    void WriteTable(std::vector<std::uint8_t>& buf, const SectionHeaderTableRequest& req) override;
};

}

#endif
