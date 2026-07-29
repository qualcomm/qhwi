#
#  shaders.cmake
#  qhwi
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

cmake_path(SET VULKAN_SDK $<IF:$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>,$ENV{VULKAN_SDK},"">)

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../android/app/src/main/assets/shaders)
else()
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

function(run_command target)
    set(Options "")
    set(SingleArgs "")
    set(MultipleArgs COMMAND)
    cmake_parse_arguments(QTARGET "${Options}" "${SingleArgs}" "${MultipleArgs}" ${ARGN})
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${QTARGET_COMMAND}
        COMMENT "running command: ${QTARGET_COMMAND}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
endfunction()