#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>

namespace Elfpatcher {

void SectionHeaderTableBuilder::WriteTable(std::vector<std::uint8_t>& buf, const SectionHeaderTableRequest& req) {
    // Build section headers for the rewritten ELF
    // .dynstr
    std::uint64_t dynStrOff = req.DynStrOffset;
    std::size_t dynStrSize = req.DynStrSize;

    // .dynsym
    std::uint64_t dynSymOff = req.DynSymOffset;
    std::size_t dynSymSize = req.DynSymSize;

    // .dynamic
    std::uint64_t dynSegOff = req.DynamicSegmentOffset;
    std::size_t dynSegSize = req.DynamicSegmentSize;

    // .text (entry stub)
    std::uint64_t stubOff = req.StubOffset;
    std::size_t stubSize = req.StubSize;

    // In a full implementation, we'd write the section header table entries
    // at the end of the file. For now, this is a stub.
}

}
