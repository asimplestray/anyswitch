# Unit tests for the host system libraries.
#
# These run without a translated binary, a guest image, or a game. That is the
# point: a contributor can implement a syscall here, run it, and know it is
# right before anyone tries to boot a real program with it.
#
# The pattern for a test:
#   * fill a SyscallArgs the way the guest's registers would look
#   * back it with a Memory you can inspect
#   * call the handler
#   * assert on the args it wrote back, and on the memory it touched
#
# See test_libkernel.cpp for worked examples. Adding a test means adding its
# name to the RUN list in that file's main().

add_executable(libkernel_tests
    test_libkernel.cpp
)

target_include_directories(libkernel_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/include
)

target_link_libraries(libkernel_tests PRIVATE libkernel)

set_target_properties(libkernel_tests PROPERTIES
    CXX_EXTENSIONS OFF
)

# Tests drive themselves through a plain main(); no framework needed yet.
add_test(NAME libkernel_units COMMAND libkernel_tests)
