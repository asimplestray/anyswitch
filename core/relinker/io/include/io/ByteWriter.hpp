#ifndef IO_BYTEWRITER_HPP
#define IO_BYTEWRITER_HPP

#include <vector>
#include <cstdint>

namespace Io {

class IByteWriter {
public:
    virtual ~IByteWriter() = default;
    virtual void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t val) = 0;
    virtual void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t val) = 0;
    virtual void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t val) = 0;
    virtual void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t val) = 0;
};

class ByteWriter : public IByteWriter {
public:
    void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t val) override;
    void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t val) override;
    void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t val) override;
    void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t val) override;
};

} // namespace Io

#endif
