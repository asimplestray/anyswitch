#pragma once

#include <cstdint>
#include <vector>

namespace Relinker {

// Wraps raw Switch code segments in a minimal ELF64LE AArch64 image
// (EHDR + PT_LOAD text/rodata/data + a DT_NULL-only PT_DYNAMIC placed
// inside the data file range so TranslateVirtualAddress() resolves it).
// Shared by the NSO and NRO loaders so both feed the existing
// ElfReader/RelinkerPipeline unchanged.

struct RawSegment {
    std::vector<std::uint8_t> bytes;
    std::uint32_t memoryOffset = 0;
};

std::vector<std::uint8_t> WrapSegmentsToElf(const RawSegment& text,
                                             const RawSegment& rodata,
                                             const RawSegment& data,
                                             std::uint32_t bssSize,
                                             std::uint64_t entryPoint);

} // namespace Relinker
