#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Relinker {

// Nintendo Switch NRO0 loader (homebrew format).
//
// Layout (see switchbrew NRO docs): magic "NRO0" at file offset 0x10,
// then text/rodata/data memory offsets + sizes and the .bss size.
// Segments are stored uncompressed and contiguously starting at file
// offset 0x80 (text, rodata, data). Optional ASET assets (icon/NACP)
// may be appended after the NRO body and are ignored.
//
// ConvertToElf() wraps the segments via WrapSegmentsToElf so the existing
// ElfReader/RelinkerPipeline consume NRO input unchanged.
//
// Limitations (alpha): entry is assumed at .text start (MOD0 parsing is
// future work), and .dynstr/.dynsym extents (0x70-0x7C) are not parsed yet.

struct NroSegment {
    std::uint32_t memoryOffset = 0;
    std::uint32_t size = 0;
    std::vector<std::uint8_t> bytes;
};

struct NroImage {
    std::uint32_t version = 0;
    std::uint32_t size = 0; // NRO body size (excludes appended assets)
    NroSegment text;
    NroSegment rodata;
    NroSegment data;
    std::uint32_t bssSize = 0;
    // .dynstr/.dynsym extents (relative to .rodata start, 0 if absent)
    std::uint32_t dynStrOffset = 0;
    std::uint32_t dynStrSize = 0;
    std::uint32_t dynSymOffset = 0;
    std::uint32_t dynSymSize = 0;
};

class NroReader {
public:
    static bool IsNro(const std::vector<std::uint8_t>& bytes);
    static NroImage Parse(const std::vector<std::uint8_t>& bytes);
    static std::vector<std::uint8_t> ConvertToElf(const NroImage& image);
};

} // namespace Relinker
