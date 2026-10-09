#ifndef ELFPATCHER_GENERAL_IPROGRAMHEADERLAYOUTBUILDER_HPP
#define ELFPATCHER_GENERAL_IPROGRAMHEADERLAYOUTBUILDER_HPP

#include <domain/Types.hpp>
#include <vector>
#include <cstdint>

namespace Elfpatcher {

struct ProgramHeaderLayoutRequest {
    std::uint64_t PhOff;
    std::uint16_t PhEntSize;
    std::uint16_t PhNum;
    std::vector<Domain::ProgramHeader> OriginalHeaders;
    std::uint64_t ExtraBlockOffset;
    std::uint64_t ExtraBlockVaddr;
    std::uint64_t ExtraBlockSize;
    std::uint64_t DynamicSegmentOffset;
    std::size_t DynamicSegmentSize;
    std::uint64_t InterpOffset;
    std::size_t InterpSize;
};

class IProgramHeaderLayoutBuilder {
public:
    virtual ~IProgramHeaderLayoutBuilder() = default;
    virtual std::uint16_t WriteLayout(std::vector<std::uint8_t>& buf, const ProgramHeaderLayoutRequest& req) = 0;
    virtual std::uint64_t ComputeExtraBlockVaddr(const std::vector<Domain::ProgramHeader>& headers, std::uint64_t extraBlockOffset) = 0;
};

}

#endif
