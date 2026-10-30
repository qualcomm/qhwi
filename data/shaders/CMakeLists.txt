include(shaders.cmake)

file(GLOB_RECURSE SHADER_FILES *.hlsl *.glsl *.vert *.frag ${CMAKE_CURRENT_SOURCE_DIR}/*.hlsli)
file(GLOB_RECURSE SHADER_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/*.py)

set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")
add_custom_target(qhwi_shaders SOURCES ${SHADER_FILES} ${SHADER_SCRIPTS} shaders.cmake)

find_package (Python3 COMPONENTS Interpreter)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/qhwi_precompiled)

run_command(qhwi_shaders
    COMMAND ${Python3_EXECUTABLE} ../scripts/compileShaders.py
    --systemName ${CMAKE_SYSTEM_NAME}
    $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:--enableVulkanBackend>
    $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:--enableD3d12Backend>
    --dxcSpirvPath ${QHWI_DXC}
    --dxcDxilPath ${QHWI_DXC}
     --outputDir ${CMAKE_BINARY_DIR}/qhwi_precompiled
     --sourceDir ${CMAKE_CURRENT_SOURCE_DIR}
)

run_command(qhwi_shaders
    COMMAND
    ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/qhwi_shaders.h.py
     --outputDir ${CMAKE_BINARY_DIR}/qhwi_precompiled
     --sourceDir ${CMAKE_BINARY_DIR}/qhwi_precompiled
)