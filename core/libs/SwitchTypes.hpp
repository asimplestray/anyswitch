#pragma once
// Common Switch/host shared types for AnySwitch system libraries.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "prx/libc/include/general/VabiMacros.hpp"

#if defined(__GNUC__) || defined(__clang__)
typedef float __m128 __attribute__((__vector_size__(16), __aligned__(16)));
#elif defined(_MSC_VER)
#include <xmmintrin.h>
#else
struct alignas(16) __m128 { float v[4]; };
#endif

using Bool = std::uint8_t;
using KernelModule = std::int32_t;
using KernelCpumask = std::uint64_t;
using KernelUseconds = unsigned int;
using KernelClockid = std::int32_t;

struct KernelTimespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};

struct KernelTimeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};

struct KernelEvent {
    std::uintptr_t ident = 0;
    std::int16_t filter = 0;
    std::uint16_t flags = 0;
    std::uint32_t fflags = 0;
    std::intptr_t data = 0;
    void* udata = nullptr;
};

struct KernelSchedParam {
    int sched_priority;
};

struct KernelAioResult {
    std::int64_t return_value;
    std::uint32_t state;
};
