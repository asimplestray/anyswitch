#include <io/FileWriter.hpp>
#include <fstream>

namespace Io {

void FileWriter::Write(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write file: " + path);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

}
