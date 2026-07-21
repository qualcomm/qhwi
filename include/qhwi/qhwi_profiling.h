//
//
//  qhwi_profiling.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#pragma once

// Tracy Profiling Macros for QHWI
// These macros encapsulate Tracy profiling instrumentation for cleaner code

#if defined(TRACY_ENABLE)
#    include <tracy/Tracy.hpp>

#    ifdef QHWI_USE_UNIFIED_COLOR
// Default: All qhwi zones share the same color.
#        define QHWI_UNIFIED_COLOR         tracy::Color::Seashell1
#        define QHWI_COLOR_COMMAND_LIST    QHWI_UNIFIED_COLOR // - Command recording
#        define QHWI_COLOR_RESOURCE_CREATE QHWI_UNIFIED_COLOR // - Resource creation
#        define QHWI_COLOR_PIPELINE        QHWI_UNIFIED_COLOR // - Pipeline operations
#        define QHWI_COLOR_QUEUE_SUBMIT    QHWI_UNIFIED_COLOR // - Queue submissions
#        define QHWI_COLOR_TRANSFER        QHWI_UNIFIED_COLOR // - Transfer operations
#        define QHWI_COLOR_COMPUTE         QHWI_UNIFIED_COLOR // - Compute operations
#        define QHWI_COLOR_GRAPHICS        QHWI_UNIFIED_COLOR // - Graphics operations
#        define QHWI_COLOR_RAYTRACING      QHWI_UNIFIED_COLOR // - Ray tracing
#        define QHWI_COLOR_CONTEXT         QHWI_UNIFIED_COLOR // - Context/Device operations
#    else
// Tracy color definitions for different QHWI operations
#        define QHWI_COLOR_COMMAND_LIST    tracy::Color::MediumSeaGreen
#        define QHWI_COLOR_RESOURCE_CREATE tracy::Color::RoyalBlue
#        define QHWI_COLOR_PIPELINE        tracy::Color::SlateBlue
#        define QHWI_COLOR_QUEUE_SUBMIT    tracy::Color::DarkOrange
#        define QHWI_COLOR_TRANSFER        tracy::Color::DodgerBlue
#        define QHWI_COLOR_COMPUTE         tracy::Color::Goldenrod
#        define QHWI_COLOR_GRAPHICS        tracy::Color::Tomato
#        define QHWI_COLOR_RAYTRACING      tracy::Color::MediumVioletRed
#        define QHWI_COLOR_CONTEXT         tracy::Color::DimGray
#    endif

// Profile an entire function with automatic naming
#    define QHWI_PROFILE_FUNCTION() ZoneScoped

// Profile an entire function with a custom name
#    define QHWI_PROFILE_FUNCTION_NAMED(name) ZoneScopedN(name)

// Profile a scope with a custom color
#    define QHWI_PROFILE_SCOPE_COLOR(color) ZoneScopedC(color)

// Profile a scope with a custom name (compile-time constant)
#    define QHWI_PROFILE_SCOPE(name) ZoneScopedN(name)

// Profile a scope with a dynamic name (runtime variable)
#    define QHWI_PROFILE_SCOPE_DYNAMIC(name)                                                                                                                                       \
        ZoneScoped;                                                                                                                                                                \
        ZoneName(name, strlen(name))

// Profile a scope with a custom name and color
#    define QHWI_PROFILE_SCOPE_NAMED_COLOR(name, color) ZoneScopedNC(name, color)

// Mark frame boundaries (useful for applications using qhwi)
#    define QHWI_PROFILE_FRAME_MARK() FrameMark

// API-neutral GPU profiling macros
#    if defined(QHWI_TRACY_GPU_BACKEND_D3D12)
#        include <tracy/TracyD3D12.hpp>
#        include <qhwi/qhwi_d3d12.h>

#        define QHWI_GPU_ZONE_SCOPED(queue, cmd, name)                                                                                                                             \
            TracyD3D12Zone(static_cast<qhwi::D3dCommandQueue*>(queue)->getTracyContext(), static_cast<qhwi::D3dCommandList*>(cmd)->get(), name)

#        define QHWI_GPU_ZONE_SCOPED_DYNAMIC(queue, cmd, name)                                                                                                                     \
            TracyD3D12ZoneTransient(static_cast<qhwi::D3dCommandQueue*>(queue)->getTracyContext(), ___tracy_gpu_zone, static_cast<qhwi::D3dCommandList*>(cmd)->get(), name, true)

#        define QHWI_GPU_NEWFRAME(queue) TracyD3D12NewFrame(static_cast<qhwi::D3dCommandQueue*>(queue)->getTracyContext())

#        define QHWI_GPU_COLLECT(queue, cmd) TracyD3D12Collect(static_cast<qhwi::D3dCommandQueue*>(queue)->getTracyContext())

#    elif defined(QHWI_TRACY_GPU_BACKEND_VULKAN)
#        include <qhwi/qhwi_vk.h>

#        define QHWI_GPU_ZONE_SCOPED(queue, cmdList, name)                                                                                                                         \
            TracyVkZone(static_cast<qhwi::VkCommandQueue*>(queue)->getTracyContext(), static_cast<qhwi::VkCommandList*>(cmdList)->get(), name)

#        define QHWI_GPU_ZONE_SCOPED_DYNAMIC(queue, cmdList, name)                                                                                                                 \
            TracyVkZoneTransient(static_cast<qhwi::VkCommandQueue*>(queue)->getTracyContext(), ___tracy_gpu_zone, static_cast<qhwi::VkCommandList*>(cmdList)->get(), name, true)

#        define QHWI_GPU_NEWFRAME(queue) // Only needed for D3D12, no-op for Vulkan

#        define QHWI_GPU_COLLECT(queue, cmdList) TracyVkCollect(static_cast<qhwi::VkCommandQueue*>(queue)->getTracyContext(), static_cast<qhwi::VkCommandList*>(cmdList)->get())

#    else
#        define QHWI_GPU_ZONE_SCOPED_DYNAMIC(queue, cmd, name)
#        define QHWI_GPU_ZONE_SCOPED(queue, cmd, name)
#        define QHWI_GPU_NEWFRAME(queue)
#        define QHWI_GPU_COLLECT(queue, cmd)
#    endif

#else
// No-op macros when Tracy is disabled
#    define QHWI_PROFILE_FUNCTION()
#    define QHWI_PROFILE_FUNCTION_NAMED(name)
#    define QHWI_PROFILE_SCOPE(name)
#    define QHWI_PROFILE_SCOPE_DYNAMIC(name)
#    define QHWI_PROFILE_SCOPE_COLOR(color)
#    define QHWI_PROFILE_SCOPE_NAMED_COLOR(name, color)
#    define QHWI_PROFILE_FRAME_MARK()

// GPU macros
#    define QHWI_GPU_ZONE_SCOPED(queue, cmd, name)
#    define QHWI_GPU_ZONE_SCOPED_DYNAMIC(queue, cmd, name) ;
#    define QHWI_GPU_NEWFRAME(queue)
#    define QHWI_GPU_COLLECT(queue, cmd)
#endif