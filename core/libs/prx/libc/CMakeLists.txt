# Host libc compatibility layer

add_library(libc SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/General.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestHeap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestAllocations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileAndHeap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StandardStreams.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Formatting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JumpBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Mspace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CxxAbiSupport.cpp
)

target_include_directories(libc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(libc PROPERTIES
    PREFIX ""
    SUFFIX ".prx"
    OUTPUT_NAME "libc"
    CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
    add_executable(guest_standard_streams_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/GuestStandardStreams.cpp
    )
    target_include_directories(guest_standard_streams_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )
    target_link_libraries(guest_standard_streams_tests PRIVATE libc)
    add_test(NAME guest_standard_streams COMMAND guest_standard_streams_tests)
endif()
