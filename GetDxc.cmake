#
#  GetDxc.cmake
#  qhwi
#
#  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

include(${CMAKE_CURRENT_LIST_DIR}/external/qre/DownloadFileUtil.cmake)

function(get_dxc)
    # Fallback version (hardcoded, with known hash for reproducible builds when
    # the GitHub API is unavailable or rate-limited).
    set(DXC_FALLBACK_VERSION 1.9.2602)
    set(DXC_FALLBACK_WIN_FILENAME dxc_2026_02_20)
    set(DXC_FALLBACK_WIN_HASH a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e)
    set(DXC_FALLBACK_LINUX_FILENAME dxc_2026_02_20)
    set(DXC_FALLBACK_LINUX_HASH a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60)

    # ---------------------------------------------------------------------------
    # Step 1: Try to discover the latest DXC release via the GitHub API.
    # ---------------------------------------------------------------------------
    set(RELEASE_INFO_FILE "${CMAKE_BINARY_DIR}/dxc_latest_release.json")

    message(STATUS "Fetching latest DXC release info from GitHub...")
    file(DOWNLOAD
        "https://api.github.com/repos/microsoft/DirectXShaderCompiler/releases/latest"
        "${RELEASE_INFO_FILE}"
        STATUS GITHUB_API_STATUS
        TIMEOUT 15
        HTTPHEADER "User-Agent: cmake/qhwi"
    )
    list(GET GITHUB_API_STATUS 0 GITHUB_API_STATUS_CODE)

    set(DXC_WIN_URL "")
    set(DXC_LINUX_URL "")

    if(GITHUB_API_STATUS_CODE EQUAL 0 AND EXISTS "${RELEASE_INFO_FILE}")
        file(READ "${RELEASE_INFO_FILE}" RELEASE_INFO)

        # Detect API errors (rate limit, auth required, etc.)
        if(RELEASE_INFO MATCHES "\"message\":")
            message(STATUS "GitHub API returned an error – falling back to DXC v${DXC_FALLBACK_VERSION}")
        else()
            # Extract tag_name, e.g. "v1.9.2602"
            string(REGEX MATCH "\"tag_name\":\"v([^\"]+)\"" _ "${RELEASE_INFO}")
            set(LATEST_VERSION "${CMAKE_MATCH_1}")

            # Extract Windows zip URL  (dxc_YYYY_MM_DD.zip, not linux_ or pdb_)
            string(REGEX MATCH
                "\"browser_download_url\":\"(https://[^\"]+/dxc_[0-9_]+\\.zip)\""
                _ "${RELEASE_INFO}")
            set(LATEST_WIN_URL "${CMAKE_MATCH_1}")

            # Extract Linux x86_64 tar.gz URL
            string(REGEX MATCH
                "\"browser_download_url\":\"(https://[^\"]+/linux_dxc_[^\"]+\\.tar\\.gz)\""
                _ "${RELEASE_INFO}")
            set(LATEST_LINUX_URL "${CMAKE_MATCH_1}")

            if(LATEST_VERSION AND (LATEST_WIN_URL OR LATEST_LINUX_URL))
                message(STATUS "Latest DXC release: v${LATEST_VERSION}")
                set(DXC_WIN_URL   "${LATEST_WIN_URL}")
                set(DXC_LINUX_URL "${LATEST_LINUX_URL}")
            else()
                message(STATUS "Could not parse latest DXC release – falling back to v${DXC_FALLBACK_VERSION}")
            endif()
        endif()
    else()
        message(STATUS "Could not reach GitHub API (${GITHUB_API_STATUS}) – falling back to DXC v${DXC_FALLBACK_VERSION}")
    endif()

    # ---------------------------------------------------------------------------
    # Step 2: Download and configure DXC for the current host platform.
    # ---------------------------------------------------------------------------

    if(CMAKE_HOST_WIN32)
        set(dxc_SOURCE_DIR ${DEFAULT_DEPENDENCY_DIR})

        if(DXC_WIN_URL)
            # Latest release – no hash verification (hash is unknown in advance)
            download_and_cache_file(
                URL "${DXC_WIN_URL}"
                DESTINATION ${DEFAULT_DEPENDENCY_DIR}
                EXTRACT true
            )
        else()
            # Fallback – known hash for integrity verification
            download_and_cache_file(
                URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_FALLBACK_VERSION}/${DXC_FALLBACK_WIN_FILENAME}.zip"
                DESTINATION ${DEFAULT_DEPENDENCY_DIR}
                EXPECTED_HASH SHA256=${DXC_FALLBACK_WIN_HASH}
                EXTRACT true
            )
        endif()

        # Dxc compiler has two different variables to be set.
        # `DXC_BINARIES_DIR` -> executable to be run on the host machine
        # `DXC_LIBRARIES_DIR` -> library to be linked/shipped with the target machine
        # Compute these two variables separately in order to account for:
        # native: intel -> native intel
        # native: arm64 -> native arm64
        # cross-compilation: intel -> arm64
        # cross-compilation: arm64 -> intel

        # host (intel, arm64) setup
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(DXC_HOST_BINARIES_DIR ${dxc_SOURCE_DIR}/bin/x64 CACHE INTERNAL "")
            set(DXC_HOST_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib/x64 CACHE INTERNAL "")
        elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(DXC_HOST_BINARIES_DIR ${dxc_SOURCE_DIR}/bin/arm64 CACHE INTERNAL "")
            set(DXC_HOST_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib/arm64 CACHE INTERNAL "")
        else()
            message(ERROR "Unrecognized host platform: ${CMAKE_SYSTEM_PROCESSOR}")
        endif()
        # target (intel, arm64) setup
        if(CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT (CMAKE_GENERATOR_PLATFORM STREQUAL ""))
            if(CMAKE_GENERATOR_PLATFORM STREQUAL "x64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "X64")
                set(DXC_BINARIES_DIR ${dxc_SOURCE_DIR}/bin/x64 CACHE INTERNAL "")
                set(DXC_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib/x64)
            elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "arm64" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
                set(DXC_BINARIES_DIR ${dxc_SOURCE_DIR}/bin/arm64 CACHE INTERNAL "")
                set(DXC_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib/arm64)
            else()
                message(ERROR "Unsupported Generator target platform:" ${CMAKE_GENERATOR_PLATFORM})
            endif()
        else()
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(DXC_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib/x64)
                set(DXC_BINARIES_DIR ${dxc_SOURCE_DIR}/bin/x64 CACHE INTERNAL "")
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
                set(DXC_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib/arm64)
                set(DXC_BINARIES_DIR ${dxc_SOURCE_DIR}/bin/arm64 CACHE INTERNAL "")
            else()
                message(ERROR "Unrecognized target platform: ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        endif()

        set(DXC_INCLUDE_DIR ${dxc_SOURCE_DIR}/inc CACHE INTERNAL "")
        set(DXC_LIBRARIES ${DXC_LIBRARIES_DIR}/dxcompiler.lib CACHE INTERNAL "")
        if(QHWI_ENABLE_QCOM_TILE_SHADING)
            set(DXC_BINARIES ${DXC_BINARIES_DIR}/dxcompiler.dll CACHE INTERNAL "")
        else()
            set(DXC_BINARIES ${DXC_BINARIES_DIR}/dxcompiler.dll ${DXC_BINARIES_DIR}/dxil.dll CACHE INTERNAL "")
        endif()

    elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
        if(NOT CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64" AND NOT CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
            message(FATAL_ERROR "Non-x64 Linux host systems not yet supported.")
        endif()

        if(DXC_LINUX_URL)
            download_and_cache_file(
                URL "${DXC_LINUX_URL}"
                DESTINATION ${DEFAULT_DEPENDENCY_DIR}
                EXTRACT true
            )
        else()
            download_and_cache_file(
                URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_FALLBACK_VERSION}/linux_${DXC_FALLBACK_LINUX_FILENAME}.x86_64.tar.gz"
                DESTINATION ${DEFAULT_DEPENDENCY_DIR}
                EXPECTED_HASH SHA256=${DXC_FALLBACK_LINUX_HASH}
                EXTRACT true
            )
        endif()

        # The Linux release tarball's layout is not stable across DXC
        # releases: some extract flat (bin/, lib/, include/ directly under
        # the extraction root), others nest everything one level down under
        # a versioned top-level directory (e.g. linux_dxc_2026_07_29.x86_x64/).
        # Locate the extracted 'dxc' binary rather than assuming a fixed path.
        file(GLOB DXC_LINUX_CANDIDATES
            "${DEFAULT_DEPENDENCY_DIR}/bin/dxc"
            "${DEFAULT_DEPENDENCY_DIR}/*/bin/dxc"
        )
        list(LENGTH DXC_LINUX_CANDIDATES DXC_LINUX_CANDIDATES_COUNT)
        if(DXC_LINUX_CANDIDATES_COUNT EQUAL 0)
            message(FATAL_ERROR "Could not locate the 'dxc' binary after extracting the Linux DXC package into ${DEFAULT_DEPENDENCY_DIR}.")
        endif()
        list(GET DXC_LINUX_CANDIDATES 0 DXC_LINUX_EXECUTABLE)
        get_filename_component(DXC_BINARIES_DIR "${DXC_LINUX_EXECUTABLE}" DIRECTORY)
        get_filename_component(dxc_SOURCE_DIR "${DXC_BINARIES_DIR}" DIRECTORY)

        set(DXC_LIBRARIES_DIR ${dxc_SOURCE_DIR}/lib)
        set(DXC_BINARIES_DIR ${DXC_BINARIES_DIR} CACHE INTERNAL "")
        set(DXC_HOST_BINARIES_DIR ${DXC_BINARIES_DIR})
        set(DXC_INCLUDE_DIR ${dxc_SOURCE_DIR}/include/dxc CACHE INTERNAL "")
        set(DXC_LIBRARIES ${DXC_LIBRARIES_DIR}/libdxcompiler.so CACHE INTERNAL "")
        set(DXC_BINARIES ${DXC_LIBRARIES_DIR}/libdxcompiler.so ${DXC_LIBRARIES_DIR}/libdxil.so CACHE INTERNAL "")

    elseif(CMAKE_HOST_APPLE)
        set(dxc_SOURCE_DIR "${DEFAULT_DEPENDENCY_DIR}/dxc-src")
        set(dxc_INSTALL_DIR "${DEFAULT_DEPENDENCY_DIR}/dxc-install")

        if(NOT EXISTS "${dxc_INSTALL_DIR}/bin/dxc")
            message(STATUS "Building DXC from source for Apple (this may take a while)...")

            if(LATEST_VERSION)
                set(DXC_GIT_TAG "v${LATEST_VERSION}")
            else()
                set(DXC_GIT_TAG "v${DXC_FALLBACK_VERSION}")
            endif()

            if(NOT EXISTS "${dxc_SOURCE_DIR}/.git")
                message(STATUS "Cloning DXC repository at tag ${DXC_GIT_TAG}...")
                execute_process(
                    COMMAND git clone --recursive --branch ${DXC_GIT_TAG} --depth 1 https://github.com/microsoft/DirectXShaderCompiler.git "${dxc_SOURCE_DIR}"
                    RESULT_VARIABLE GIT_CLONE_RESULT
                )
                if(NOT GIT_CLONE_RESULT EQUAL 0)
                    message(FATAL_ERROR "Failed to clone DXC repository")
                endif()
            endif()

            message(STATUS "Configuring DXC...")
            set(DXC_CMAKE_ARGS
                -G "${CMAKE_GENERATOR}"
                -B "${dxc_SOURCE_DIR}/build"
                -C "${dxc_SOURCE_DIR}/cmake/caches/PredefinedParams.cmake"
                "-DCMAKE_BUILD_TYPE=Release"
                "-DCMAKE_INSTALL_PREFIX=${dxc_INSTALL_DIR}"
                "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_HOST_SYSTEM_PROCESSOR}"
                "-DSPIRV_BUILD_TESTS=OFF"
                # SPIRV-Tools' Python generators produce headers without trailing
                # newlines.  On macOS/Xcode -Wnewline-eof is enabled by default and
                # SPIRV-Tools adds -Werror, which turns this into a build failure.
                # Disabling SPIRV_WERROR prevents -Werror from being injected into
                # the SPIRV-Tools compile flags, so the warning is non-fatal.
                "-DSPIRV_WERROR=OFF"
                "-DLLVM_BUILD_TOOLS=OFF"
            )
            if(CMAKE_C_COMPILER)
                list(APPEND DXC_CMAKE_ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
            endif()
            if(CMAKE_CXX_COMPILER)
                list(APPEND DXC_CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
            endif()
            if(CMAKE_MAKE_PROGRAM)
                list(APPEND DXC_CMAKE_ARGS "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
            endif()
            if(CMAKE_TOOLCHAIN_FILE)
                list(APPEND DXC_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
            endif()
            if(CMAKE_OSX_SYSROOT)
                list(APPEND DXC_CMAKE_ARGS "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")
            endif()
            if(CMAKE_OSX_DEPLOYMENT_TARGET)
                list(APPEND DXC_CMAKE_ARGS "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
            endif()

            execute_process(
                COMMAND ${CMAKE_COMMAND} ${DXC_CMAKE_ARGS}
                WORKING_DIRECTORY "${dxc_SOURCE_DIR}"
                RESULT_VARIABLE CMAKE_CONF_RESULT
            )
            if(NOT CMAKE_CONF_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to configure DXC")
            endif()

            message(STATUS "Building DXC...")
            set(DXC_BUILD_ARGS --build "${dxc_SOURCE_DIR}/build" --config Release)
            include(ProcessorCount)
            ProcessorCount(N)
            if(NOT N EQUAL 0)
                list(APPEND DXC_BUILD_ARGS "-j${N}")
            endif()

            execute_process(
                COMMAND ${CMAKE_COMMAND} ${DXC_BUILD_ARGS}
                WORKING_DIRECTORY "${dxc_SOURCE_DIR}"
                RESULT_VARIABLE CMAKE_BUILD_RESULT
            )
            if(NOT CMAKE_BUILD_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to build DXC")
            endif()

            message(STATUS "Installing DXC...")
            execute_process(
                COMMAND ${CMAKE_COMMAND} --install "${dxc_SOURCE_DIR}/build" --config Release
                WORKING_DIRECTORY "${dxc_SOURCE_DIR}"
                RESULT_VARIABLE CMAKE_INSTALL_RESULT
            )
            if(NOT CMAKE_INSTALL_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to install DXC")
            endif()
        endif()

        set(DXC_LIBRARIES_DIR "${dxc_INSTALL_DIR}/lib")
        set(DXC_BINARIES_DIR "${dxc_INSTALL_DIR}/bin" CACHE INTERNAL "")
        set(DXC_HOST_BINARIES_DIR ${DXC_BINARIES_DIR})
        set(DXC_INCLUDE_DIR "${dxc_INSTALL_DIR}/include/dxc" CACHE INTERNAL "")
        set(DXC_LIBRARIES "${DXC_LIBRARIES_DIR}/libdxcompiler.dylib" CACHE INTERNAL "")
        set(DXC_BINARIES "${DXC_LIBRARIES_DIR}/libdxcompiler.dylib" CACHE INTERNAL "")
    endif()

    set(QHWI_DXC ${DXC_HOST_BINARIES_DIR}/dxc CACHE STRING "")
    message(STATUS "Found `dxc`: ${QHWI_DXC}")
endfunction()