file(GLOB_RECURSE SHADER_FILES *.glsl)
set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")
add_custom_target(qhwi_example_qcom_tile_shading_shaders SOURCES ${SHADER_FILES})

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../android/app/src/main/assets/shaders)
else()
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(CMAKE_HOST_WIN32)
    if(DEFINED ENV{VULKAN_SDK})
        set(GLSLC_DEFAULT "$ENV{VULKAN_SDK}/Bin/glslc.exe")
    else()
        set(GLSLC_DEFAULT "glslc.exe")
    endif()
else()
    set(GLSLC_DEFAULT "glslc")
endif()
set(QHWI_GLSLC ${GLSLC_DEFAULT} CACHE STRING "glslc executable")

file(MAKE_DIRECTORY ${OUTPUT_DIR})

macro(compile_glsl_shader SHADER_FILE STAGE)
    add_custom_command(
        TARGET qhwi_example_qcom_tile_shading_shaders
        COMMAND ${QHWI_GLSLC} -fshader-stage=${STAGE} -o ${OUTPUT_DIR}/${SHADER_FILE}.spv ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
        COMMENT "Compiling GLSL shader: ${SHADER_FILE}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
endmacro()

compile_glsl_shader(vert1.glsl vert)
compile_glsl_shader(vert2.glsl vert)
compile_glsl_shader(frag1.glsl frag)
compile_glsl_shader(frag2.glsl frag)
compile_glsl_shader(compute1.glsl comp)
compile_glsl_shader(compute2.glsl comp)
