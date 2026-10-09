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

# Remill integration (conditional on LLVM + Remill availability).
# Root CMake sets ANYSWITCH_HAS_LLVM only when both are present.
if(ANYSWITCH_HAS_LLVM)
    target_sources(relinker PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/codegen/src/RemillArm64Translator.cpp
    )
    target_compile_definitions(relinker PRIVATE
        ANYSWITCH_HAS_LLVM
        ANYSWITCH_HAS_REMILL
    )
    if(DEFINED LLVM_LIBRARY_DIR)
        target_link_directories(relinker PRIVATE ${LLVM_LIBRARY_DIR})
    endif()
    if(TARGET remill)
        target_link_libraries(relinker PRIVATE remill)
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME elf_magic COMMAND ${CMAKE_COMMAND} -E echo "TODO: elf_magic_test")
endif()
