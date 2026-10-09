#include <relinker/parsing/NroReader.hpp>
#include <relinker/parsing/ElfImageBuilder.hpp>
#include <domain/Types.hpp>
#include <cstring>

namespace Relinker {

namespace {

constexpr std::size_t kNroMagicOffset = 0x10;
constexpr std::size_t kNroHeaderSize = 0x80;
constexpr std::uint32_t kMaxSegmentSize = 256u * 1024u * 1024u; // 256 MiB sanity cap

std::uint32_t ReadU32Le(const std::vector<std::uint8_t>& buf, std::size_t off) {
    if (off + 4 > buf.size())
        throw Domain::RelinkerException("NRO header out of bounds", off);
    std::uint32_t v = 0;
    std::memcpy(&v, buf.data() + off, 4);
    return v;
}

} // namespace

bool NroReader::IsNro(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= kNroMagicOffset + 4 && bytes[kNroMagicOffset] == 'N' &&
           bytes[kNroMagicOffset + 1] == 'R' && bytes[kNroMagicOffset + 2] == 'O' &&
           bytes[kNroMagicOffset + 3] == '0';
}

NroImage NroReader::Parse(const std::vector<std::uint8_t>& bytes) {
    if (!IsNro(bytes) || bytes.size() < kNroHeaderSize)
        throw Domain::RelinkerException("Not an NRO0 image (bad magic or truncated header)");

    NroImage image;
    image.version = ReadU32Le(bytes, 0x14);
    if (image.version != 0)
        throw Domain::RelinkerException("Unsupported NRO version", image.version);

    image.size = ReadU32Le(bytes, 0x18);
    image.text.memoryOffset = ReadU32Le(bytes, 0x20);
    image.text.size = ReadU32Le(bytes, 0x24);
    image.rodata.memoryOffset = ReadU32Le(bytes, 0x28);
    image.rodata.size = ReadU32Le(bytes, 0x2C);
    image.data.memoryOffset = ReadU32Le(bytes, 0x30);
    image.data.size = ReadU32Le(bytes, 0x34);
    image.bssSize = ReadU32Le(bytes, 0x38);

    // The size field holds the segment bytes (real NROs exclude the header),
    // so it must at least cover the three segments and fit in the file.
    // Appended ASET assets may extend the file beyond it.
    const std::uint64_t segSum = static_cast<std::uint64_t>(image.text.size) +
                                 image.rodata.size + image.data.size;
    if (image.size < segSum || image.size > bytes.size())
        throw Domain::RelinkerException("NRO size field out of bounds", image.size);

    // Segments are contiguous from the end of the 0x80-byte header.
    // Note: the NRO size field holds the segment bytes only (excludes the
    // header) and assets may be appended after the body, so extents are
    // validated against the actual file size.
    std::size_t cursor = kNroHeaderSize;
    for (NroSegment* seg : {&image.text, &image.rodata, &image.data}) {
        if (seg->size > kMaxSegmentSize)
            throw Domain::RelinkerException("NRO segment too large", seg->size);
        if (cursor + seg->size > bytes.size())
            throw Domain::RelinkerException("NRO segment out of bounds", cursor);
        seg->bytes.assign(bytes.begin() + cursor, bytes.begin() + cursor + seg->size);
        cursor += seg->size;
    }
    if (image.bssSize > kMaxSegmentSize)
        throw Domain::RelinkerException("NRO .bss too large", image.bssSize);

    // .dynstr/.dynsym extents (relative to .rodata start)
    image.dynStrOffset = ReadU32Le(bytes, 0x70);
    image.dynStrSize = ReadU32Le(bytes, 0x74);
    image.dynSymOffset = ReadU32Le(bytes, 0x78);
    image.dynSymSize = ReadU32Le(bytes, 0x7C);
    return image;
}

std::vector<std::uint8_t> NroReader::ConvertToElf(const NroImage& image) {
    RawSegment text{image.text.bytes, image.text.memoryOffset};
    RawSegment rodata{image.rodata.bytes, image.rodata.memoryOffset};
    RawSegment data{image.data.bytes, image.data.memoryOffset};
    return WrapSegmentsToElf(text, rodata, data, image.bssSize, image.text.memoryOffset);
}

} // namespace Relinker
