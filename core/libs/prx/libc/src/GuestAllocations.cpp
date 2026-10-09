#include "prx/libc/include/General.hpp"
#include <cstdlib>

extern "C" {

void* ANYSWITCH_HOST_ABI GuestHeapAllocate_asw(std::size_t size);
void ANYSWITCH_HOST_ABI GuestHeapFree_asw(void* ptr);

}
