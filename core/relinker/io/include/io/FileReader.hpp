#ifndef IO_FILEREADER_HPP
#define IO_FILEREADER_HPP

#include <vector>
#include <cstdint>
#include <string>

namespace Io {

class FileReader {
public:
    std::vector<std::uint8_t> Read(const std::string& path);
    std::vector<std::uint8_t> ReadIfExists(const std::string& path);
};

}

#endif
