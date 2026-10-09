#include <relinker/parsing/NsoReader.hpp>
#include <domain/Types.hpp>
#include <cstring>

namespace Relinker {

namespace {

constexpr std::size_t kNsoHeaderSize = 0x100;
constexpr std::uint32_t kMaxSegmentSize = 256u * 1024u * 1024u; // 256 MiB sanity cap

constexpr std::uint16_t kEmAarch64 = 183;
constexpr std::uint16_t kEtDyn = 3;
constexpr std::uint32_t kPtLoad = 1;
constexpr std::uint32_t kPtDynamic = 2;
constexpr std::uint32_t kPfX = 1;
constexpr std::uint32_t kPfW = 2;
constexpr std::uint32_t kPfR = 4;

std::uint32_t ReadU32Le(const std::vector<std::uint8_t>& buf, std::size_t off) {
    if (off + 4 > buf.size())
        throw Domain::RelinkerException("NSO header out of bounds", off);
    std::uint32_t v = 0;
    std::memcpy(&v, buf.data() + off, 4);
    return v;
}

void AppendU16Le(std::vector<std::uint8_t>& buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void AppendU32Le(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

void AppendU64Le(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

void AppendPhdr(std::vector<std::uint8_t>& buf, std::uint32_t type, std::uint32_t flags,
                std::uint64_t offset, std::uint64_t vaddr, std::uint64_t filesz,
                std::uint64_t memsz) {
    AppendU32Le(buf, type);
    AppendU32Le(buf, flags);
    AppendU64Le(buf, offset);
    AppendU64Le(buf, vaddr);
    AppendU64Le(buf, vaddr); // p_paddr = p_vaddr
    AppendU64Le(buf, filesz);
    AppendU64Le(buf, memsz);
    AppendU64Le(buf, 0x1000); // p_align
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
    // Minimal ELF64LE/AArch64: EHDR + 4 PHDRs, then raw segment bytes.
    // .dynamic (a single DT_NULL entry) is appended inside the data file
    // range so TranslateVirtualAddress() can resolve PT_DYNAMIC.
    constexpr std::uint64_t kEhdrSize = 64;
    constexpr std::uint64_t kPhdrSize = 56;
    constexpr std::uint32_t kPhNum = 4;
    constexpr std::uint64_t kHeaderEnd = kEhdrSize + kPhNum * kPhdrSize;

    auto align16 = [](std::uint64_t v) { return (v + 15) & ~std::uint64_t(15); };
    const std::uint64_t textOff = kHeaderEnd;
    const std::uint64_t roOff = align16(textOff + image.text.bytes.size());
    const std::uint64_t dataOff = align16(roOff + image.rodata.bytes.size());
    const std::uint64_t dynOff = dataOff + image.data.bytes.size();
    const std::uint64_t dynVaddr =
        static_cast<std::uint64_t>(image.data.memoryOffset) + image.data.bytes.size();
    constexpr std::uint64_t kDynSize = 16; // one Elf64_Dyn (DT_NULL)
    const std::uint64_t dataFileSize = image.data.bytes.size() + kDynSize;

    std::vector<std::uint8_t> elf;
    elf.reserve(dataOff + dataFileSize);

    // --- EHDR ---
    elf.push_back(0x7F);
    elf.push_back('E');
    elf.push_back('L');
    elf.push_back('F');
    elf.push_back(2); // ELFCLASS64
    elf.push_back(1); // ELFDATA2LSB
    elf.push_back(1); // EV_CURRENT
    elf.push_back(0); // ELFOSABI_SYSV
    for (int i = 0; i < 8; ++i)
        elf.push_back(0); // ABI version + padding
    AppendU16Le(elf, kEtDyn);
    AppendU16Le(elf, kEmAarch64);
    AppendU32Le(elf, 1); // e_version
    AppendU64Le(elf, image.text.memoryOffset); // e_entry
    AppendU64Le(elf, kEhdrSize); // e_phoff
    AppendU64Le(elf, 0); // e_shoff (none)
    AppendU32Le(elf, 0); // e_flags
    AppendU16Le(elf, static_cast<std::uint16_t>(kEhdrSize)); // e_ehsize
    AppendU16Le(elf, static_cast<std::uint16_t>(kPhdrSize)); // e_phentsize
    AppendU16Le(elf, kPhNum); // e_phnum
    AppendU16Le(elf, 0); // e_shentsize
    AppendU16Le(elf, 0); // e_shnum
    AppendU16Le(elf, 0); // e_shstrndx

    // --- PHDRs ---
    AppendPhdr(elf, kPtLoad, kPfR | kPfX, textOff, image.text.memoryOffset,
               image.text.bytes.size(), image.text.bytes.size());
    AppendPhdr(elf, kPtLoad, kPfR, roOff, image.rodata.memoryOffset,
               image.rodata.bytes.size(), image.rodata.bytes.size());
    AppendPhdr(elf, kPtLoad, kPfR | kPfW, dataOff, image.data.memoryOffset, dataFileSize,
               dataFileSize + image.bssSize);
    AppendPhdr(elf, kPtDynamic, kPfR | kPfW, dynOff, dynVaddr, kDynSize, kDynSize);

    // --- Payload ---
    while (elf.size() < textOff)
        elf.push_back(0);
    elf.insert(elf.end(), image.text.bytes.begin(), image.text.bytes.end());
    while (elf.size() < roOff)
        elf.push_back(0);
    elf.insert(elf.end(), image.rodata.bytes.begin(), image.rodata.bytes.end());
    while (elf.size() < dataOff)
        elf.push_back(0);
    elf.insert(elf.end(), image.data.bytes.begin(), image.data.bytes.end());
    for (std::uint64_t i = 0; i < kDynSize; ++i)
        elf.push_back(0); // DT_NULL tag + value

    return elf;
}

} // namespace Relinker
