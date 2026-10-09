add_library(libkernel SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Semaphore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Mutex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/File.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libkernel.cpp
)

target_include_directories(libkernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

set_target_properties(libkernel PROPERTIES
    PREFIX ""
    SUFFIX ".prx"
    OUTPUT_NAME "libkernel"
)

target_link_libraries(libkernel PRIVATE pthread dl)
