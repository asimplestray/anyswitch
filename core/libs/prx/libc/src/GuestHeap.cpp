#include "prx/libc/include/General.hpp"
#include <cstdlib>

extern "C" {

void* ANYSWITCH_HOST_ABI malloc_asw(std::size_t size) { return std::malloc(size); }
void ANYSWITCH_HOST_ABI free_asw(void* ptr) { std::free(ptr); }
void* ANYSWITCH_HOST_ABI realloc_asw(void* ptr, std::size_t size) { return std::realloc(ptr, size); }
void* ANYSWITCH_HOST_ABI calloc_asw(std::size_t num, std::size_t size) { return std::calloc(num, size); }

void* GuestHeapAllocate_asw(std::size_t size);
void GuestHeapFree_asw(void* ptr);

}
