#pragma once
#include "prx/libc/include/General.hpp"
#include <cstdlib>

// JumpBuf type - matches Newlib _JBLEN used by Switch homebrew/libnx.
using JumpBuf = std::array<std::uint64_t, 32>;
