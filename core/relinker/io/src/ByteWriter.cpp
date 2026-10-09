#include <io/ByteWriter.hpp>
#include <cstring>

namespace Io {

void ByteWriter::AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t val) {
    std::size_t pos = buf.size();
    buf.resize(pos + 8);
    std::memcpy(buf.data() + pos, &val, 8);
}

void ByteWriter::WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t val) {
    std::memcpy(buf.data() + offset, &val, 8);
}

void ByteWriter::WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t val) {
    std::memcpy(buf.data() + offset, &val, 4);
}

void ByteWriter::WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t val) {
    std::memcpy(buf.data() + offset, &val, 2);
}

} // namespace Io
