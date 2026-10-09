# Source files
set(RELINKER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cli/src/CliArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/src/ByteWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/src/FileWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/ElfImageBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/NsoReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/parsing/NroReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/pipeline/RelinkerPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/output/SysVDynamicSectionBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/src/output/CallRegistryWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/linux/LinuxElfPatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/EntryStubBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/ProgramHeaderLayoutBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SectionHeaderTableBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/src/general/SegmentFilter.cpp
)

add_executable(relinker ${RELINKER_SOURCES})

target_include_directories(relinker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cli/include
    ${CMAKE_CURRENT_SOURCE_DIR}/domain/include
    ${CMAKE_CURRENT_SOURCE_DIR}/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include/elfpatcher/general
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include/elfpatcher/linux
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include/elfpatcher/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/codegen/include
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include
    ${CMAKE_CURRENT_SOURCE_DIR}/relinker/include/relinker
    ${CMAKE_CURRENT_SOURCE_DIR}/elfpatcher/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs
)

# Remill integration (translation enabled only when LLVM + Remill resolve).
if(ANYSWITCH_HAS_REMILL)
    find_package(Threads REQUIRED)
    target_sources(relinker PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/RemillArm64Translator.cpp
    )
    target_compile_definitions(relinker PRIVATE
        ANYSWITCH_HAS_REMILL
        ANYSWITCH_HAS_LLVM
        GLOG_USE_GLOG_EXPORT
    )

    # Arch/LLVM dylib builds ship a single libLLVM.so instead of per-component
    # archives; link it directly.
    set(_anyswitch_llvm_libs LLVM)

    if(remill_FOUND)
        target_link_libraries(relinker PRIVATE remill ${_anyswitch_llvm_libs})
    elseif(DEFINED ANYSWITCH_REMILL_BUILD_DIR)
        # Locally built Remill tree (see 3rdparty/README.md): link the
        # static archives directly, grouped to resolve cycles.
        target_include_directories(relinker PRIVATE
            ${CMAKE_SOURCE_DIR}/3rdparty/remill/include
            ${ANYSWITCH_REMILL_DEPS_DIR}/include
            ${ANYSWITCH_REMILL_BUILD_DIR}/_deps/sleigh-src/support/include
            ${ANYSWITCH_REMILL_BUILD_DIR}/_deps/sleigh-build/include
            ${LLVM_INCLUDE_DIRS}
        )
        target_compile_definitions(relinker PRIVATE
            "ANYSWITCH_REMILL_AARCH64_BC=\"${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/AArch64/Runtime/aarch64.bc\""
        )
        target_link_libraries(relinker PRIVATE
            -Wl,--start-group
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Version/libremill_version.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/OS/libremill_os.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/AArch64/libremill_arch_aarch64.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/X86/libremill_arch_x86.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/Sleigh/libremill_arch_sleigh.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/SPARC32/libremill_arch_sparc32.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/SPARC64/libremill_arch_sparc64.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/Arch/libremill_arch.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/lib/BC/libremill_bc.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/_deps/sleigh-build/libsla.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/_deps/sleigh-build/libdecomp.a
            ${ANYSWITCH_REMILL_BUILD_DIR}/_deps/sleigh-build/support/libslaSupport.a
            ${ANYSWITCH_REMILL_DEPS_DIR}/lib/libxed.a
            ${ANYSWITCH_REMILL_DEPS_DIR}/lib/libxed-ild.a
            ${ANYSWITCH_REMILL_DEPS_DIR}/lib/libglog.a
            ${ANYSWITCH_REMILL_DEPS_DIR}/lib/libgflags.a
            -Wl,--end-group
            ${_anyswitch_llvm_libs}
            ${CMAKE_DL_LIBS}
            unwind
            Threads::Threads
        )
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME elf_magic COMMAND ${CMAKE_COMMAND} -E echo "TODO: elf_magic_test")
endif()
