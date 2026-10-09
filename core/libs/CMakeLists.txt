# AnySwitch core libraries

# libc - Host libc compatibility
add_subdirectory(prx/libc)

# libkernel - Kernel services (svc calls → host kernel)
add_subdirectory(prx/libkernel)

# libnx - Nintendo libnx runtime reimplementation
add_subdirectory(prx/libnx)
