# libnx - Host reimplementation of Nintendo Switch libnx runtime

set(LIBNX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libnx.cpp
)

add_library(libnx SHARED ${LIBNX_SOURCES})
target_include_directories(libnx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
set_target_properties(libnx PROPERTIES
    PREFIX ""
    SUFFIX ".prx"
    OUTPUT_NAME "libnx"
)
target_link_libraries(libnx PRIVATE pthread dl)
