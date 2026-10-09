#ifndef ELFPATCHER_GENERAL_PROGRAMHEADERLAYOUTBUILDER_HPP
#define ELFPATCHER_GENERAL_PROGRAMHEADERLAYOUTBUILDER_HPP

#include <elfpatcher/general/IProgramHeaderLayoutBuilder.hpp>
#include <vector>
#include <algorithm>

namespace Elfpatcher {

class ProgramHeaderLayoutBuilder : public IProgramHeaderLayoutBuilder {
public:
    std::uint16_t WriteLayout(std::vector<std::uint8_t>& buf, const ProgramHeaderLayoutRequest& req) override;
    std::uint64_t ComputeExtraBlockVaddr(const std::vector<Domain::ProgramHeader>& headers, std::uint64_t extraBlockOffset) override;
};

}

#endif
