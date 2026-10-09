#ifndef ELFPATCHER_GENERAL_ENTRYSTUBBUILDER_HPP
#define ELFPATCHER_GENERAL_ENTRYSTUBBUILDER_HPP

#include <elfpatcher/general/IEntryStubBuilder.hpp>
#include <vector>
#include <cstdint>

namespace Elfpatcher {

class EntryStubBuilder : public IEntryStubBuilder {
public:
    std::vector<std::uint8_t> BuildEntryStub(std::uint64_t stubVaddr, std::uint64_t entryVaddr) override;
};

}

#endif
