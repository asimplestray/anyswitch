#ifndef ELFPATCHER_GENERAL_IENTRYSTUBBUILDER_HPP
#define ELFPATCHER_GENERAL_IENTRYSTUBBUILDER_HPP

#include <vector>
#include <cstdint>
#include <string>

namespace Elfpatcher {

class IEntryStubBuilder {
public:
    virtual ~IEntryStubBuilder() = default;
    virtual std::vector<std::uint8_t> BuildEntryStub(std::uint64_t stubVaddr, std::uint64_t entryVaddr) = 0;
};

}

#endif
