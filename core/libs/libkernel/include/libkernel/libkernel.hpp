#pragma once

// Host implementation of the Switch kernel's syscall surface.
//
// This is the single source of truth for syscall behaviour: the guest runtime
// dispatches through here, the `.prx` links against the same objects, and the
// unit tests exercise these functions directly. Implementing a syscall in more
// than one place is the bug this layout exists to prevent.

#include "libkernel/Dispatch.hpp"
