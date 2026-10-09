#ifndef ELFPATCHER_GENERAL_SEGMENTFILTER_HPP
#define ELFPATCHER_GENERAL_SEGMENTFILTER_HPP

#include <domain/Types.hpp>
#include <vector>

namespace Elfpatcher {

class ISegmentFilter {
public:
    virtual ~ISegmentFilter() = default;
    virtual std::vector<Domain::ProgramHeader> Filter(const std::vector<Domain::ProgramHeader>& headers) = 0;
};

class SegmentFilter : public ISegmentFilter {
public:
    std::vector<Domain::ProgramHeader> Filter(const std::vector<Domain::ProgramHeader>& headers) override;
};

}

#endif
