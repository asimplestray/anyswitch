#ifndef DOMAIN_TYPES_HPP
#define DOMAIN_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <map>
#include <domain/GuestRuntime.hpp>

namespace Domain {

using FileByteOffset = std::uint64_t;
using VirtualAddress = std::uint64_t;
using ByteCount = std::uint64_t;

struct ElfHeader {
    std::uint16_t Machine;
    std::uint16_t Type;
    std::uint8_t OsAbi;
    std::uint8_t AbiVersion;
    std::uint64_t EntryPoint;
    std::uint64_t ProgramHeaderOffset;
    std::uint64_t SectionHeaderOffset;
    std::uint32_t ProgramHeaderEntrySize;
    std::uint16_t ProgramHeaderCount;
    std::uint32_t SectionHeaderEntrySize;
    std::uint16_t SectionHeaderCount;
    std::uint16_t SectionHeaderStringIndex;
};

struct ProgramHeader {
    std::uint32_t Type;
    std::uint32_t Flags;
    FileByteOffset Offset;
    VirtualAddress MappedAddress;
    VirtualAddress PhysicalAddress;
    ByteCount FileSize;
    ByteCount MemorySize;
    std::uint64_t Alignment;
};

struct SectionHeader {
    std::string Name;
    std::uint32_t Type;
    std::uint64_t Flags;
    VirtualAddress MappedAddress;
    FileByteOffset Offset;
    ByteCount SectionSize;
    std::uint32_t Link;
    std::uint32_t Info;
    std::uint64_t EntrySize;
};

struct Relocation {
    FileByteOffset Offset;
    std::uint32_t Type;
    std::int32_t SymbolIndex;
    std::int64_t Addend;
    std::string ImportName;
    std::string Library;
};

struct DynamicTag {
    std::int64_t Tag;
    std::uint64_t Value;
};

struct SysVDynamicSection {
    std::vector<std::uint8_t> DynamicSegmentData;
    std::vector<std::uint8_t> DynSymData;
    std::vector<std::uint8_t> DynStrData;
    std::vector<std::uint8_t> RelaData;
    std::vector<std::uint8_t> RelaPltData;
    std::vector<GuestRuntime> GuestModules;
    std::vector<std::string> NeededLibraries;
};

struct RelinkerException : std::runtime_error {
    FileByteOffset FailureOffset;
    explicit RelinkerException(const std::string& message, const FileByteOffset failureOffset = 0)
        : std::runtime_error(message), FailureOffset(failureOffset) {}
};

} // namespace Domain

#endif
