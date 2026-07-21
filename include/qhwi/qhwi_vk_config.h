//
//
//  qhwi_vk_config.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_VK_CONFIG_H__
#define __QHWI_VK_CONFIG_H__

#if defined(QHWI_PLATFORM_WINDOWS) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#    define VK_USE_PLATFORM_WIN32_KHR
#elif defined(QHWI_PLATFORM_ANDROID) && !defined(VK_USE_PLATFORM_ANDROID_KHR)
#    define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(QHWI_PLATFORM_LINUX) && !defined(VK_USE_PLATFORM_XLIB_KHR) && !defined(VK_USE_PLATFORM_XCB_KHR) && !defined(VK_USE_PLATFORM_WAYLAND_KHR)
#    if defined(QHWI_WSI_XLIB)
#        define VK_USE_PLATFORM_XLIB_KHR
#    elif defined(QHWI_WSI_XCB)
#        define VK_USE_PLATFORM_XCB_KHR
#    elif defined(QHWI_WSI_WAYLAND)
#        define VK_USE_PLATFORM_WAYLAND_KHR
#    else
#        error "Unrecognized or unset Linux WSI"
#    endif
#endif

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#ifdef QHWI_TRACY_GPU_BACKEND_VULKAN
#    define TRACY_VK_USE_SYMBOL_TABLE
#    include "tracy/TracyVulkan.hpp"
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
#    include <vulkan/vulkan_wayland.h>
#endif

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#    define VK_SURFACE_EXTENSION_NAMES VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#    define VK_SURFACE_EXTENSION_NAMES VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#    define VK_SURFACE_EXTENSION_NAMES VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#    define VK_SURFACE_EXTENSION_NAMES VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#    define VK_SURFACE_EXTENSION_NAMES VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#else
#    error "Unrecognized or unset Linux WSI"
#endif

#endif
