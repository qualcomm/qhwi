#
#  qhwi_lib.cmake
#  qhwi
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

include(FetchContent)
include(GNUInstallDirs)

configure_file(src/qhwi_config.h.in include/qhwi/qhwi_config.h)

if (QHWI_ENABLE_D3D12_BACKEND)
    if(QHWI_USE_AGILITY_SDK_717_PREVIEW)
        DownloadExtractFile(d3d12 https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.717.1-preview)
    else()
        DownloadExtractFile(d3d12 https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.618.4)
    endif()

    # Even if we do not have the PIX capture backend enabled we still need to link with WinPixEventRuntime
    # See: https://devblogs.microsoft.com/pix/winpixeventruntime/
    DownloadExtractFile(pix https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.231030001)

    set(QHWI_D3D12_INCLUDE_DIRS "${d3d12_SOURCE_DIR}/build/native/include" "${d3d12_SOURCE_DIR}/build/native/include/d3dx12" "${pix_SOURCE_DIR}/Include/WinPixEventRuntime" CACHE INTERNAL "")

    set(QHWI_DXC_LIBRARIES_DIR "${DXC_LIBRARIES_DIR}" CACHE INTERNAL "")
    set(QHWI_DXC_BINARIES_DIR "${DXC_BINARIES_DIR}" CACHE INTERNAL "")

    if(QHWI_ENABLE_COMPRESSED_STORAGE)
        # Download direct storage API package
        DownloadExtractFile(dstorage https://www.nuget.org/api/v2/package/Microsoft.Direct3D.DirectStorage/1.3.0)

        list(APPEND QHWI_D3D12_INCLUDE_DIRS "${dstorage_SOURCE_DIR}/native/include")
        set(QHWI_D3D12_INCLUDE_DIRS "${QHWI_D3D12_INCLUDE_DIRS}" CACHE INTERNAL "" FORCE)
    endif()

    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(QHWI_D3D12_BIN_DIR "${d3d12_SOURCE_DIR}/build/native/bin/x64" CACHE INTERNAL "")
        set(QHWI_D3D12_PIX_BIN_DIR "${pix_SOURCE_DIR}/bin/x64" CACHE INTERNAL "")
        if(QHWI_ENABLE_COMPRESSED_STORAGE)
            set(QHWI_D3D12_DSTORAGE_LIB_DIR "${dstorage_SOURCE_DIR}/native/lib/x64" CACHE INTERNAL "")
            set(QHWI_D3D12_DSTORAGE_BIN_DIR "${dstorage_SOURCE_DIR}/native/bin/x64" CACHE INTERNAL "")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(QHWI_D3D12_BIN_DIR "${d3d12_SOURCE_DIR}/build/native/bin/arm64" CACHE INTERNAL "")
        set(QHWI_D3D12_PIX_BIN_DIR "${pix_SOURCE_DIR}/bin/ARM64" CACHE INTERNAL "")
        if(QHWI_ENABLE_COMPRESSED_STORAGE)
            set(QHWI_D3D12_DSTORAGE_LIB_DIR "${dstorage_SOURCE_DIR}/native/lib/ARM64" CACHE INTERNAL "")
            set(QHWI_D3D12_DSTORAGE_BIN_DIR "${dstorage_SOURCE_DIR}/native/bin/ARM64" CACHE INTERNAL "")
        endif()
    else()
        message(ERROR "Unrecognized platform: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

if (QHWI_BUILD_UNIT_TESTS OR QHWI_BUILD_EXAMPLES)
    if(WIN32 OR CMAKE_SYSTEM_NAME MATCHES "Linux" OR APPLE)
        Message(STATUS "Downloading GLFW")
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        7482de6 # release 3.3.8
        )
        FetchContent_MakeAvailable(glfw)
    endif()
endif()

string(TOUPPER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_UPPERCASE)
if(DEFINED QHWI_LINUX_WSI)
    string(TOUPPER ${QHWI_LINUX_WSI} QHWI_LINUX_WSI_UPPERCASE)
    add_compile_definitions("QHWI_WSI_${QHWI_LINUX_WSI_UPPERCASE}")
    message(STATUS "QHWI_WSI_${QHWI_LINUX_WSI_UPPERCASE} is defined")
endif()


file(GLOB_RECURSE QHWI_SOURCE_FILES
    src/qhwi.cpp
    src/qhwi_util.cpp
    src/qhwi_ops.cpp
    include/qhwi/qhwi.h
    include/qhwi/qhwi_util.h
    include/qhwi/qhwi_enums.h
    include/qhwi/qhwi_log_util.h
    include/qhwi/qhwi_ops.h
    data/precompiled/qhwi_shaders.h
)
file(GLOB_RECURSE QHWI_D3D12_SOURCE_FILES
    src/qhwi_d3d12.cpp
    src/qhwi_d3d12_enums.cpp
    src/qhwi_d3d12_enums_internal.h
    src/qhwi_enums_from_d3d12.cpp
    src/qhwi_d3d_render_pass_encoder.cpp
    include/qhwi/qhwi_d3d12.h
    include/qhwi/qhwi_d3d12_enums.h
    include/qhwi/qhwi_d3d_render_pass_encoder.h
    util/qhwi_d3d_internal_extensions.h
    util/qhwi_d3d_internal_extensions.cpp
)
file(GLOB_RECURSE QHWI_VULKAN_SOURCE_FILES
    src/qhwi_vk.cpp
    src/qhwi_vk_enums.cpp
    src/qhwi_vk_render_pass_encoder.cpp
    src/qhwi_vk_enums_internal.h
    src/qhwi_enums_from_vk.cpp
    include/qhwi/qhwi_vk.h
    include/qhwi/qhwi_vk_config.h
    include/qhwi/qhwi_vk_enums.h
    include/qhwi/qhwi_vk_render_pass_encoder.h
)
file(GLOB_RECURSE QHWI_GLFW_SOURCE_FILES
    src/qhwi_glfw.cpp
    include/qhwi/qhwi_glfw.h
)
file(GLOB_RECURSE QHWI_PIX_SOURCE_FILES
    src/qhwi_pix.cpp
    include/qhwi/qhwi_pix.h
)
file(GLOB_RECURSE QHWI_ANDROID_WINDOW_SOURCE_FILES
    src/qhwi_android_window.cpp
    include/qhwi/qhwi_android_window.h
)
file(GLOB_RECURSE QHWI_PERFORMANCE_UTIL
    util/qhwi_performance.h
    util/qhwi_performance.cpp
    util/qhwi_metrics.cpp)

if(${QHWI_BUILD_UNIT_TESTS} OR ${QHWI_BUILD_EXAMPLES})
    if(ANDROID)
        set(USE_GLFW_WINDOW_BACKEND OFF)
        set(USE_ANDROID_WINDOW_BACKEND ON)
    else()
        set(USE_GLFW_WINDOW_BACKEND ON)
        set(USE_ANDROID_WINDOW_BACKEND OFF)
    endif()
else()
    set(USE_GLFW_WINDOW_BACKEND OFF)
    set(USE_ANDROID_WINDOW_BACKEND OFF)
endif()

if (QHWI_ENABLE_D3D12_BACKEND)
    add_library(dma STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp
    )
	set_target_properties(dma PROPERTIES FOLDER "ThirdParty")
	
	target_include_directories(dma
		PUBLIC
			${CMAKE_CURRENT_SOURCE_DIR}/external/D3D12MemoryAllocator/include
	)
	
	# Suppress warnings from D3D12MemoryAllocator dependency
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(dma PRIVATE
			-Wno-switch
			-Wno-unused-parameter
			-Wno-missing-field-initializers
		)
	elseif(MSVC)
		target_compile_options(dma PRIVATE
			/wd4100  # unreferenced formal parameter
			/wd4065  # switch statement contains 'default' but no 'case' labels
		)
	endif()
endif()

if(QHWI_ENABLE_COMPRESSED_STORAGE)
    set_target_properties(GDeflate PROPERTIES FOLDER "ThirdParty")
endif()

add_library(qhwi STATIC
    ${QHWI_SOURCE_FILES}
    qhwi_lib.cmake
    $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:${QHWI_D3D12_SOURCE_FILES}>
    $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:${QHWI_VULKAN_SOURCE_FILES}>
    $<$<BOOL:${USE_GLFW_WINDOW_BACKEND}>:${QHWI_GLFW_SOURCE_FILES}>
    $<$<BOOL:${USE_ANDROID_WINDOW_BACKEND}>:${QHWI_ANDROID_WINDOW_SOURCE_FILES}>
    $<$<BOOL:${QHWI_ENABLE_PIX_SUPPORT}>:${QHWI_PIX_SOURCE_FILES}>
    ${QHWI_PERFORMANCE_UTIL}
)

if (QHWI_ENABLE_PIL AND WIN32)
    add_custom_command(TARGET qhwi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qhwi>/PIL"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PIL_BINARY_DIR}/QProfilerInterface.dll"
            "$<TARGET_FILE_DIR:qhwi>/PIL/QProfilerInterface.dll"
        COMMENT "Copying QProfilerInterface.dll to output"
    )
endif()

target_compile_definitions(qhwi
    PUBLIC
        $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:QHWI_BACKEND_D3D12>
        $<$<BOOL:${QHWI_USE_AGILITY_SDK_717_PREVIEW}>:QHWI_USE_AGILITY_SDK_717_PREVIEW>
        $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:QHWI_BACKEND_VULKAN>
        $<$<BOOL:${QHWI_LINUX_WSI}>:QHWI_WSI_${QHWI_LINUX_WSI_UPPERCASE}>
        $<$<BOOL:${QHWI_ENABLE_PIX_SUPPORT}>:QHWI_GPU_CAPTURE_BACKEND_PIX USE_PIX>
        $<$<BOOL:${QHWI_ENABLE_SER}>:QHWI_ENABLE_SER>
        $<$<BOOL:${QHWI_ENABLE_PIL}>:QHWI_ENABLE_PIL>
        $<$<BOOL:${QHWI_ENABLE_COMPRESSED_STORAGE}>:QHWI_ENABLE_COMPRESSED_STORAGE>
        QHWI_PLATFORM_${SYSTEM_NAME_UPPERCASE}
    PRIVATE
        $<$<BOOL:${USE_GLFW_WINDOW_BACKEND}>:QHWI_WINDOW_BACKEND_GLFW>
        $<$<BOOL:${USE_ANDROID_WINDOW_BACKEND}>:QHWI_WINDOW_BACKEND_ANDROID>
    NOMINMAX
)

target_compile_options(qhwi PUBLIC
)

target_include_directories(qhwi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:
           ${QHWI_D3D12_INCLUDE_DIRS}>
        $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:
           ${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-Headers/include>		   
        ${CMAKE_CURRENT_SOURCE_DIR}/data/shaders/include
        ${CMAKE_CURRENT_SOURCE_DIR}/util
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        $<$<BOOL:${QHWI_ENABLE_PIL}>:${PIL_INCLUDE_DIR}>
        ${CMAKE_CURRENT_SOURCE_DIR}/data/precompiled
)

target_include_directories(qhwi
    SYSTEM PUBLIC
        $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:
			${CMAKE_CURRENT_SOURCE_DIR}/external/D3D12MemoryAllocator/include>
        $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:
			${CMAKE_CURRENT_SOURCE_DIR}/external/VulkanMemoryAllocator/include>
)

target_link_libraries(qhwi PUBLIC
    qre
    $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:dxguid ${QHWI_D3D12_PIX_BIN_DIR}/WinPixEventRuntime.lib>
    $<$<AND:$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>,$<BOOL:${QHWI_ENABLE_COMPRESSED_STORAGE}>>:${QHWI_D3D12_DSTORAGE_LIB_DIR}/dstorage.lib>
	$<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:dma>
    $<$<BOOL:${USE_GLFW_WINDOW_BACKEND}>:glfw>
    $<$<BOOL:${QHWI_ENABLE_COMPRESSED_STORAGE}>:GDeflate>
)

qre_set_target_recommended_warnings(qhwi)

# clang-cl on Windows: enable exceptions and suppress warnings not covered by MSVC-style /wd flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(qhwi PRIVATE /EHsc)
    else()
        target_compile_options(qhwi PRIVATE -fexceptions)
    endif()
    target_compile_options(qhwi PRIVATE
        -Wno-missing-field-initializers
        -Wno-cast-function-type-mismatch
    )
    target_compile_options(qre PRIVATE
        -Wno-missing-field-initializers
    )
endif()

install(FILES include/qhwi/qhwi*.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
