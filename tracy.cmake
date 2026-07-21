#
#  tracy.cmake
#  qhwi
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

# Tracy profiler configuration for qhwi
# This file integrates Tracy profiling support into the qhwi library.
# It is included conditionally from the top-level CMakeLists.txt when
# QHWI_ENABLE_TRACY is ON.

# Ensure the qhwi target exists before attempting to configure Tracy
if(NOT TARGET qhwi)
    message(WARNING "Tracy profiling enabled but qhwi target does not exist. Tracy configuration skipped.")
    return()
endif()

# Determine how to obtain the Tracy client target
if(TARGET Tracy::TracyClient)
    message(STATUS "Tracy profiling support detected for qhwi (from parent project)")
    set(TRACY_TARGET Tracy::TracyClient)

elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/tracy/CMakeLists.txt)
    message(STATUS "Building Tracy from submodule for qhwi")

    # Configure Tracy options (without FORCE to respect parent project settings)
    if(NOT DEFINED TRACY_ENABLE)
        set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy profiling")
    endif()
    if(NOT DEFINED TRACY_ON_DEMAND)
        set(TRACY_ON_DEMAND OFF CACHE BOOL "On-demand profiling")
    endif()
    if(NOT DEFINED TRACY_NO_BROADCAST)
        set(TRACY_NO_BROADCAST OFF CACHE BOOL "Disable client discovery by broadcast")
    endif()
    if(NOT DEFINED TRACY_STATIC)
        set(TRACY_STATIC ON CACHE BOOL "Build Tracy as static library")
    endif()

    # Add Tracy subdirectory
    add_subdirectory(external/tracy)

    # Set folder for IDE organization and configure warnings/includes
    if(TARGET TracyClient)
        set_target_properties(TracyClient PROPERTIES FOLDER "qhwi/ThirdParty")

        # Mark Tracy headers as SYSTEM to suppress warnings
        target_include_directories(TracyClient SYSTEM PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/tracy/public>
            $<INSTALL_INTERFACE:include>
        )

        # Disable all warnings for Tracy
        if(MSVC)
            target_compile_options(TracyClient PRIVATE /w)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(TracyClient PRIVATE -w)
        endif()

        # Create an alias for consistent naming
        if(NOT TARGET Tracy::TracyClient)
            add_library(Tracy::TracyClient ALIAS TracyClient)
        endif()

        set(TRACY_TARGET Tracy::TracyClient)
    endif()

else()
    message(WARNING "Tracy profiling enabled but Tracy not found. Please add Tracy as a submodule at external/tracy or ensure it's available from parent project.")
    return()
endif()

# Link qhwi with Tracy using PUBLIC to allow parent project to access it
target_link_libraries(qhwi PUBLIC ${TRACY_TARGET})

# Add Tracy compile definitions to enable profiling
target_compile_definitions(qhwi PUBLIC TRACY_ENABLE)

# Set option for qhwi cpu zone colors
if (QHWI_TRACY_USE_UNIFORM_COLOR)
    target_compile_definitions(qhwi PRIVATE QHWI_USE_UNIFIED_COLOR)
    message(STATUS "QHWI using unified profiling color")
endif()

# Select which backend (if any) to enable for GPU profiling
if(TRACY_ENABLE)
    if(QHWI_TRACY_GPU_BACKEND STREQUAL "dx12" OR QHWI_TRACY_GPU_BACKEND STREQUAL "d3d12")
        target_compile_definitions(qhwi PUBLIC QHWI_TRACY_GPU_BACKEND_D3D12)
        message(STATUS "Tracy GPU profiling (QHWI): D3D12 backend enabled")
    elseif(QHWI_TRACY_GPU_BACKEND STREQUAL "vk" OR QHWI_TRACY_GPU_BACKEND STREQUAL "vulkan")
        target_compile_definitions(qhwi PUBLIC QHWI_TRACY_GPU_BACKEND_VULKAN)
        message(STATUS "Tracy GPU profiling (QHWI): Vulkan backend enabled")
    elseif(QHWI_TRACY_GPU_BACKEND STREQUAL "NONE")
        message(STATUS "Tracy GPU profiling (QHWI): Disabled")
    else()
        message(WARNING "Invalid TRACY_GPU_BACKEND value: ${QHWI_TRACY_GPU_BACKEND}. GPU profiling (QHWI) disabled.")
    endif()
endif()

message(STATUS "Tracy profiling enabled for qhwi")
