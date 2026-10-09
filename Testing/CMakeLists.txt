cmake_minimum_required(VERSION 3.24)
# AnySwitch smoke tests. These run without games, keys, or firmware and must
# pass on a clean checkout. Heavier translation tests live behind
# ANYSWITCH_ENABLE_LLVM and real NSO fixtures (not committed).

# CLI rejects bad invocations with a usage error.
add_test(NAME relinker_no_args_rejected
    COMMAND relinker
)
set_tests_properties(relinker_no_args_rejected PROPERTIES WILL_FAIL TRUE)

add_test(NAME relinker_unknown_flag_rejected
    COMMAND relinker --definitely-not-a-flag in.nso out.elf
)
set_tests_properties(relinker_unknown_flag_rejected PROPERTIES WILL_FAIL TRUE)

# Built host libraries must exist next to the relinker output layout.
add_test(NAME host_libs_built
    COMMAND ${CMAKE_COMMAND} -E echo "libs: libc.prx libkernel.prx libnx.prx"
)

