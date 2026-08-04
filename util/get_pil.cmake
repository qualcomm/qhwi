#
#  get_pil.cmake
#  qhwi
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

# Profiler Interface Library
# Set QHWI_PIL_DIR to the root of an extracted PIL package to enable PIL support.
# ARM64 Windows and Android only. In Android Studio, set QHWI_PIL_DIR in local.properties.
set(QHWI_PIL_DIR "" CACHE PATH "Path to Profiler Interface Library root directory (contains include/ and bin/)")

if (QHWI_PIL_DIR)
    if (NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64"
             OR (ANDROID AND ANDROID_ABI STREQUAL "arm64-v8a")))
        message(WARNING "QHWI_PIL_DIR is set but PIL is only supported on ARM64 - ignoring")
    elseif (NOT EXISTS "${QHWI_PIL_DIR}/include/QProfiler/QProfilerInterface.h")
        message(WARNING "QHWI_PIL_DIR is set but QProfilerInterface.h not found at ${QHWI_PIL_DIR}/include - PIL support disabled")
    else()
        set(PIL_INCLUDE_DIR "${QHWI_PIL_DIR}/include" CACHE INTERNAL "PIL include directory")
        if (WIN32)
            set(PIL_BINARY_DIR "${QHWI_PIL_DIR}/bin/ARM64" CACHE INTERNAL "PIL binary directory")
        elseif (ANDROID)
            set(PIL_BINARY_DIR "${QHWI_PIL_DIR}/bin/arm64-v8a" CACHE INTERNAL "PIL binary directory")
        endif()
        message(STATUS "PIL found: ${QHWI_PIL_DIR}")
    endif()
endif()

include(CMakeDependentOption)
cmake_dependent_option(QHWI_ENABLE_PIL "Enable Profiler Interface Library (PIL) support" ON
                       "PIL_INCLUDE_DIR" OFF)
