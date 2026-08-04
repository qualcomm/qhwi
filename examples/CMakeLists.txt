DownloadFile(cxxopts https://raw.githubusercontent.com/jarro2783/cxxopts/v3.2.0/include/cxxopts.hpp)
set(CXXOPTS_INCLUDE_DIRS ${cxxopts_SOURCE_DIR})

if (ANDROID)
    DownloadExtractFile(vk_vl https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/vulkan-sdk-1.3.268.0/android-binaries-vulkan-sdk-1.3.268.0-android.tar.gz)
endif()

if(ANDROID)
    add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    target_include_directories(native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue)
endif()

function(fetch_OpenXR)
# openxr_loader - From github.com/KhronosGroup
set(BUILD_TESTS OFF CACHE INTERNAL "Build tests")
set(BUILD_API_LAYERS ON CACHE INTERNAL "Use OpenXR layers")
FetchContent_Declare(
    OpenXR
    URL_HASH SHA256=54a85b7bcf56fae880e4fed76407ff1215cb2c59c351edd75353233c261101da
    URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.34.zip
    SOURCE_DIR openxr
)
cmake_policy(PUSH)
cmake_policy(SET CMP0148 OLD)
FetchContent_MakeAvailable(OpenXR)
cmake_policy(POP)
endfunction()

function(add_qhwi_example name)
    # TODO: remove dependency on test_util
    file(GLOB_RECURSE QHWI_EXAMPLE_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/util/*)
    file(GLOB_RECURSE QHWI_ANDROID_UTIL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../util/qhwi_android_util.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../util/qhwi_android_util.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../util/qhwi_metrics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../util/qhwi_performance.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../util/qhwi_performance.h)
    file(GLOB_RECURSE QHWI_EXAMPLE_SHADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/shaders/*)
    set_source_files_properties(${QHWI_EXAMPLE_SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")
    if(ANDROID)
        set(VK_VALIDATION_LAYER_PREBUILT_LIBRARY_DIR ${vk_vl_SOURCE_DIR})
        add_library(${name} SHARED ${QHWI_EXAMPLE_SOURCE_FILES} ${QHWI_EXAMPLE_SHADER_FILES} ${QHWI_ANDROID_UTIL_SOURCE_FILES})
        target_include_directories(${name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../../util
            $<$<BOOL:${QHWI_ENABLE_PIL}>:${PIL_INCLUDE_DIR}>)
        target_link_libraries(${name} PRIVATE
                android native_app_glue log "-u ANativeActivity_onCreate")
        set(ANDROID_ABI arm64-v8a)
        file(COPY ${VK_VALIDATION_LAYER_PREBUILT_LIBRARY_DIR}/${ANDROID_ABI}/libVkLayer_khronos_validation.so
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/jniLibs/${ANDROID_ABI})
        if (QHWI_ENABLE_PIL)
            if(EXISTS "${PIL_BINARY_DIR}/libQProfilerInterface.so")
                file(COPY "${PIL_BINARY_DIR}/libQProfilerInterface.so"
                    DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/jniLibs/${ANDROID_ABI}")
            else()
                message(WARNING "PIL library not found at ${PIL_BINARY_DIR}/libQProfilerInterface.so - skipping copy")
            endif()
        endif()
    else()
        add_executable(${name} ${QHWI_EXAMPLE_SOURCE_FILES} ${QHWI_EXAMPLE_SHADER_FILES})
    endif()
    source_group(Shaders FILES ${QHWI_EXAMPLE_SHADER_FILES})
    # TODO: remove dependency on test_util
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../test/util)

    target_link_libraries(${name} PRIVATE qhwi_examples_common)
    if (QHWI_ENABLE_D3D12_BACKEND)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_BIN_DIR}/D3D12Core.dll" "$<TARGET_FILE_DIR:${name}>/D3D12/D3D12Core.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_BIN_DIR}/d3d12SDKLayers.dll" "$<TARGET_FILE_DIR:${name}>/D3D12/d3d12SDKLayers.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_PIX_BIN_DIR}/WinPixEventRuntime.dll" "$<TARGET_FILE_DIR:${name}>/WinPixEventRuntime.dll")
    endif()
    if (QHWI_ENABLE_D3D12_BACKEND AND QHWI_ENABLE_COMPRESSED_STORAGE)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_DSTORAGE_BIN_DIR}/dstorage.dll" "$<TARGET_FILE_DIR:${name}>/dstorage.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QHWI_D3D12_DSTORAGE_BIN_DIR}/dstoragecore.dll" "$<TARGET_FILE_DIR:${name}>/dstoragecore.dll")
    endif()
    qre_set_target_recommended_warnings(${name})
endfunction()

function(add_qhwi_vr_example name)
    fetch_OpenXR()

    add_qhwi_example(${name})
    add_subdirectory(data/shaders)

    target_compile_definitions(${name} PRIVATE
        QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
    )
    target_include_directories(${name} PRIVATE
        "${OPENXR_BUILD_DIR}/include"
    )
    if(ANDROID)
        target_link_directories(${name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/jniLibs/${CMAKE_ANDROID_ARCH_ABI})
    endif()
    target_link_libraries(${name} PRIVATE openxr_loader)
    add_dependencies(${name} ${name}_shaders)
endfunction()

add_subdirectory(common)

function(add_example_dir name)
    if (QHWI_EXAMPLES_FILTER STREQUAL "" OR QHWI_EXAMPLES_FILTER MATCHES ".*${name}.*")
        message(STATUS "add example: ${name}")
        add_subdirectory(${name})
    endif()
endfunction()

add_example_dir(qcom_image_proc)
add_example_dir(qcom_multiview)
if(QHWI_ENABLE_VULKAN_BACKEND)
    add_example_dir(vk_custom_resolve)
endif()
add_example_dir(vr)
if(ANDROID)
    add_example_dir(qcom_tile_shading)
    add_example_dir(qcom_tile_memory_heap)
endif()
add_example_dir(gpl)
