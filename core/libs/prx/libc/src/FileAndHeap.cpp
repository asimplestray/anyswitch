#include "prx/libc/include/General.hpp"
#include <cstdlib>

extern "C" {

int ANYSWITCH_HOST_ABI atoi_asw(const char* str) { return std::atoi(str); }
long ANYSWITCH_HOST_ABI atol_asw(const char* str) { return std::atol(str); }

}
