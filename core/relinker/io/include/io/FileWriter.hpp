#ifndef IO_FILEWRITER_HPP
#define IO_FILEWRITER_HPP

#include <vector>
#include <cstdint>
#include <string>

namespace Io {

class FileWriter {
public:
    void Write(const std::string& path, const std::vector<std::uint8_t>& data);
};

}

#endif
