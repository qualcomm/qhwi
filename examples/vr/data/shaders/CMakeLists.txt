file(GLOB_RECURSE SHADER_FILES *.hlsl *.glsl *.vert *.frag)

set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None") 
add_custom_target(qhwi_example_vr_shaders SOURCES ${SHADER_FILES} compileShaders.py)

cmake_path(SET VULKAN_SDK $<IF:$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>,$ENV{VULKAN_SDK},"">)

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../android/app/src/main/assets/shaders)
else()
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

macro(run_command)
    add_custom_command(
        TARGET qhwi_example_vr_shaders 
        POST_BUILD
        COMMAND ${ARGV}
        COMMENT "running command: ${ARGV}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
endmacro()

find_package (Python3 COMPONENTS Interpreter)

run_command(${Python3_EXECUTABLE} ./compileShaders.py
    --systemName ${CMAKE_SYSTEM_NAME}
    $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:--enableVulkanBackend>
    $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:--enableD3d12Backend>
    --dxcSpirvPath ${QHWI_DXC}
    --dxcDxilPath ${QHWI_DXC}
     --outputDir ${OUTPUT_DIR}
     --sourceDir ${CMAKE_CURRENT_SOURCE_DIR}
)
