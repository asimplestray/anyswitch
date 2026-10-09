#include <cstring>
#include <stdexcept>

namespace Io {

class BufferUtils {
public:
    static void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t val) {
        if (offset + 8 > buf.size()) throw std::out_of_range("WriteU64 out of bounds");
        std::memcpy(buf.data() + offset, &val, 8);
    }

    static void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t val) {
        if (offset + 4 > buf.size()) throw std::out_of_range("WriteU32 out of bounds");
        std::memcpy(buf.data() + offset, &val, 4);
    }

    static void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t val) {
        if (offset + 2 > buf.size()) throw std::out_of_range("WriteU16 out of bounds");
        std::memcpy(buf.data() + offset, &val, 2);
    }

    static std::uint64_t ReadU64(const std::vector<std::uint8_t>& buf, std::size_t offset) {
        if (offset + 8 > buf.size()) throw std::out_of_range("ReadU64 out of bounds");
        std::uint64_t val;
        std::memcpy(&val, buf.data() + offset, 8);
        return val;
    }

    static std::uint32_t ReadU32(const std::vector<std::uint8_t>& buf, std::size_t offset) {
        if (offset + 4 > buf.size()) throw std::out_of_range("ReadU32 out of bounds");
        std::uint32_t val;
        std::memcpy(&val, buf.data() + offset, 4);
        return val;
    }

    static std::uint16_t ReadU16(const std::vector<std::uint8_t>& buf, std::size_t offset) {
        if (offset + 2 > buf.size()) throw std::out_of_range("ReadU16 out of bounds");
        std::uint16_t val;
        std::memcpy(&val, buf.data() + offset, 2);
        return val;
    }

    static void AppendU8(std::vector<std::uint8_t>& buf, std::uint8_t val) {
        buf.push_back(val);
    }

    static void AppendU32(std::vector<std::uint8_t>& buf, std::uint32_t val) {
        std::size_t pos = buf.size();
        buf.resize(pos + 4);
        std::memcpy(buf.data() + pos, &val, 4);
    }

    static void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t val) {
        std::size_t pos = buf.size();
        buf.resize(pos + 8);
        std::memcpy(buf.data() + pos, &val, 8);
    }

    static void AppendString(std::vector<std::uint8_t>& buf, const std::string& s) {
        buf.insert(buf.end(), s.begin(), s.end());
        buf.push_back(0);
    }
};

}
