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

# Execution harness: hand-built .nro -> translate -> link native runtime -> run.
# This is the test that proves translated code is semantically correct, not
# merely that bytes get emitted.
add_test(NAME relinker_executes_translated
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/core/relinker/tests/run_exec_tests.py
        --relinker $<TARGET_FILE:relinker>
        --workdir ${CMAKE_CURRENT_BINARY_DIR}/exec
)

# End-to-end: synthetic ARM64 ELF + NSO fixtures (plain, LZ4, match-vector,
# truncated) through the relinker. Needs Python 3 + the built binary.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_test(NAME relinker_end_to_end
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/core/relinker/tests/test_end_to_end.py
            --relinker $<TARGET_FILE:relinker>
    )
else()
    message(STATUS "Python3 not found — relinker_end_to_end test disabled")
endif()

