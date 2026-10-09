#ifndef ELFPATCHER_WINDOWS_WINDOWS_PE_FORMAT_HPP
#define ELFPATCHER_WINDOWS_WINDOWS_PE_FORMAT_HPP

#include <cstdint>

namespace Elfpatcher::Windows {

constexpr std::uint16_t kPeImageFileExecutable = 0x0002;
constexpr std::uint16_t kPeImageFileLargeAddressAware = 0x0020;
constexpr std::uint16_t kPeImageFileDynamicBase = 0x0040;
constexpr std::uint16_t kPeImageFileNxCompat = 0x0200;
constexpr std::uint16_t kPeImageFileNoIsolation = 0x0400;

constexpr std::uint32_t kPeImageNtOptionalHdr64Magic = 0x020B;
constexpr std::uint32_t kPeImageSizeofOptionalHeader64 = 0xF0;
constexpr std::uint16_t kPeImageNumberOfRvaAndSizes = 16;

constexpr std::uint32_t kPeImageDirectoryEntryExport = 0;
constexpr std::uint32_t kPeImageDirectoryEntryImport = 1;
constexpr std::uint32_t kPeImageDirectoryEntryResource = 2;
constexpr std::uint32_t kPeImageDirectoryEntryException = 3;
constexpr std::uint32_t kPeImageDirectoryEntrySecurity = 4;
constexpr std::uint32_t kPeImageDirectoryEntryBasereloc = 5;
constexpr std::uint32_t kPeImageDirectoryEntryDebug = 6;
constexpr std::uint32_t kPeImageDirectoryEntryArchitecture = 7;
constexpr std::uint32_t kPeImageDirectoryEntryGlobal = 8;
constexpr std::uint32_t kPeImageDirectoryEntryTls = 9;
constexpr std::uint32_t kPeImageDirectoryEntryIat = 12;
constexpr std::uint32_t kPeImageDirectoryEntryIat = 12;

constexpr std::uint32_t kPeImageDllCharactericsNxCompat = 0x0200;
constexpr std::uint32_t kPeImageDllCharactersDynamicBase = 0x0040;

constexpr std::uint32_t kPeImageSectChrCode = 0x00000020;
constexpr std::uint32_t kPeImageSectChrInitializedData = 0x00000040;
constexpr std::uint32_t kPeImageSectChrUninitializedData = 0x00000080;
constexpr std::uint32_t kPeImageSectChrLib = 0x00002000;
constexpr std::uint32_t kPeImageSectChrRemove = 0x02000000;

constexpr std::uint64_t kPeImageBase = 0x180000000ULL;
constexpr std::uint16_t kPeImagePageSize = 0x1000;

} // namespace Elfpatcher::Windows

#endif
