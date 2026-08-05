cmake_minimum_required(VERSION 3.22.1)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Enable multiprocessor compilation when using MSVC
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

include(FetchContent)

macro(DownloadFileFn name url no_extract)
    FetchContent_Declare(${name}
        URL ${url}
        DOWNLOAD_NO_EXTRACT ${no_extract}
    )
    FetchContent_MakeAvailable(${name})
endmacro()

macro(DownloadFile name url)
    DownloadFileFn(${name} ${url} TRUE)
endmacro()
macro(DownloadExtractFile name url)
    DownloadFileFn(${name} ${url} FALSE)
endmacro()

project(qhwi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# clang-cl on Windows requires _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH when the
# Clang version is older than what the installed MSVC STL expects (Clang 19+).
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.0.0")
    add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()

add_subdirectory(external/qre)

option(QHWI_ENABLE_COMPRESSED_STORAGE "Enable DirectStorage (D3D12) and GDeflate-based tile decompression (D3D12 + Vulkan)" OFF)

if(QHWI_ENABLE_COMPRESSED_STORAGE)
    add_subdirectory(external/DirectStorage/GDeflate/GDeflate)

    # runtimeobject.lib is Windows-only; strip it from GDeflate when building for other platforms.
    if(NOT WIN32)
        get_target_property(_libs GDeflate INTERFACE_LINK_LIBRARIES)
        if(_libs)
            list(REMOVE_ITEM _libs "$<LINK_ONLY:runtimeobject.lib>")
            set_target_properties(GDeflate PROPERTIES INTERFACE_LINK_LIBRARIES "${_libs}")
        endif()
    endif()
endif()

if(WIN32)
    option(QHWI_ENABLE_D3D12_BACKEND "Enable DirectX 12 Backend" ON)
    option(QHWI_ENABLE_SER "Enable Shader Execution Reordering (SER) support. Requires Shader Model 6.9 preview drivers." OFF)
    
    # SER requires Agility SDK 717-Preview - automatically enable it
    if(QHWI_ENABLE_SER)
        set(QHWI_USE_AGILITY_SDK_717_PREVIEW ON CACHE BOOL "Use latest Agility SDK 717-Preview. Needed for DirectSR and SER." FORCE)
        message(STATUS "QHWI_ENABLE_SER is ON - automatically enabling QHWI_USE_AGILITY_SDK_717_PREVIEW")
    else()
        option(QHWI_USE_AGILITY_SDK_717_PREVIEW  "Use latest Agility SDK 717-Preview.  Needed for DirectSR." OFF)
    endif()
    
    option(QHWI_ENABLE_VULKAN_BACKEND "Enable Vulkan Backend" ON)
    option(QHWI_ENABLE_PIX_SUPPORT "Enable PIX capture support. Not required for integrations." OFF)
    if(QHWI_ENABLE_PIX_SUPPORT AND NOT QHWI_ENABLE_D3D12_BACKEND)
        message(FATAL_ERROR "QHWI_ENABLE_PIX_SUPPORT requires QHWI_ENABLE_D3D12_BACKEND.")
    endif()
    if(QHWI_ENABLE_SER AND NOT QHWI_ENABLE_D3D12_BACKEND)
        message(FATAL_ERROR "QHWI_ENABLE_SER requires QHWI_ENABLE_D3D12_BACKEND.")
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    option(QHWI_ENABLE_VULKAN_BACKEND "Enable Vulkan Backend" ON)
    set(QHWI_LINUX_WSI "xlib" CACHE STRING "Linux WSI system to enable. May be 'xlib', 'xcb', or 'wayland'")
    set_property(CACHE QHWI_LINUX_WSI PROPERTY STRINGS "xlib" "xcb" "wayland")
elseif(ANDROID)
    option(QHWI_ENABLE_VULKAN_BACKEND "Enable Vulkan Backend" ON)
endif()

option(QHWI_BUILD_UNIT_TESTS "Build qhwi unit tests. Not required for integrations." OFF)
option(QHWI_BUILD_EXAMPLES "Build qhwi examples." OFF)
option(QHWI_BUILD_SHADERS "Build qhwi shaders" OFF)
option(QHWI_ENABLE_QCOM_TILE_SHADING "Enable the use of VK_QCOM_tile_shading" OFF)
if(QHWI_ENABLE_QCOM_TILE_SHADING AND NOT QHWI_ENABLE_VULKAN_BACKEND)
    message(FATAL_ERROR "QHWI_ENABLE_QCOM_TILE_SHADING requires QHWI_ENABLE_VULKAN_BACKEND.")
endif()
set(QHWI_EXAMPLES_FILTER "" CACHE STRING "QHWI Examples build filter")
option(QHWI_ENABLE_TRACY "Enable Tracy profiling support" OFF)
option(QHWI_TRACY_USE_UNIFORM_COLOR "Use a uniform color for all qhwi CPU profiling zones" ON)
set(QHWI_TRACY_GPU_BACKEND "NONE" CACHE STRING "Tracy GPU profiling backend (dx12, vk, or NONE)")
set_property(CACHE QHWI_TRACY_GPU_BACKEND PROPERTY STRINGS "dx12" "d3d12" "vk" "vulkan" "NONE")
if(NOT QHWI_TRACY_GPU_BACKEND STREQUAL "NONE")
    if(NOT QHWI_ENABLE_TRACY)
        message(FATAL_ERROR "QHWI_TRACY_GPU_BACKEND requires QHWI_ENABLE_TRACY.")
    endif()
    # Validate that the backend value is one of the allowed options
    if(NOT (QHWI_TRACY_GPU_BACKEND MATCHES "^(dx12|d3d12)$" OR 
            QHWI_TRACY_GPU_BACKEND MATCHES "^(vk|vulkan)$"))
        message(FATAL_ERROR 
            "Invalid QHWI_TRACY_GPU_BACKEND value: '${QHWI_TRACY_GPU_BACKEND}'. "
            "Allowed values are: dx12, d3d12, vk, vulkan, NONE")
    endif()
    
    # Existing dependency validation
    if(QHWI_TRACY_GPU_BACKEND MATCHES "^(dx12|d3d12)$")
        if(NOT QHWI_ENABLE_D3D12_BACKEND)
            message(FATAL_ERROR "QHWI_TRACY_GPU_BACKEND=${QHWI_TRACY_GPU_BACKEND} requires QHWI_ENABLE_D3D12_BACKEND=ON")
        endif()
    elseif(QHWI_TRACY_GPU_BACKEND MATCHES "^(vk|vulkan)$")
        if(NOT QHWI_ENABLE_VULKAN_BACKEND)
            message(FATAL_ERROR "QHWI_TRACY_GPU_BACKEND=${QHWI_TRACY_GPU_BACKEND} requires QHWI_ENABLE_VULKAN_BACKEND=ON")
        endif()
    endif()
endif()

option(QHWI_DOWNLOAD_DXC "Download DirectXShaderCompiler during qhwi configuration." ON)
if(QHWI_DOWNLOAD_DXC)
    include(GetDxc.cmake)
    get_dxc()
endif()

include(util/get_pil.cmake)

include(qhwi_lib.cmake)

# Include Tracy profiling support
if(QHWI_ENABLE_TRACY)
    include(tracy.cmake)
endif()

if(QHWI_BUILD_UNIT_TESTS)
    add_subdirectory(test)
endif()

if(QHWI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (QHWI_BUILD_SHADERS OR QHWI_BUILD_UNIT_TESTS)
    add_subdirectory(data/shaders)
    add_dependencies(qhwi qhwi_shaders)
endif()
