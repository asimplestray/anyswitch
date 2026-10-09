#include <io/FileReader.hpp>
#include <fstream>
#include <filesystem>

namespace Io {

std::vector<std::uint8_t> FileReader::Read(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open file: " + path);
    auto size = file.tellg();
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!file) throw std::runtime_error("Failed to read file: " + path);
    return buffer;
}

std::vector<std::uint8_t> FileReader::ReadIfExists(const std::string& path) {
    if (!std::filesystem::exists(path)) return {};
    return Read(path);
}

}
