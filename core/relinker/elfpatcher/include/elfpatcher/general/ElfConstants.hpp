#ifndef ELFPATCHER_GENERAL_ELFCONSTANTS_HPP
#define ELFPATCHER_GENERAL_ELFCONSTANTS_HPP

#include <cstddef>
#include <cstdint>

namespace Elfpatcher {

inline constexpr std::size_t kEhdrOsAbiOffset = 7;
inline constexpr std::size_t kEhdrAbiVersionOffset = 8;
inline constexpr std::size_t kEhdrTypeOffset = 0x10;
inline constexpr std::size_t kEhdrEntryOffset = 24;
inline constexpr std::size_t kEhdrPhOffOffset = 32;
inline constexpr std::size_t kEhdrShOffOffset = 40;
inline constexpr std::size_t kEhdrPhEntSizeOffset = 54;
inline constexpr std::size_t kEhdrPhNumOffset = 56;
inline constexpr std::size_t kEhdrShEntSizeOffset = 58;
inline constexpr std::size_t kEhdrShNumOffset = 60;
inline constexpr std::size_t kEhdrShStrNdxOffset = 62;

inline constexpr std::size_t kDynEntrySize = 16;
inline constexpr std::uint64_t kSymEntrySize = 24;
inline constexpr std::uint64_t kRelaEntrySize = 24;

inline constexpr std::size_t kDynStrAlignment = 24;
inline constexpr std::size_t kDynSymAlignment = 24;
inline constexpr std::size_t kRelaAlignment = 24;
inline constexpr std::size_t kRelaPltAlignment = 8;

inline constexpr std::uint64_t kDefaultLoadAlignment = 0x1000;

inline constexpr std::uint32_t PT_LOAD = 1;
inline constexpr std::uint32_t PT_DYNAMIC = 2;
inline constexpr std::uint32_t PT_PHDR = 6;
inline constexpr std::uint32_t PT_INTERP = 3;
inline constexpr std::uint32_t PF_X = 0x1;

inline constexpr std::int64_t DT_NEEDED = 1;
inline constexpr std::int64_t DT_STRSZ = 10;
inline constexpr std::int64_t DT_RELA = 7;
inline constexpr std::int64_t DT_RELASZ = 8;
inline constexpr std::int64_t DT_RELAENT = 9;
inline constexpr std::int64_t DT_STRTAB = 5;
inline constexpr std::int64_t DT_SYMTAB = 6;
inline constexpr std::int64_t DT_SYMENT = 11;
inline constexpr std::int64_t DT_JMPREL = 23;
inline constexpr std::int64_t DT_PLTRELSZ = 2;
inline constexpr std::int64_t DT_PLTREL = 20;
inline constexpr std::uint64_t DF_BIND_NOW = 0x8;

}

#endif
