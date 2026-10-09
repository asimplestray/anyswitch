# Host implementation of the Switch kernel's syscall surface.
#
# Built twice from the same sources:
#   libkernel.prx  - what a ported game links against (dynamic linking)
#   libkernel      - a static archive, so the runtime and the unit tests can
#                    link the same objects
#
# Adding a syscall: implement it in one of the src/*.cpp files, then add one
# line to the table in src/Dispatch.cpp.

add_library(libkernel STATIC
    src/Dispatch.cpp
    src/Memory.cpp
    src/Handle.cpp
    src/Info.cpp
    src/Sessions.cpp
    src/SystemRegisters.cpp
)

target_include_directories(libkernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# A plain host library: no Remill, no LLVM, which is what keeps these
# functions unit-testable without a translated binary.
target_compile_features(libkernel PUBLIC cxx_std_20)

add_library(libkernel_prx SHARED
    src/Dispatch.cpp
    src/Memory.cpp
    src/Handle.cpp
    src/Info.cpp
    src/Sessions.cpp
    src/SystemRegisters.cpp
)

target_include_directories(libkernel_prx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(libkernel_prx PROPERTIES
    PREFIX ""
    SUFFIX ".prx"
    OUTPUT_NAME "libkernel"
    CXX_EXTENSIONS OFF
)
