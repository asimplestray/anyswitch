# AnySwitch Shader Recompiler
# Translates Nintendo Switch GPU shaders (NVIDIA Maxwell/Pascal via NVN) to SPIR-V/Vulkan

add_library(anyswitch_shader_recompiler STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Recompiler.cpp
)

# Optional stages: each subdirectory hosts one pipeline stage.
# Sources are added automatically when contributors land them, so the
# target stays buildable while the recompiler is still a stub.
foreach(_stage MaxwellDecoder ControlFlow IntermediateRepresentation Translation Optimization SpirvBackend)
    file(GLOB _stage_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_stage}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${_stage}/src/*/*.cpp"
    )
    if(_stage_sources)
        target_sources(anyswitch_shader_recompiler PRIVATE ${_stage_sources})
    endif()
endforeach()

target_include_directories(anyswitch_shader_recompiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs
)

foreach(_stage MaxwellDecoder ControlFlow IntermediateRepresentation Translation Optimization SpirvBackend)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_stage}/include")
        target_include_directories(anyswitch_shader_recompiler PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/${_stage}/include"
        )
    endif()
endforeach()

# glslang is optional; link only when the target exists.
if(TARGET glslang)
    target_link_libraries(anyswitch_shader_recompiler PUBLIC glslang)
endif()
if(TARGET SPIRV)
    target_link_libraries(anyswitch_shader_recompiler PUBLIC SPIRV)
endif()

target_compile_features(anyswitch_shader_recompiler PUBLIC cxx_std_20)
