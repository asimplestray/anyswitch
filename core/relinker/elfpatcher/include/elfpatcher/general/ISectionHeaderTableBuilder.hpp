#ifndef ELFPATCHER_GENERAL_ISECTIONHEADERTABLEBUILDER_HPP
#define ELFPATCHER_GENERAL_ISECTIONHEADERTABLEBUILDER_HPP

#include <vector>
#include <cstdint>
#include <cstddef>

namespace Elfpatcher {

struct SectionHeaderTableRequest;

class ISectionHeaderTableBuilder {
public:
    virtual ~ISectionHeaderTableBuilder() = default;
    virtual void WriteTable(std::vector<std::uint8_t>& buf, const SectionHeaderTableRequest& req) = 0;
};

}

#endif
