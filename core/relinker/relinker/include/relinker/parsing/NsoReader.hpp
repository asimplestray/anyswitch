#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Relinker {

// Nintendo Switch NSO0 loader.
//
// Layout (see the switchbrew NSO0 format documentation): a 0x100-byte header
// with three segment extents (.text/.rodata/.data) plus LZ4-compressed payloads.
// Compressed sizes live at 0x60/0x64/0x68; flag bits 0-2 mark compressed
// segments and bit 7 selects ZBIC (zstd) instead of LZ4.
//
// ConvertToElf() wraps the decompressed segments in a minimal ELF64LE
// AArch64 image (PT_LOADs + a DT_NULL-only PT_DYNAMIC) so the existing
// ElfReader/RelinkerPipeline can consume NSO input unchanged.
//
// Limitations (alpha): no SHA-256 verification, no ZBIC support, dynamic
// symbols are rebuilt downstream from an empty set — real NSO .dynamic
// extents (0x90-0x9C) are not parsed yet.

struct NsoSegment {
    std::uint32_t fileOffset = 0;
    std::uint32_t memoryOffset = 0;
    std::uint32_t decompressedSize = 0;
    std::uint32_t compressedSize = 0;
    bool compressed = false;
    std::vector<std::uint8_t> bytes; // decompressed payload
};

struct NsoImage {
    std::uint32_t flags = 0;
    NsoSegment text;
    NsoSegment rodata;
    NsoSegment data;
    std::uint32_t bssSize = 0;
    // .dynstr/.dynsym extents (relative to .rodata start, 0 if absent)
    std::uint32_t dynStrOffset = 0;
    std::uint32_t dynStrSize = 0;
    std::uint32_t dynSymOffset = 0;
    std::uint32_t dynSymSize = 0;
};

class NsoReader {
public:
    static bool IsNso(const std::vector<std::uint8_t>& bytes);
    static NsoImage Parse(const std::vector<std::uint8_t>& bytes);
    static std::vector<std::uint8_t> ConvertToElf(const NsoImage& image);

    // Raw LZ4 block decompression (NSO uses raw blocks, not frames).
    // Returns true iff exactly dstSize bytes were produced.
    static bool Lz4Decompress(const std::uint8_t* src, std::size_t srcSize,
                              std::uint8_t* dst, std::size_t dstSize);
};

} // namespace Relinker
