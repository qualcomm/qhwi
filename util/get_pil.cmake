#
#  get_pil.cmake
#  qhwi
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

# Profiler Interface Library
# Set these to enable PIL support (ARM64 Windows and Android only).
set(PIL_INCLUDE_DIR "" CACHE PATH "Path to Profiler Interface Library include directory")
set(PIL_BINARY_DIR  "" CACHE PATH "Path to Profiler Interface Library binary directory (ARM64 Windows: .../bin/ARM64, Android: .../bin)")
