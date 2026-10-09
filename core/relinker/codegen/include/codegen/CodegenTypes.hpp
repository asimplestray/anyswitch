#ifndef CODEGEN_CODEGENTYPES_HPP
#define CODEGEN_CODEGENTYPES_HPP

#include <cstdint>
#include <vector>
#include <string>

namespace Codegen {

struct InstructionMatch {
    std::uint64_t Offset;
    std::size_t Length;
};

struct TranslationReport {
    std::string InstructionName;
    std::uint64_t Offset;
    std::size_t OriginalLength;
    std::size_t ReplacementLength;
};

}

#endif
