#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <cstring>
#include <malloc.h>

extern "C" {

int ANYSWITCH_HOST_ABI memalign_asw(std::size_t alignment, std::size_t size) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) return -1;
    return 0;
}

std::size_t ANYSWITCH_HOST_ABI malloc_usable_size_asw(void* ptr) {
    return ptr ? malloc_usable_size(ptr) : 0;
}

}
