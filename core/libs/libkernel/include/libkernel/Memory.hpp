#pragma once

#include <cstddef>
#include <cstdint>

// The only definition of the guest memory handle.
//
// Remill's runtime forward-declares `struct Memory` and never dereferences it;
// the read/write intrinsics that do are ours. So this definition satisfies both
// the translated code and the host libraries, while keeping the host libraries
// free of any Remill dependency - which is what makes them unit-testable.
struct Memory {
    std::uint8_t* data;
    std::size_t size;
};
