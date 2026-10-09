cmake_minimum_required(VERSION 3.24)
project(AnySwitch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build tests" OFF)
option(ANYSWITCH_ENABLE_SPIRV_TOOLS "Enable SPIR-V validation and optimization" OFF)
option(ANYSWITCH_ENABLE_LLVM "Enable LLVM-based ARM64 translation" ON)
option(ANYSWITCH_USE_SYSTEM_REMILL "Use system Remill instead of building from source" OFF)
option(ANYSWITCH_BUILD_SHARED_LIBS "Build PRX libraries as shared objects" ON)
option(ANYSWITCH_BUILD_SHADER_RECOMPILER "Build the shader recompiler" OFF)


set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- LLVM (optional - required for Remill-based translation) ---
if(ANYSWITCH_ENABLE_LLVM)
    find_package(LLVM QUIET)
endif()
if(LLVM_FOUND)
    message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
    set(ANYSWITCH_HAS_LLVM ON)
else()
    message(STATUS "LLVM not found — ARM64→x86-64 codegen disabled")
    set(ANYSWITCH_HAS_LLVM OFF)
endif()

# --- Remill (optional - required for ARM64 lifting) ---
# Resolution order: system/install find_package first, then a locally built
# tree under 3rdparty (see 3rdparty/README.md). Never breaks the default
# configure; translation stays disabled if neither exists.
if(ANYSWITCH_HAS_LLVM)
    find_package(remill QUIET)
    if(remill_FOUND)
        message(STATUS "Using Remill: ${remill_DIR}")
        set(ANYSWITCH_HAS_REMILL ON)
    else()
        set(_remill_build "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/remill/build")
        set(_remill_deps "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/remill/dependencies/install")
        if(EXISTS "${_remill_build}/lib/BC/libremill_bc.a" AND EXISTS "${_remill_deps}/lib/libxed.a")
            set(ANYSWITCH_REMILL_BUILD_DIR "${_remill_build}" CACHE PATH "Local Remill build tree")
            set(ANYSWITCH_REMILL_DEPS_DIR "${_remill_deps}" CACHE PATH "Local Remill deps install")
            message(STATUS "Using locally built Remill: ${ANYSWITCH_REMILL_BUILD_DIR}")
            set(ANYSWITCH_HAS_REMILL ON)
        else()
            message(STATUS "Remill not found — ARM64→x86-64 codegen disabled")
            message(STATUS "  (build it per 3rdparty/README.md, then reconfigure)")
            set(ANYSWITCH_HAS_REMILL OFF)
            set(ANYSWITCH_HAS_LLVM OFF)
        endif()
    endif()
endif()
# --- glslang (for shader recompiler) - optional ---
if(ANYSWITCH_BUILD_SHADER_RECOMPILER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glslang/CMakeLists.txt")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
    set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPIRV ON CACHE BOOL "" FORCE)
    add_subdirectory(3rdparty/glslang 3rdparty_build/glslang EXCLUDE_FROM_ALL)
endif()

# --- Subdirectories ---
add_subdirectory(core/relinker)
add_subdirectory(core/libs)

# Convenience meta-target: builds all host system libraries.
add_custom_target(libs DEPENDS libc libkernel libkernel_prx libnx)

if(ANYSWITCH_BUILD_SHADER_RECOMPILER)
    add_subdirectory(core/shader/recompiler)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(Testing)
endif()

install(TARGETS relinker RUNTIME DESTINATION bin)
install(TARGETS libc libkernel libnx
    LIBRARY DESTINATION libs
    RUNTIME DESTINATION libs
)
