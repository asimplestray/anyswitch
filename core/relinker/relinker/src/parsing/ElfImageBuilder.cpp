#include <relinker/parsing/ElfImageBuilder.hpp>

namespace Relinker {

namespace {

constexpr std::uint16_t kEmAarch64 = 183;
constexpr std::uint16_t kEtDyn = 3;
constexpr std::uint32_t kPtLoad = 1;
constexpr std::uint32_t kPtDynamic = 2;
constexpr std::uint32_t kPfX = 1;
constexpr std::uint32_t kPfW = 2;
constexpr std::uint32_t kPfR = 4;

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

std::vector<std::uint8_t> WrapSegmentsToElf(const RawSegment& text,
                                             const RawSegment& rodata,
                                             const RawSegment& data,
                                             std::uint32_t bssSize,
                                             std::uint64_t entryPoint) {
    constexpr std::uint64_t kEhdrSize = 64;
    constexpr std::uint64_t kPhdrSize = 56;
    constexpr std::uint32_t kPhNum = 4;
    constexpr std::uint64_t kHeaderEnd = kEhdrSize + kPhNum * kPhdrSize;

    auto align16 = [](std::uint64_t v) { return (v + 15) & ~std::uint64_t(15); };
    const std::uint64_t textOff = kHeaderEnd;
    const std::uint64_t roOff = align16(textOff + text.bytes.size());
    const std::uint64_t dataOff = align16(roOff + rodata.bytes.size());
    const std::uint64_t dynOff = dataOff + data.bytes.size();
    const std::uint64_t dynVaddr =
        static_cast<std::uint64_t>(data.memoryOffset) + data.bytes.size();
    constexpr std::uint64_t kDynSize = 16; // one Elf64_Dyn (DT_NULL)
    const std::uint64_t dataFileSize = data.bytes.size() + kDynSize;

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
    AppendU64Le(elf, entryPoint); // e_entry
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
    AppendPhdr(elf, kPtLoad, kPfR | kPfX, textOff, text.memoryOffset,
               text.bytes.size(), text.bytes.size());
    AppendPhdr(elf, kPtLoad, kPfR, roOff, rodata.memoryOffset,
               rodata.bytes.size(), rodata.bytes.size());
    AppendPhdr(elf, kPtLoad, kPfR | kPfW, dataOff, data.memoryOffset, dataFileSize,
               dataFileSize + bssSize);
    AppendPhdr(elf, kPtDynamic, kPfR | kPfW, dynOff, dynVaddr, kDynSize, kDynSize);

    // --- Payload ---
    while (elf.size() < textOff)
        elf.push_back(0);
    elf.insert(elf.end(), text.bytes.begin(), text.bytes.end());
    while (elf.size() < roOff)
        elf.push_back(0);
    elf.insert(elf.end(), rodata.bytes.begin(), rodata.bytes.end());
    while (elf.size() < dataOff)
        elf.push_back(0);
    elf.insert(elf.end(), data.bytes.begin(), data.bytes.end());
    for (std::uint64_t i = 0; i < kDynSize; ++i)
        elf.push_back(0); // DT_NULL tag + value

    return elf;
}

} // namespace Relinker
