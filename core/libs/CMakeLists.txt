# AnySwitch core libraries
#
# These are the host implementations of the Switch's system interface. They are
# deliberately free of Remill and LLVM so they can be built as .prx libraries
# for games *and* unit-tested without a translated binary.

# libkernel - kernel syscalls (the single source of truth for syscall
# behaviour; the guest runtime dispatches through this)
add_subdirectory(libkernel)

# libc - Host libc compatibility
add_subdirectory(prx/libc)

# Legacy libkernel entry point removed: the implementation in libkernel/ is
# now the only one, built both as a static archive and as libkernel.prx.

# libnx - Nintendo Switch runtime reimplementation
add_subdirectory(prx/libnx)

# Unit tests for the host libraries. These run with no translated binary.
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
