include(../../../data/shaders/shaders.cmake)

file(GLOB_RECURSE SHADER_FILES *.hlsl *.glsl *.vert *.frag)

set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None") 
add_custom_target(qhwi_test_shaders SOURCES ${SHADER_FILES} compileShaders.py)

find_package (Python3 COMPONENTS Interpreter)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../precompiled)

run_command(qhwi_test_shaders
    COMMAND ${Python3_EXECUTABLE} ./compileShaders.py
    --systemName ${CMAKE_SYSTEM_NAME}
    $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:--enableVulkanBackend>
    $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:--enableD3d12Backend>
    $<$<BOOL:${QHWI_ENABLE_SER}>:--enableSER>
    --dxcSpirvPath ${QHWI_DXC}
    --dxcDxilPath ${QHWI_DXC}
     --outputDir ${OUTPUT_DIR}
     --sourceDir ${CMAKE_CURRENT_SOURCE_DIR}
)
