#include <relinker/parsing/NsoReader.hpp>
#include <relinker/parsing/ElfImageBuilder.hpp>
#include <domain/Types.hpp>
#include <cstring>

namespace Relinker {

namespace {

constexpr std::size_t kNsoHeaderSize = 0x100;
constexpr std::uint32_t kMaxSegmentSize = 256u * 1024u * 1024u; // 256 MiB sanity cap

std::uint32_t ReadU32Le(const std::vector<std::uint8_t>& buf, std::size_t off) {
    if (off + 4 > buf.size())
        throw Domain::RelinkerException("NSO header out of bounds", off);
    std::uint32_t v = 0;
    std::memcpy(&v, buf.data() + off, 4);
    return v;
}

} // namespace

bool NsoReader::IsNso(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= 4 && bytes[0] == 'N' && bytes[1] == 'S' &&
           bytes[2] == 'O' && bytes[3] == '0';
}

bool NsoReader::Lz4Decompress(const std::uint8_t* src, std::size_t srcSize,
                              std::uint8_t* dst, std::size_t dstSize) {
    std::size_t ip = 0;
    std::size_t op = 0;
    while (ip < srcSize) {
        const std::uint8_t token = src[ip++];

        std::size_t litLen = token >> 4;
        if (litLen == 15) {
            std::uint8_t s = 0;
            do {
                if (ip >= srcSize)
                    return false;
                s = src[ip++];
                if (litLen + s < litLen)
                    return false;
                litLen += s;
            } while (s == 255);
        }
        if (litLen > srcSize - ip || litLen > dstSize - op)
            return false;
        std::memcpy(dst + op, src + ip, litLen);
        ip += litLen;
        op += litLen;
        if (ip >= srcSize)
            break; // trailing literals end the block

        if (ip + 2 > srcSize)
            return false;
        const std::size_t offset =
            static_cast<std::size_t>(src[ip]) |
            (static_cast<std::size_t>(src[ip + 1]) << 8);
        ip += 2;
        if (offset == 0 || offset > op)
            return false;

        std::size_t matchLen = (token & 0x0F) + 4;
        if ((token & 0x0F) == 15) {
            std::uint8_t s = 0;
            do {
                if (ip >= srcSize)
                    return false;
                s = src[ip++];
                if (matchLen + s < matchLen)
                    return false;
                matchLen += s;
            } while (s == 255);
        }
        if (matchLen > dstSize - op)
            return false;
        for (std::size_t i = 0; i < matchLen; ++i)
            dst[op + i] = dst[op - offset + i];
        op += matchLen;
    }
    return op == dstSize;
}

namespace {

NsoSegment ReadSegment(const std::vector<std::uint8_t>& buf, std::size_t hdrOff,
                       std::size_t csizeOff, bool compressed, const char* name) {
    NsoSegment seg;
    seg.fileOffset = ReadU32Le(buf, hdrOff);
    seg.memoryOffset = ReadU32Le(buf, hdrOff + 4);
    seg.decompressedSize = ReadU32Le(buf, hdrOff + 8);
    seg.compressedSize = ReadU32Le(buf, csizeOff);
    seg.compressed = compressed;

    if (seg.fileOffset < kNsoHeaderSize)
        throw Domain::RelinkerException(std::string("NSO ") + name + ": file offset inside header",
                                        seg.fileOffset);
    if (seg.decompressedSize > kMaxSegmentSize)
        throw Domain::RelinkerException(std::string("NSO ") + name + ": segment too large",
                                        seg.decompressedSize);

    if (compressed) {
        if (seg.compressedSize == 0 || seg.fileOffset + seg.compressedSize > buf.size())
            throw Domain::RelinkerException(std::string("NSO ") + name + ": compressed extent out of bounds",
                                            seg.fileOffset);
        seg.bytes.resize(seg.decompressedSize);
        if (!NsoReader::Lz4Decompress(buf.data() + seg.fileOffset, seg.compressedSize,
                                      seg.bytes.data(), seg.bytes.size()))
            throw Domain::RelinkerException(std::string("NSO ") + name + ": LZ4 decompression failed",
                                            seg.fileOffset);
    } else {
        if (seg.fileOffset + seg.decompressedSize > buf.size())
            throw Domain::RelinkerException(std::string("NSO ") + name + ": segment out of bounds",
                                            seg.fileOffset);
        seg.bytes.assign(buf.begin() + seg.fileOffset,
                         buf.begin() + seg.fileOffset + seg.decompressedSize);
    }
    return seg;
}

} // namespace

NsoImage NsoReader::Parse(const std::vector<std::uint8_t>& bytes) {
    if (!IsNso(bytes) || bytes.size() < kNsoHeaderSize)
        throw Domain::RelinkerException("Not an NSO0 image (bad magic or truncated header)");

    const std::uint32_t version = ReadU32Le(bytes, 0x04);
    if (version != 0)
        throw Domain::RelinkerException("Unsupported NSO version", version);

    const std::uint32_t flags = ReadU32Le(bytes, 0x0C);
    if (flags & 0x80)
        throw Domain::RelinkerException("NSO uses ZBIC (zstd) compression, only LZ4 is supported");
    if (flags & 0xFFFFFF00u)
        throw Domain::RelinkerException("NSO has unknown flag bits", flags);

    NsoImage image;
    image.flags = flags;
    image.text = ReadSegment(bytes, 0x10, 0x60, (flags & 0x01) != 0, ".text");
    image.rodata = ReadSegment(bytes, 0x20, 0x64, (flags & 0x02) != 0, ".rodata");
    image.data = ReadSegment(bytes, 0x30, 0x68, (flags & 0x04) != 0, ".data");
    image.bssSize = ReadU32Le(bytes, 0x3C);
    if (image.bssSize > kMaxSegmentSize)
        throw Domain::RelinkerException("NSO .bss too large", image.bssSize);
    return image;
}

std::vector<std::uint8_t> NsoReader::ConvertToElf(const NsoImage& image) {
    RawSegment text{image.text.bytes, image.text.memoryOffset};
    RawSegment rodata{image.rodata.bytes, image.rodata.memoryOffset};
    RawSegment data{image.data.bytes, image.data.memoryOffset};
    return WrapSegmentsToElf(text, rodata, data, image.bssSize, image.text.memoryOffset);
}

} // namespace Relinker
