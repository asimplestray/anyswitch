#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTHEAP_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTHEAP_HPP

#include <cstddef>
#include <cstdint>

namespace Domain {

struct GuestHeap {
    static void* GuestHeapAllocate(std::size_t size);
    static void GuestHeapFree(void* ptr);
};

}

#endif
