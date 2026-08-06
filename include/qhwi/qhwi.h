//
//
//  qhwi.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_H__
#define __QHWI_H__

#include <qhwi/qhwi_config.h>
#include <qhwi/qhwi_enums.h>
#include <qhwi/qhwi_log_util.h>
#include <cstring>
#include <cstdlib>
#include <array>
#include <memory>
#include <span>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <variant>

#include <qre/memory/FreeList.h>
#include <qre/memory/RefCountPtr.h>
#include <qre/memory/StaticVector.h>

#if defined(QHWI_PLATFORM_ANDROID)
struct ANativeWindow;
#elif defined(QHWI_PLATFORM_LINUX)
#    if defined(QHWI_WSI_WAYLAND)
#        include <wayland-client.h>
#    elif defined(QHWI_WSI_XLIB)
#        include <X11/Xlib.h>
#    else
#        include <xcb/xcb.h>
#    endif
#endif

#define QHWI_DECLARE_BUFFER_STATES(s)                                                                                                                                              \
    constexpr BufferState BufferState_VertexShader_##s = { PipelineStage_VertexShader, AccessMode_##s };                                                                           \
    constexpr BufferState BufferState_PixelShader_##s = { PipelineStage_PixelShader, AccessMode_##s };                                                                             \
    constexpr BufferState BufferState_VertexOrPixelShader_##s = { PipelineStage_VertexOrPixelShader, AccessMode_##s };                                                             \
    constexpr BufferState BufferState_ComputeShader_##s = { PipelineStage_ComputeShader, AccessMode_##s };                                                                         \
    constexpr BufferState BufferState_MeshShader_##s = { PipelineStage_MeshShader, AccessMode_##s };                                                                               \
    constexpr BufferState BufferState_RayTracingShader_##s = { PipelineStage_RayTracingShader, AccessMode_##s };

#define QHWI_DECLARE_TEXTURE_STATES(s, t)                                                                                                                                          \
    constexpr TextureState TextureState_VertexShader_##s = { PipelineStage_VertexShader, AccessMode_##s, ImageLayout::e##t };                                                      \
    constexpr TextureState TextureState_PixelShader_##s = { PipelineStage_PixelShader, AccessMode_##s, ImageLayout::e##t };                                                        \
    constexpr TextureState TextureState_VertexOrPixelShader_##s = { PipelineStage_VertexOrPixelShader, AccessMode_##s, ImageLayout::e##t };                                        \
    constexpr TextureState TextureState_ComputeShader_##s = { PipelineStage_ComputeShader, AccessMode_##s, ImageLayout::e##t };                                                    \
    constexpr TextureState TextureState_MeshShader_##s = { PipelineStage_MeshShader, AccessMode_##s, ImageLayout::e##t };                                                          \
    constexpr TextureState TextureState_RayTracingShader_##s = { PipelineStage_RayTracingShader, AccessMode_##s, ImageLayout::e##t };

namespace qhwi
{
constexpr int MaxStringSize = 128;
constexpr int MaxPathSize = 1024;
constexpr int MaxSemanticNameSize = 32;
constexpr int MaxColorAttachments = 8;
constexpr int MaxResolveAttachments = MaxColorAttachments;
constexpr int MaxSubpasses = 8;
constexpr int MaxSubpassInputAttachments = 4;
constexpr int MaxAttachments = 2 * (MaxColorAttachments + 1);
constexpr int MaxDependencies = 16;
constexpr int MaxCommandLists = 16;
constexpr int MaxDescriptorHeapDescriptors = 65536;
constexpr int MaxCbvDescriptors = 4096;
constexpr int MaxSrvDescriptors = 4096;
constexpr int MaxUavDescriptors = 4096;
constexpr int MaxSamplerDescriptors = 1024;
constexpr int MaxDsvDescriptors = 4096;
constexpr int MaxRtvDescriptors = 4096;
constexpr int MaxDescriptorHeaps = 1024;
constexpr int MaxDescriptorSetLayoutsPerPipeline = 16;
constexpr int MaxDescriptorSetsPerPipeline = MaxDescriptorSetLayoutsPerPipeline;
constexpr int MaxDescriptorSets = 2048;
constexpr int MaxDescriptorSetLayoutBindings = 32;
constexpr int MaxDescriptorSetWrites = 128;
constexpr int MaxSwapchainImages = 5;
[[deprecated]] constexpr int MaxSwapchainBuffers = MaxSwapchainImages;
constexpr int MaxDevices = 8;
constexpr int MaxSurfaceFormats = 1024;
constexpr int MaxColorSpaces = 32;
constexpr int MaxVertexShaderInputAttributes = 32;
constexpr int MaxVertexShaderInputBindings = MaxVertexShaderInputAttributes;
constexpr int MaxShaderStages = 8;
constexpr int MaxPushConstantRanges = 4;
constexpr int MaxPushConstantSize = 128;
constexpr int MaxPipelineDynamicStates = 1024;
constexpr int MaxRayPipelineShaderStages = 128;
constexpr int MaxRayPipelineShaderGroups = 128;
constexpr int MaxSubresources = 1024;
constexpr int MaxShaderResourceViews = 32;
constexpr int MaxUnorderedAccessViews = 32;
constexpr int MaxRenderTargetViews = 32;
constexpr int MaxDepthStencilViews = 32;
constexpr int MaxTimerQueries = 1024;
constexpr int MaxTimerQueryHeapSize = MaxTimerQueries << 1;
constexpr int MaxCommandListResources = 1024;
constexpr int MaxBarriers = 1024;
constexpr int MaxDynamicOffsets = 4;

constexpr int DisplayWidth = 0;
constexpr int DisplayHeight = 0;

constexpr int MaxViews = 16;
constexpr int MaxViewports = MaxViews;
constexpr int MaxScissors = MaxViews;

constexpr size_t UploadPoolCompactThreshold = 32u * 1024u * 1024u;

[[deprecated]] const uint32_t SubpassExternal = ~0u;

const uint32_t AttachmentUnused = (~0U);

class CommandList;
class CommandQueue;
class Device;

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
namespace compressedstorage
{
class File;
class Factory;
} // namespace compressedstorage
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

struct Result
{
    Result(ResultCode code, int64_t value = 0)
        : code(code)
        , value(value)
    {
    }

    ResultCode code;
    int64_t value = 0;
};

using Color = std::array<float, 4>;

struct Rect2D
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;
};

struct ClearValue
{
    bool operator==(const ClearValue& rhs) const = default;
    Color rgba = { 0.0f };
    float depth = 1.0f;
    uint32_t stencil = 0;
};

constexpr std::array<uint32_t, int(BindingOffset::eCount)> InitBindingOffsets()
{
    std::array<uint32_t, int(BindingOffset::eCount)> v = { 0 };
    v[int(BindingOffset::eBShift)] = 256;
    v[int(BindingOffset::eUShift)] = 384;
    v[int(BindingOffset::eSShift)] = 128;

    return v;
}

constexpr std::array DefaultBindingSetOffsets = InitBindingOffsets();

struct Subresource
{
    // Describes a subresource of a texture
    // Set the mipmap slice
    constexpr Subresource& setMipSlice(uint16_t v)
    {
        mipSlice = v;
        return *this;
    }

    // Set the array slice
    constexpr Subresource& setArraySlice(uint16_t v)
    {
        arraySlice = v;
        return *this;
    }

    // Set the plane slice
    constexpr Subresource& setPlaneSlice(uint16_t v)
    {
        planeSlice = v;
        return *this;
    }

    bool operator==(const Subresource& rhs) const = default;
    uint16_t mipSlice = 0;
    uint16_t arraySlice = 0;
    uint16_t planeSlice = 0;
};

class Buffer;
class Texture;

struct SubresourceRange
{
    // Describes a range of subresources in a texture
    SubresourceRange() = default;

    constexpr SubresourceRange(Subresource subresource)
        : start(subresource)
        , end(subresource)
    {
    }

    constexpr SubresourceRange(Subresource start, Subresource end)
        : start(start)
        , end(end)
    {
    }

    constexpr SubresourceRange& setStart(const Subresource& v)
    {
        start = v;
        return *this;
    }

    constexpr SubresourceRange& setEnd(const Subresource& v)
    {
        end = v;
        return *this;
    }

    void clamp(const Texture* texture);
    bool isEntireResource(const Texture* texture);

    constexpr bool isEntireResource() const
    {
        return (start.mipSlice == 0) && (start.arraySlice == 0) && (start.planeSlice == 0) && (end.mipSlice == UINT16_MAX) && (end.arraySlice == UINT16_MAX) &&
               (end.planeSlice == UINT16_MAX);
    };

    bool operator==(const SubresourceRange& rhs) const = default;
    Subresource start;
    Subresource end;
};

constexpr SubresourceRange EntireResource = SubresourceRange().setEnd({ .mipSlice = UINT16_MAX, .arraySlice = UINT16_MAX, .planeSlice = UINT16_MAX });

struct BufferState
{
    bool operator==(const BufferState& rhs) const = default;

    constexpr void operator|=(const BufferState& x)
    {
        stages |= x.stages;
        access |= x.access;
    }

    PipelineStageMask stages = PipelineStage_None;
    AccessModeMask access = AccessMode_None;
};

struct BufferBarrier
{
    Buffer* buffer = nullptr;
    BufferState src, dst;
};

struct TextureState
{
    bool operator==(const TextureState& rhs) const = default;

    constexpr void operator|=(const TextureState& x)
    {
        QHWI_ASSERT(layout == x.layout);
        stages |= x.stages;
        access |= x.access;
    }

    PipelineStageMask stages = PipelineStage_None;
    AccessModeMask access = AccessMode_None;
    ImageLayout layout = ImageLayout::eUndefined;
};

constexpr BufferState BufferState_NoAccess = { PipelineStage_None, AccessMode_None };
constexpr BufferState BufferState_Transfer_Src = { PipelineStage_Transfer, AccessMode_TransferRead };
constexpr BufferState BufferState_Transfer_Dst = { PipelineStage_Transfer, AccessMode_TransferWrite };

QHWI_DECLARE_BUFFER_STATES(ConstantBufferRead);
QHWI_DECLARE_BUFFER_STATES(StorageRead);
QHWI_DECLARE_BUFFER_STATES(StorageWrite);
QHWI_DECLARE_BUFFER_STATES(StorageReadWrite);
QHWI_DECLARE_BUFFER_STATES(UnorderedAccess);

constexpr BufferState BufferState_PixelShader_TileTransientReadQcom = { PipelineStage_PixelShader, AccessMode_TileTransientReadQcom };
constexpr BufferState BufferState_ComputeShader_TileTransientReadQcom = { PipelineStage_ComputeShader, AccessMode_TileTransientReadQcom };
constexpr BufferState BufferState_ComputeShader_TileTransientWriteQcom = { PipelineStage_ComputeShader, AccessMode_TileTransientWriteQcom };
constexpr BufferState BufferState_ComputeShader_TileTransientReadWriteQcom = { PipelineStage_ComputeShader, AccessMode_TileTransientReadWriteQcom };

constexpr BufferState BufferState_VertexInput = { PipelineStage_VertexInput, AccessMode_VertexAttributeBufferRead };
constexpr BufferState BufferState_IndexInput = { PipelineStage_IndexInput, AccessMode_IndexBufferRead };
constexpr BufferState BufferState_DrawIndirect_CommandBufferRead = { PipelineStage_DrawIndirect, AccessMode_IndirectCommandBufferRead };
constexpr BufferState BufferState_DispatchIndirect_CommandBufferRead = { PipelineStage_DispatchIndirect, AccessMode_IndirectCommandBufferRead };

constexpr BufferState BufferState_AccelerationStructureBuild = { .stages = PipelineStage_AccelerationStructureBuild, .access = AccessMode::AccessMode_UnorderedAccess };

constexpr BufferState BufferState_ComputeShader_AccelerationStructureRead = { .stages = PipelineStage_ComputeShader, .access = AccessMode::AccessMode_AccelerationStructureRead };

[[deprecated]] constexpr BufferState BufferState_DrawIndirectCommandBufferRead = BufferState_DrawIndirect_CommandBufferRead;
[[deprecated]] constexpr BufferState BufferState_DispatchIndirectCommandBufferRead = BufferState_DispatchIndirect_CommandBufferRead;

constexpr TextureState TextureState_NoAccess = { PipelineStage_None, AccessMode_None, ImageLayout::eUndefined };
constexpr TextureState TextureState_Transfer_Src = { PipelineStage_Transfer, AccessMode_TransferRead, ImageLayout::eTransferSrcOptimal };
constexpr TextureState TextureState_Transfer_Dst = { PipelineStage_Transfer, AccessMode_TransferWrite, ImageLayout::eTransferDstOptimal };

QHWI_DECLARE_TEXTURE_STATES(Sample, ShaderReadOnlyOptimal);
QHWI_DECLARE_TEXTURE_STATES(StorageRead, GenericRead);
QHWI_DECLARE_TEXTURE_STATES(StorageWrite, UnorderedAccess);
QHWI_DECLARE_TEXTURE_STATES(StorageReadWrite, UnorderedAccess);
QHWI_DECLARE_TEXTURE_STATES(UnorderedAccess, UnorderedAccess);

constexpr TextureState TextureState_PixelShader_Sample_LocalRead = { PipelineStage_PixelShader, AccessMode_ShaderSampledRead, ImageLayout::eLocalRead };
constexpr TextureState TextureState_ColorAttachment = { PipelineStage_ColorAttachmentOutput, AccessMode_ColorAttachmentReadWrite, ImageLayout::eColorAttachmentOptimal };
constexpr TextureState TextureState_ColorAttachment_Undefined = { PipelineStage_ColorAttachmentOutput, AccessMode_ColorAttachmentReadWrite, ImageLayout::eUndefined };
constexpr TextureState TextureState_ColorAttachment_LocalRead = { PipelineStage_ColorAttachmentOutput, AccessMode_ColorAttachmentReadWrite, ImageLayout::eLocalRead };
constexpr TextureState TextureState_ColorResolveDest = { PipelineStage_ColorAttachmentOutput, AccessMode_ColorResolveDest, ImageLayout::eColorResolveDest };
constexpr TextureState TextureState_ComputeShader_TileTransientReadQcom = { PipelineStage_ComputeShader, AccessMode_TileTransientReadQcom, ImageLayout::eUnorderedAccess };
constexpr TextureState TextureState_ComputeShader_TileTransientWriteQcom = { PipelineStage_ComputeShader, AccessMode_TileTransientWriteQcom, ImageLayout::eUnorderedAccess };
constexpr TextureState TextureState_ComputeShader_TileTransientReadWriteQcom = { PipelineStage_ComputeShader,
                                                                                 AccessMode_TileTransientReadWriteQcom,
                                                                                 ImageLayout::eUnorderedAccess };

constexpr TextureState TextureState_DepthStencilResolveDest = { PipelineStage_EarlyPixelTests | PipelineStage_ColorAttachmentOutput,
                                                                AccessMode_DepthResolveDest,
                                                                ImageLayout::eDepthResolveDest };

constexpr TextureState TextureState_DepthStencilAttachment = { PipelineStage_EarlyPixelTests | PipelineStage_LatePixelTests,
                                                               AccessMode_DepthStencilAttachmentReadWrite,
                                                               ImageLayout::eDepthStencilAttachmentOptimal };
constexpr TextureState TextureState_DepthStencilAttachment_Undefined = { PipelineStage_EarlyPixelTests | PipelineStage_LatePixelTests,
                                                                         AccessMode_DepthStencilAttachmentReadWrite,
                                                                         ImageLayout::eUndefined };
constexpr TextureState TextureState_DepthStencilAttachment_LocalRead = { PipelineStage_EarlyPixelTests | PipelineStage_LatePixelTests,
                                                                         AccessMode_DepthStencilAttachmentReadWrite,
                                                                         ImageLayout::eLocalRead };
constexpr TextureState TextureState_General = { PipelineStage_BottomOfPipe, AccessMode_None, ImageLayout::eGeneral };
constexpr TextureState TextureState_Present = { PipelineStage_BottomOfPipe, AccessMode_MemoryRead, ImageLayout::ePresent };

constexpr TextureState TextureState_Clear = { PipelineStage_Clear, AccessMode_UnorderedAccess, ImageLayout::eUnorderedAccess };
constexpr TextureState TextureState_ClearTransfer = { PipelineStage_Clear, AccessMode_TransferWrite, ImageLayout::eUnorderedAccess };

constexpr BufferState operator|(const BufferState& x, const BufferState& y)
{
    return { x.stages | y.stages, x.access | y.access };
}

constexpr TextureState operator|(const TextureState& x, const TextureState& y)
{
    if (x == TextureState_NoAccess)
    {
        return y;
    }
    if (y == TextureState_NoAccess)
    {
        return x;
    }
    QHWI_ASSERT(x.layout == y.layout);
    return { x.stages | y.stages, x.access | y.access, x.layout };
}

struct TextureBarrier
{
    constexpr TextureBarrier& setTexture(Texture* v)
    {
        texture = v;
        return *this;
    }

    constexpr TextureBarrier& setSrc(const TextureState& v)
    {
        src = v;
        return *this;
    }

    constexpr TextureBarrier& setDst(const TextureState& v)
    {
        dst = v;
        return *this;
    }

    constexpr TextureBarrier& setSubresourceRange(const SubresourceRange& v)
    {
        subresourceRange = v;
        return *this;
    }

    Texture* texture = nullptr;
    TextureState src, dst;
    SubresourceRange subresourceRange = EntireResource;
};

struct GlobalBarrier
{
    PipelineStageMask srcStages = PipelineStage_None;
    AccessModeMask srcAccess = AccessMode_None;
    PipelineStageMask dstStages = PipelineStage_None;
    AccessModeMask dstAccess = AccessMode_None;
};

namespace ext
{
const int MaxPresentModes = 16;

class Window;
#if defined(QHWI_PLATFORM_LINUX) && defined(QHWI_WSI_XLIB)
using XDisplay = ::Display;
using XWindow = ::Window;
#endif

struct WindowSurface
{
    int width = 0;
    int height = 0;
#if defined(QHWI_PLATFORM_WINDOWS)
    HWND hwnd = nullptr;
#elif defined(QHWI_PLATFORM_ANDROID)
    ANativeWindow* aNativeWindow = nullptr;
#elif defined(QHWI_PLATFORM_LINUX)
#    if defined(QHWI_WSI_WAYLAND)
    struct wl_display* wlDisplay = nullptr;
    struct wl_surface* wlSurface = nullptr;
#    elif defined(QHWI_WSI_XLIB)
    XDisplay* display = nullptr;
    XWindow window = 0;
#    else
    xcb_drawable_t drawable = 0;
    xcb_connection_t* connection = nullptr;
#    endif
#endif
};
#if defined(QHWI_PLATFORM_WINDOWS)
WindowSurface toWindowSurface(const HWND hwnd, int width, int height);
#elif defined(QHWI_PLATFORM_ANDROID)
WindowSurface toWindowSurface(ANativeWindow* window, int width, int height);
#elif defined(QHWI_PLATFORM_LINUX)
#    if defined(QHWI_WSI_WAYLAND)
WindowSurface toWindowSurface(wl_display* wlDisplay, wl_surface* wlSurface, int width, int height);
#    elif defined(QHWI_WSI_XLIB)
WindowSurface toWindowSurface(XDisplay* display, XWindow window, int width, int height);
#    else
WindowSurface toWindowSurface(xcb_drawable_t drawable, xcb_connection_t* connection, int width, int height);
#    endif
#endif

class Swapchain
{
    // Supports creating and managing a series of images for presenting to a display
public:
    virtual ~Swapchain() {}

    struct Description
    {
        Description()
        {
            formatCandidates.resize(DefaultFormatCandidates.size());
            std::copy(DefaultFormatCandidates.begin(), DefaultFormatCandidates.end(), formatCandidates.begin());
        }

        // Set number of swapchain image
        Description& setNumImages(int v)
        {
            numImages = v;
            return *this;
        }

        // Enable / disable vsync
        Description& setVsync(bool v)
        {
            vsync = v;
            return *this;
        }

        // Set debug name
        Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Set automatic barriers
        Description& setAutomaticBarriers(bool v)
        {
            automaticBarriers = v;
            return *this;
        }

        // Set swapchain candidate formats
        // When the swapchain is created, QHWI will choose the first format that is supported
        Description& setFormatCandidates(std::span<const Format> v)
        {
            formatCandidates.resize(v.size());
            std::copy(v.begin(), v.end(), formatCandidates.begin());
            return *this;
        }

        Description& setImageUsage(const TextureUsageMask& v)
        {
            imageUsage = v;
            return *this;
        }

        Description& setPreRotation(bool v)
        {
            preRotation = v;
            return *this;
        }

        int numImages = 0;
        bool vsync = true;
        const char* name = nullptr;
        bool automaticBarriers = false;
        qre::StaticVector<Format, MaxSurfaceFormats> formatCandidates;
        TextureUsageMask imageUsage = TextureUsage::TextureUsage_ColorAttachment;
        bool preRotation = false;
    };

    // Resize the swapchain
    virtual void resize(const WindowSurface& surface) = 0;
    virtual void resize(int width, int height) = 0;
    // Acquire the next swapchain image
    virtual void acquireNextImage() = 0;
    // Present the swapchain image on the screen
    virtual void presentImage(CommandQueue* queue) = 0;
    // Get current swapchain image
    virtual Texture* getCurrentImage() const = 0;
    // Destroy swapchain
    virtual void destroy() {};

    // Get current swapchain image index
    uint32_t getCurrentImageIndex() const { return m_currentImageIndex; }

    virtual int getNumImages() const = 0;
    virtual Format getFormat() const = 0;
    static constexpr std::array DefaultFormatCandidates{ Format::eRgba8Srgb, Format::eBgra8Srgb, Format::eRgba8Unorm, Format::eBgra8Unorm };
    uint32_t m_currentImageIndex = (uint32_t)-1;
};

using SwapchainDesc = Swapchain::Description;

class Window
{
    // Supports creating and rendering to a window
public:
    struct Description
    {
        // Set the windowing system backend
        constexpr Description& setWindowBackend(WindowBackend v)
        {
            windowBackend = v;
            return *this;
        }

        // Set debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Set the window width
        constexpr Description& setWidth(int v)
        {
            width = v;
            return *this;
        }

        // Set the window height
        constexpr Description& setHeight(int v)
        {
            height = v;
            return *this;
        }

        WindowBackend windowBackend = WindowBackend::eAuto;
        const char* name = nullptr;
        int width = DisplayWidth;
        int height = DisplayHeight;
    };

    virtual ~Window() {}

    using WindowResizeCb = void (*)(void* userdata, uint32_t w, uint32_t h);

    void setUserData(void* userdata) { m_userdata = userdata; }

    void setWindowResizeCallback(WindowResizeCb cb) { m_windowResizeCb = cb; }

    virtual void pollEvents() {}

    virtual bool shouldClose() = 0;

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    // Get the window width
    constexpr int getWidth() const { return m_surface.width; }

    // Get the window height
    constexpr int getHeight() const { return m_surface.height; }

    // Get the window surface
    const WindowSurface& getSurface() const { return m_surface; }

protected:
    WindowSurface m_surface;
    WindowResizeCb m_windowResizeCb = nullptr;
    void* m_userdata = nullptr;
};

using WindowDesc = Window::Description;

class GpuCapture
{
    // Supports programmatic capture of GPU traces
    // using various backends such as PIX
public:
    struct Description
    {
        GpuCaptureBackend backend = GpuCaptureBackend::eAuto;
    };

    virtual ~GpuCapture() {}

    // Register the device
    virtual void setDevice(Device* device) {}

    // Start capturing the frame
    virtual void startFrameCapture() = 0;
    // End capturing the frame
    virtual void endFrameCapture() = 0;
    // Set the location to save the capture output to
    virtual void setCaptureOutputPath(const char* path) = 0;
};

using GpuCaptureDesc = GpuCapture::Description;
extern GpuCapture* createGpuCapture(const char* path, const GpuCaptureDesc& desc = {});

struct DisplayChromaticityValues
{
    float RedPrimaryX, RedPrimaryY, GreenPrimaryX, GreenPrimaryY, BluePrimaryX, BluePrimaryY, WhitePointX, WhitePointY;
};

enum DisplayGamut
{
    DisplayGamut_Rec709,
    DisplayGamut_Rec2020
};

constexpr DisplayChromaticityValues DisplayChromaticities[2] = {
    // Display Gamut Rec709 / sRGB: https://en.wikipedia.org/wiki/Rec._709#Primary_chromaticities
    { 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f },
    // Display Gamut Rec2020 : https://en.wikipedia.org/wiki/Rec._2020#System_colorimetry
    { 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }
};

struct DefaultHdrMetadata
{
    static constexpr float MaxLuminance = 1000.0f;
    static constexpr float MinLuminance = 0.001f;
    static constexpr float MaxContentLightLevel = 2000.0f;
    static constexpr float MaxFrameAverageLightLevel = 500.0f;
};
} // namespace ext

struct uvec2
{
    uint32_t x = 0, y = 0;
};

struct uvec3
{
    uint32_t x = 0, y = 0, z = 0;
};

struct uvec4
{
    uint32_t x = 0, y = 0, z = 0, w = 0;
};

struct Extent3D
{
    uint32_t w = 0, h = 0, d = 0;
};

class Texture : public qre::RefCounterDelete
{
    // Supports creating and managing GPU textures
    // Textures support various usage scenarios including:
    //      - GPU sampling from a shader using various filters
    //      - render to texture
    //      - load / store via a shader
    // - Supports compressed GPU formats
    // - Supports planar textures (e.g. NV12, etc.)
public:
    struct Description
    {
        // Set heap flags
        constexpr Description& setHeapFlags(HeapFlags v)
        {
            heapFlags = v;
            return *this;
        }

        // Set the texture width
        constexpr Description& setWidth(uint32_t v)
        {
            w = v;
            return *this;
        }

        // Set the texture height
        constexpr Description& setHeight(uint32_t v)
        {
            h = v;
            return *this;
        }

        // Set the texture depth
        constexpr Description& setDepth(uint32_t v)
        {
            d = v;
            return *this;
        }

        // Set the number of mip levels.  If set to 0, QHWI will calculate automatically
        constexpr Description& setMipLevels(uint32_t v)
        {
            mipLevels = v;
            return *this;
        }

        // Set number of array layers
        constexpr Description& setArrayLayers(uint32_t v)
        {
            arrayLayers = v;
            return *this;
        }

        // Set number of planes
        constexpr Description& setNumPlanes(uint32_t v)
        {
            numPlanes = v;
            return *this;
        }

        // Set number of samples per texel (when used as a multisample render target)
        constexpr Description& setNumSamples(SampleCount v)
        {
            numSamples = v;
            return *this;
        }

        // Set the pixel format
        constexpr Description& setFormat(Format v)
        {
            format = v;
            return *this;
        }

        // Set the component swizzle
        constexpr Description& setComponentSwizzle(ComponentSwizzle r = ComponentSwizzle::eR,
                                                   ComponentSwizzle g = ComponentSwizzle::eG,
                                                   ComponentSwizzle b = ComponentSwizzle::eB,
                                                   ComponentSwizzle a = ComponentSwizzle::eA)
        {
            rSwizzle = r;
            gSwizzle = g;
            bSwizzle = b;
            aSwizzle = a;
            return *this;
        }

        // Set the texture usage bitmask, describing the usage scenarios
        constexpr Description& setUsage(TextureUsageMask v)
        {
            usage = v;
            return *this;
        }

        // Set the texture type
        constexpr Description& setType(TextureType v)
        {
            type = v;
            return *this;
        }

        // Set the debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Set automatic barriers
        constexpr Description& setAutomaticBarriers(bool v)
        {
            automaticBarriers = v;
            return *this;
        }

        HeapFlags heapFlags = 0;
        uint32_t w = 1;
        uint32_t h = 1;
        uint32_t d = 1;
        uint32_t mipLevels = 1; // If 0, calculates the level count based on dimensions
        uint32_t arrayLayers = 1;
        uint32_t numPlanes = 1;
        SampleCount numSamples = SampleCount::e1;
        Format format = Format::eRgba8Unorm;
        ComponentSwizzle rSwizzle = ComponentSwizzle::eR;
        ComponentSwizzle gSwizzle = ComponentSwizzle::eG;
        ComponentSwizzle bSwizzle = ComponentSwizzle::eB;
        ComponentSwizzle aSwizzle = ComponentSwizzle::eA;
        TextureUsageMask usage = TextureUsage_Sampled;
        TextureType type = TextureType::e2D;
        const char* name = nullptr;
        bool automaticBarriers = false;
    };

    Texture(const Description& desc)
        : RefCounterDelete()
        , m_desc(desc)
    {
        m_desc.name = nullptr;
    }

    virtual ~Texture() {}

    typedef void* Handle;

    Description* desc() { return &m_desc; }

    const Description* desc() const { return &m_desc; }

    // Wrap an existing surface handle as a texture
    virtual void updateFromHandle(Handle handle) = 0;
    // Returns the pitch: the bytes per row including alignment
    virtual uint32_t pitch(uint16_t mipLevel = 0) const = 0;

    constexpr uint32_t calcSubresourceIndex(Subresource s) const
    {
        // TODO: Handle plane slice
        return s.mipSlice + s.arraySlice * m_desc.mipLevels;
    }

    bool automaticBarriersEnabled() const { return m_desc.automaticBarriers; }

protected:
    uint32_t calculateMipLevels(uint32_t w, uint32_t h, uint32_t d);
    Description m_desc;
};

using TextureDesc = Texture::Description;

class TextureSubresourceStates final
{
public:
    TextureSubresourceStates() {}

    TextureSubresourceStates(const Texture* texture, TextureState initialState) { init(texture, initialState); }

    void init(const Texture* texture, TextureState initialState);
    void setState(TextureState state, SubresourceRange subresourceRange = EntireResource);
    const TextureState& getState(Subresource subresource = {}) const;

    bool isUnifiedState() const { return m_isUnifiedState; }

private:
    std::vector<TextureState> m_subresourceStates;
    const Texture* m_texture = nullptr;
    bool m_isUnifiedState = true; // true if all subresources share the same state
};

class Sampler : public qre::RefCounterDelete
{
    // Supports sampling a texture using various filters including:
    //      - nearest filter
    //      - bilinear filtering
    //      - trilinear filtering (linear interpolation between mipmaps)
    //      - anisotropic filtering (sampling non-uniformly based on the viewing angle)
public:
    Sampler()
        : RefCounterDelete()
    {
    }

    virtual ~Sampler() {}

    struct Description
    {
        // Set the minification, magnification, and mipmap filter
        constexpr Description& setAllFilters(Filter filter)
        {
            minFilter = magFilter = mipFilter = filter;
            return *this;
        }

        // Set the u,v,w address modes
        constexpr Description& setAllAddressModes(SamplerAddressMode addressMode)
        {
            addressModeU = addressModeV = addressModeW = addressMode;
            return *this;
        }

        // Set minification filter
        constexpr Description& setMinFilter(Filter v)
        {
            minFilter = v;
            return *this;
        }

        // Set magnification filter
        constexpr Description& setMagFilter(Filter v)
        {
            magFilter = v;
            return *this;
        }

        // Set mipmap filter
        constexpr Description& setMipFilter(Filter v)
        {
            mipFilter = v;
            return *this;
        }

        // Set address mode u
        constexpr Description& setAddressModeU(SamplerAddressMode v)
        {
            addressModeU = v;
            return *this;
        }

        // Set address mode v
        constexpr Description& setAddressModeV(SamplerAddressMode v)
        {
            addressModeV = v;
            return *this;
        }

        // Set address mode w
        constexpr Description& setAddressModeW(SamplerAddressMode v)
        {
            addressModeW = v;
            return *this;
        }

        // Set the mipmap LOD bias
        constexpr Description& setMipLodBias(float v)
        {
            mipLodBias = v;
            return *this;
        }

        // Set minimum mipmap LOD
        constexpr Description& setMinLod(float v)
        {
            minLod = v;
            return *this;
        }

        // Set maximum mipmap LOD
        constexpr Description& setMaxLod(float v)
        {
            maxLod = v;
            return *this;
        }

        // Set maximum anisotropy
        constexpr Description& setMaxAnisotropy(float v)
        {
            maxAnisotropy = v;
            return *this;
        }

        // Set additional flags
        constexpr Description& setFlags(SamplerCreateFlags f)
        {
            flags = f;
            return *this;
        }

        // Set reduction mode (currently ignored)
        constexpr Description& setReductionMode(SamplerReductionMode r)
        {
            reductionMode = r;
            return *this;
        }

        // Set comparison operation
        constexpr Description& setCompareOp(CompareOp c)
        {
            compareOp = c;
            return *this;
        }

        Filter minFilter = Filter::eNearest;
        Filter magFilter = Filter::eNearest;
        Filter mipFilter = Filter::eNearest;
        SamplerAddressMode addressModeU = SamplerAddressMode::eClampToEdge;
        SamplerAddressMode addressModeV = SamplerAddressMode::eClampToEdge;
        SamplerAddressMode addressModeW = SamplerAddressMode::eClampToEdge;
        float mipLodBias = 0.0f;
        float minLod = 0.0f;
        float maxLod = std::numeric_limits<float>::max();
        float maxAnisotropy = 1.0f;
        SamplerCreateFlags flags = 0;
        SamplerReductionMode reductionMode = SamplerReductionMode::eNone;
        CompareOp compareOp = CompareOp::eNone;
    };
};

using SamplerDesc = Sampler::Description;

struct ShaderByteCode
{
    // Stores the binary byte code data for a shader
    void* data = nullptr;
    size_t size = 0;
};

struct RayTracingPipelineProperties
{
    // Size (bytes) of shader group handle
    uint32_t shaderGroupHandleSize = 0;

    // Alignment (bytes) of shader group handle
    uint32_t shaderGroupHandleAlignment = 0;

    // Alignment (bytes) of shader table
    uint32_t shaderGroupBaseAlignment = 0;
};

constexpr uint32_t ShaderUnused = UINT32_MAX;

struct RayTracingPipelineShaderGroup
{
    RayTracingPipelineShaderGroup& setGeneralShader(uint32_t v)
    {
        generalShader = v;
        return *this;
    };

    RayTracingPipelineShaderGroup& setClosestHitShader(uint32_t v)
    {
        closestHitShader = v;
        return *this;
    };

    RayTracingPipelineShaderGroup& setAnyHitShader(uint32_t v)
    {
        anyHitShader = v;
        return *this;
    };

    RayTracingPipelineShaderGroup& setIntersectionShader(uint32_t v)
    {
        intersectionShader = v;
        return *this;
    };

    RayTracingPipelineShaderGroup& setShaderGroupType(RayTracingShaderGroupType v)
    {
        type = v;
        return *this;
    };

    RayTracingPipelineShaderGroup& setShaderGroupName(const char* v)
    {
        name = v;
        return *this;
    };

    // Index of ray generation, callable or miss shader
    uint32_t generalShader = ShaderUnused;

    // Index of closest hit shader
    uint32_t closestHitShader = ShaderUnused;

    // Index of any hit shader
    uint32_t anyHitShader = ShaderUnused;

    // Index of intersection shader
    uint32_t intersectionShader = ShaderUnused;

    // Ray tracing shader group type
    RayTracingShaderGroupType type = RayTracingShaderGroupType::eGeneral;

    // Shader group name
    const char* name = nullptr;
};

struct TraceRaysIndirectCommand
{
    uint64_t raygenAddress = 0;
    uint64_t raygenSize = 0;

    uint64_t missTableAddress = 0;
    uint64_t missTableSize = 0;
    uint64_t missTableStride = 0;

    uint64_t hitTableAddress = 0;
    uint64_t hitTableSize = 0;
    uint64_t hitTableStride = 0;

    uint64_t callableTableAddress = 0;
    uint64_t callableTableSize = 0;
    uint64_t callableTableStride = 0;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;
};

class Shader : public qre::RefCounterDelete
{
    // Supports loading and managing GPU shaders.
    // With regards to shading language, the user must provide the platform-specific bytecode
    // (e.g. DXIL when using d3d12, SPIRV when using vk).
    // The user may wish to use a unified shading language, and leverage tools such as
    // spirv-cross to automatically convert one language to others. Alternatively, the user
    // can use platform-specific languages directly for each platform, e.g. HLSL, GLSL.
    // QHWI only cares about the compiled shader bytecode.

public:
    Shader()
        : RefCounterDelete()
    {
    }

    virtual ~Shader() {}

    struct Description
    {
        // Set the debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Set the entry point name
        constexpr Description& setEntryPointName(const char* v)
        {
            entryPointName = v;
            return *this;
        }

        // Set the shader type
        constexpr Description& setShaderType(ShaderType v)
        {
            shaderType = v;
            return *this;
        }

        const char* name = nullptr;
        const char* entryPointName = nullptr;
        ShaderType shaderType = ShaderType::eUnknown;
    };

    const Description* desc() const { return &m_desc; }

protected:
    Description m_desc;
};

using ShaderDesc = Shader::Description;

struct TextureAttachmentData
{
    TextureAttachmentData& setTexture(Texture* v)
    {
        texture = v;
        return *this;
    }

    TextureAttachmentData& setSubresource(Subresource v)
    {
        subresource = v;
        subresourceRange = SubresourceRange({ .mipSlice = {}, .arraySlice = 1, .planeSlice = {} }, {});
        return *this;
    }

    TextureAttachmentData& setSubresourceRange(SubresourceRange v)
    {
        subresourceRange = v;
        return *this;
    }

    SubresourceRange getSubresourceRange() const
    {
        if (subresourceRange.start.arraySlice > subresourceRange.end.arraySlice)
        {
            return SubresourceRange(subresource);
        }
        else
        {
            return subresourceRange;
        }
    }

    // Supports rendering to a texture.
    // A swapchain image attachment is also described using this data structure.
    // Also supports writing to a depth / stencil attachment

    bool operator==(const TextureAttachmentData& rhs) const = default;
    // The texture attachment
    Texture* texture = nullptr;
    //TODO: remove subresource, always store as subresource range (breaking change)
    Subresource subresource;
    SubresourceRange subresourceRange = SubresourceRange({ .mipSlice = {}, .arraySlice = 1, .planeSlice = {} }, {});
};

struct ColorAttachmentDescription
{
    const TextureDesc* desc() const
    {
        QHWI_ASSERT(textureData.texture);
        return textureData.texture->desc();
    }

    // Supports rendering to a color attachment.
    // This data structure is part of the render pass description

    // Set the texture and subresource
    constexpr ColorAttachmentDescription& setTexture(Texture* texture, Subresource subresource = {})
    {
        textureData = TextureAttachmentData{ .texture = texture, .subresource = subresource, .subresourceRange = {} };
        return *this;
    }

    constexpr ColorAttachmentDescription& setTexture(Texture* texture, SubresourceRange subresourceRange)
    {
        textureData = TextureAttachmentData{ .texture = texture, .subresource = {}, .subresourceRange = subresourceRange };
        return *this;
    }

    // Set the load operation: the operation to perform on the attachment at the beginning of a render pass
    constexpr ColorAttachmentDescription& setLoadOp(AttachmentLoadOp v)
    {
        loadOp = v;
        return *this;
    }

    // Set the store operation: the operation to perform on the attachment at the end of a render pass
    constexpr ColorAttachmentDescription& setStoreOp(AttachmentStoreOp v)
    {
        storeOp = v;
        return *this;
    }

    // Set the clear value: the value to clear the attachment at the beginning of the render pass when the load op is clear
    constexpr ColorAttachmentDescription& setClearValue(ClearValue v)
    {
        clearValue = v;
        return *this;
    }

    // Set the initial state of the attachment at the beginning of the render pass.
    // This function should only be called when automatic resource barriers are disabled on this resource.
    [[deprecated]] constexpr ColorAttachmentDescription& setInitialState(TextureState v)
    {
        initialState = v;
        return *this;
    }

    // Set the final state of the attachment at the end of the render pass.
    // This function should only be called when automatic resource barriers are disabled on this resource.
    [[deprecated]] constexpr ColorAttachmentDescription& setFinalState(TextureState v)
    {
        finalState = v;
        return *this;
    }

    // Set the resolve target when rendering using multisampling
    constexpr ColorAttachmentDescription& setResolveTexture(Texture* texture, Subresource subresource = {})
    {
        resolveTarget = { .texture = texture, .subresource = subresource, .subresourceRange = {} };
        return *this;
    }

    bool operator==(const ColorAttachmentDescription& rhs) const = default;

    Format getFormat() { return textureData.texture->desc()->format; }

    inline bool isInputAttachment() const
    {
        switch (loadOp)
        {
        case AttachmentLoadOp::ePreserveLocalRender:
        case AttachmentLoadOp::ePreserveLocalSrv:
        case AttachmentLoadOp::ePreserveLocalUav:
            return true;
        default:
            return false;
        }
    }

    //TODO: make these private, and use getters / setters
    TextureAttachmentData textureData;
    TextureAttachmentData resolveTarget;
    AttachmentLoadOp loadOp = AttachmentLoadOp::eDontCare;
    AttachmentStoreOp storeOp = AttachmentStoreOp::eDontCare;
    ClearValue clearValue;

    [[deprecated]] TextureState getInitialState() const
    {
        QHWI_ASSERT(textureData.texture);
        if (textureData.texture->automaticBarriersEnabled())
        {
            return TextureState_ColorAttachment;
        }
        return initialState;
    }

    [[deprecated]] TextureState getFinalState() const
    {
        QHWI_ASSERT(textureData.texture);
        if (textureData.texture->automaticBarriersEnabled())
        {
            return TextureState_ColorAttachment;
        }
        return finalState;
    }

    TextureState initialState = TextureState_NoAccess, finalState = TextureState_ColorAttachment;
};

struct DepthStencilAttachmentDescription
{
    // Supports writing to a depth / stencil attachment.
    // This data structure is part of the render pass description

    // Set the texture attachment
    constexpr DepthStencilAttachmentDescription& setTexture(Texture* texture, Subresource subresource = {})
    {
        textureData = TextureAttachmentData{ .texture = texture, .subresource = subresource, .subresourceRange = {} };
        return *this;
    }

    constexpr DepthStencilAttachmentDescription& setTexture(Texture* texture, SubresourceRange subresourceRange)
    {
        textureData = TextureAttachmentData{ .texture = texture, .subresource = {}, .subresourceRange = subresourceRange };
        return *this;
    }

    // Set the load operation to perform on the depth buffer data at the beginning of the render pass
    constexpr DepthStencilAttachmentDescription& setDepthLoadOp(AttachmentLoadOp v)
    {
        depthLoadOp = v;
        return *this;
    }

    // Set the store operation to perform on the depth buffer data at the end of the render pass
    constexpr DepthStencilAttachmentDescription& setDepthStoreOp(AttachmentStoreOp v)
    {
        depthStoreOp = v;
        return *this;
    }

    // Set the load operation to perform on the stencil buffer data at the beginning of the render pass
    constexpr DepthStencilAttachmentDescription& setStencilLoadOp(AttachmentLoadOp v)
    {
        stencilLoadOp = v;
        return *this;
    }

    // Set the store operation to perform on the stencil buffer data at the beginning of the render pass
    constexpr DepthStencilAttachmentDescription& setStencilStoreOp(AttachmentStoreOp v)
    {
        stencilStoreOp = v;
        return *this;
    }

    // Set the clear value: the value to clear the attachment at the beginning of the render pass when the load op is clear
    constexpr DepthStencilAttachmentDescription& setClearValue(ClearValue v)
    {
        clearValue = v;
        return *this;
    }

    // Set the initial state of the attachment at the beginning of the render pass.
    // This function should only be called when automatic resource barriers are disabled on this resource.
    [[deprecated]] constexpr DepthStencilAttachmentDescription& setInitialState(TextureState v)
    {
        initialState = v;
        return *this;
    }

    // Set the final state of the attachment at the end of the render pass.
    // This function should only be called when automatic resource barriers are disabled on this resource.
    [[deprecated]] constexpr DepthStencilAttachmentDescription& setFinalState(TextureState v)
    {
        finalState = v;
        return *this;
    }

    // Set the resolve target when writing to a depth/stencil attachment using multisampling
    constexpr DepthStencilAttachmentDescription& setResolveTexture(Texture* texture, Subresource subresource = {})
    {
        resolveTarget = { .texture = texture, .subresource = subresource, .subresourceRange = {} };
        return *this;
    }

    constexpr DepthStencilAttachmentDescription& setDepthResolveMode(ResolveMode v)
    {
        depthResolveMode = v;
        return *this;
    }

    constexpr DepthStencilAttachmentDescription& setStencilResolveMode(ResolveMode v)
    {
        stencilResolveMode = v;
        return *this;
    }

    constexpr DepthStencilAttachmentDescription& setResolveModes(ResolveMode v)
    {
        depthResolveMode = stencilResolveMode = v;
        return *this;
    }

    bool operator==(const DepthStencilAttachmentDescription& rhs) const = default;
    //TODO: make these private, and use getters / setters
    TextureAttachmentData textureData;
    TextureAttachmentData resolveTarget;
    AttachmentLoadOp depthLoadOp = AttachmentLoadOp::eDontCare;
    AttachmentStoreOp depthStoreOp = AttachmentStoreOp::eDontCare;
    AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::eNone;
    AttachmentStoreOp stencilStoreOp = AttachmentStoreOp::eNone;
    ClearValue clearValue;

    TextureState getInitialState() const
    {
        const auto& texture = textureData.texture;
        if (texture && texture->automaticBarriersEnabled())
        {
            return TextureState_NoAccess;
        }
        return initialState;
    }

    TextureState getFinalState() const
    {
        const auto& texture = textureData.texture;
        if (texture && texture->automaticBarriersEnabled())
        {
            return TextureState_DepthStencilAttachment;
        }
        return finalState;
    }

    ResolveMode depthResolveMode = ResolveMode::eNone;
    ResolveMode stencilResolveMode = ResolveMode::eNone;
    TextureState initialState = TextureState_NoAccess, finalState = TextureState_DepthStencilAttachment;
};

struct ColorAttachmentBlendState
{
    // Describes the blending mode for a color attachment.
    // This data structure is part of the pipeline description

    bool operator==(const ColorAttachmentBlendState& s) const = default;
    // Enable / disable blending
    bool enableBlend = false;
    // The source color blend factor
    BlendFactor srcColorBlendFactor = BlendFactor::eZero;
    // The destination color blend factor
    BlendFactor dstColorBlendFactor = BlendFactor::eZero;
    // The color blend operation
    BlendOp colorBlendOp = BlendOp::eAdd;
    // The source alpha blend factor
    BlendFactor srcAlphaBlendFactor = BlendFactor::eZero;
    // The destination alpha blend factor
    BlendFactor dstAlphaBlendFactor = BlendFactor::eZero;
    // The alpha blend operation
    BlendOp alphaBlendOp = BlendOp::eAdd;
    // Enable / disable logic operations
    bool enableLogicOp = false;
    // The logic operation
    LogicOp logicOp = LogicOp::eClear;
    // The color write mask: describes which color channels are written
    ColorWriteMask colorWriteMask = 0xF;
};

struct BlendState
{
    // Describes the blending state.
    // This data structure is part of the pipeline description

    bool operator==(const BlendState& s) const = default;
    // Enable independent blending: separate blending modes for each attachment.
    // When independent blend is disabled, all color attachment blend states must be equal
    bool enableIndependentBlend = false;

    // Returns the number of color attachments
    size_t numColorAttachments() const { return colorAttachmentBlendStates.size(); }

    // The blend states for each color attachment
    qre::StaticVector<ColorAttachmentBlendState, MaxColorAttachments> colorAttachmentBlendStates = {};
};

struct MeshShaderParams
{
    // Describes the mesh shader stage parameters.
    // This data structure is part of the pipeline description

    bool operator==(const MeshShaderParams& s) const = default;
    // The maximum threads per amplification threadgroup along each dimension
    uint32_t maxThreadsPerAmplificationThreadgroup = 0;
    // The maximum threads per mesh threadgroup along each dimension
    uint32_t maxThreadsPerMeshThreadgroup = 0;
};

struct RasterizerState
{
    // Describes the rasterizer state.
    // This data structure is part of the pipeline description

    bool operator==(const RasterizerState& s) const = default;
    // The fill mode (e.g. solid, wireframe, etc.)
    FillMode fillMode = FillMode::eSolid;
    // The cull mode
    CullMode cullMode = CullMode::eBack;
    // The culling winding order
    bool frontFaceCounterClockwise = false;
    // The depth bias, added to the depth values
    int depthBias = 0;
    // The maximum (or minimum) depth bias
    float depthBiasClamp = 0.0f;
    // Enable clipping based on the depth value
    bool enableDepthClipping = true;
    // Enable conservative rasterization (if supported)
    bool enableConservativeRasterization = false;
};

struct MultisampleState
{
    // Describes the multisample state.
    // This data structure is part of the pipeline description

    bool operator==(const MultisampleState& s) const = default;
    // The number of samples
    SampleCount sampleCount = SampleCount::e1;
    // Enable alpha to coverage
    bool enableAlphaToCoverage = false;
};

struct StencilOpState
{
    // Describes the stencil operation state.
    // This data structure is part of the pipeline description

    bool operator==(const StencilOpState& s) const = default;
    // The action performed when stencil test fails
    StencilOp failOp = StencilOp::eKeep;
    // The action performed when stencil test passes and depth test fails
    StencilOp depthFailOp = StencilOp::eKeep;
    // The action performed when stenciltest passes
    StencilOp passOp = StencilOp::eKeep;
    // The comparision operator
    CompareOp compareOp = CompareOp::eAlways;
};

struct DepthStencilState
{
    // Describes the depth / stencil state.
    // This data structure is part of the pipeline description

    bool operator==(const DepthStencilState& s) const = default;
    // Enable depth test
    bool enableDepthTest = false;
    // Enable writing to the depth attachment
    bool enableDepthWrite = false;
    // Depth comparison operator
    CompareOp depthCompareOp = CompareOp::eLess;
    // Enable stencil test
    bool enableStencilTest = false;
    // The stencil read mask affecting which bits are read
    uint8_t stencilReadMask = 0xff;
    // The stencil write mask affecting which bits are written
    uint8_t stencilWriteMask = 0xff;
    // The stencil operation state applied on front-facing polygons
    StencilOpState frontFace;
    // The stencil operation state applied on back-facing polygons
    StencilOpState backFace;
    // The stencil reference value
    uint32_t stencilReference = 0;
};

struct VertexInputAttributeDescription
{
    // Describes a vertex input attribute.
    // A vertex input attribute represents per-vertex data such as position, normal, texcoord.
    // The data can also be used per-instance (e.g. per polygon) instead of per-vertex.
    // This data is read by the vertex shader.
    // The user can encode multiple attributes in the same buffer (interleaved) or in multiple buffers.

    bool operator==(const VertexInputAttributeDescription& s) const = default;
    // The semantic name when using HLSL as the shading language.  This value is only used by d3d12 backend.
    char semanticName[MaxSemanticNameSize];
    // The semantic index when using HLSL as the shading language.  This value is only used by d3d12 backend.
    uint32_t semanticIndex = 0;
    // The binding index.  Note: this must match the value declared in the shader.
    uint32_t binding = 0;
    // The data format
    Format format = Format::eUnknown;
    // An offset into the buffer
    uint32_t offset = 0;
    // The stride: number of bytes per vertex data entry
    uint32_t stride = 0;
    // The input rate describing how the data is used: e.g. per vertex or per instance
    VertexInputRate inputRate = VertexInputRate::eVertex;
};

struct VertexInputState
{
    // Describes the vertex input state including all the attributes.
    // This data structure is part of the pipeline description

    bool operator==(const VertexInputState& s) const = default;
    qre::StaticVector<VertexInputAttributeDescription, MaxVertexShaderInputAttributes> vertexInputAttributeDescriptions;
};

struct ViewportState
{
    bool operator==(const ViewportState& s) const = default;
    uint32_t numViewports = 1;
};

struct ScissorState
{
    bool operator==(const ScissorState& s) const = default;
    uint32_t numScissors = 1;
};

struct MultiViewState
{
    bool operator==(const MultiViewState& rhs) const = default;
    // When numViews is 0, multiView is disabled
    uint32_t numViews = 0;
};

//TODO: expose memory types
class Buffer : public qre::RefCounterDelete
{
    // Supports GPU buffers
    // The user can specify where the memory is allocated via the HeapType param.
    // TODO: expose better interface for leveraging shared memory on UMA architectures
public:
    struct Description
    {
        constexpr Description& setHeapType(HeapType v)
        {
            heapType = v;
            return *this;
        }

        constexpr Description& setHeapFlags(HeapFlags v)
        {
            heapFlags = v;
            return *this;
        }

        constexpr Description& setUsage(BufferUsageMask v)
        {
            usage = v;
            return *this;
        }

        constexpr Description& setSize(uint64_t v)
        {
            size = v;
            return *this;
        }

        // Set format (optional).  Only needed when using a "typed" buffer
        constexpr Description& setFormat(Format v)
        {
            format = v;
            return *this;
        }

        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Set automatic barriers
        constexpr Description& setAutomaticBarriers(bool v)
        {
            automaticBarriers = v;
            return *this;
        }

        HeapType heapType = HeapType::eDefault;
        HeapFlags heapFlags = 0;
        BufferUsageMask usage = 0;
        uint64_t size = 0;
        Format format = Format::eUnknown;
        const char* name = nullptr;
        bool automaticBarriers = false;
    };

    Buffer(const Description& desc)
        : RefCounterDelete()
        , m_heapType(desc.heapType)
        , m_usage(desc.usage)
        , m_size(desc.size)
        , m_format(desc.format)
        , m_automaticBarriers(desc.automaticBarriers)
    {
    }

    virtual ~Buffer() {}

    virtual uint64_t getDeviceAddress() const = 0;
    virtual void* map(uint64_t offset = 0, uint64_t size = 0) = 0;
    virtual void unmap() = 0;

    //The most recent address returned by call to map
    //The user can call this function when using persistent mapped buffers
    void* mapAddr() const { return m_mapAddr; }

    HeapType heapType() const { return m_heapType; }

    BufferUsageMask usage() const { return m_usage; }

    uint64_t size() const { return m_size; }

    Format format() const { return m_format; }

    bool automaticBarriersEnabled() const { return m_automaticBarriers; }

protected:
    void* m_mapAddr = nullptr;
    HeapType m_heapType = HeapType::eDefault;
    BufferUsageMask m_usage;
    uint64_t m_size;
    Format m_format;
    bool m_automaticBarriers;
};

using BufferDesc = Buffer::Description;

struct Viewport
{
    // Defines the transformation from NDC coordinates (normalized device coordinates)
    // to window coordinates

    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float zNear = 0.0f;
    float zFar = 1.0f;

    [[nodiscard]] float width() const { return w; }

    [[nodiscard]] float height() const { return h; }
};

typedef Rect2D ScissorRect;

struct ScopedMarker;

struct StridedDeviceAddressRegion
{
    // Device virtual address
    uint64_t startAddress = 0;

    // Stride in bytes
    uint64_t stride = 0;

    // Size in bytes
    uint64_t size = 0;
};

class AccelerationStructure : public qre::RefCounterDelete
{
    // Supports ray tracing acceleration structures (BVH)
    // including BLAS (bottom level acceleration structure)
    // and TLAS (top level acceleration structure)

public:
    AccelerationStructure()
        : RefCounterDelete()
    {
    }

    virtual ~AccelerationStructure() {}

    struct Description
    {
        // Set to true if this is a top level acceleration structure
        constexpr Description& setIsTopLevel(bool v)
        {
            isTopLevel = v;
            return *this;
        }

        // Set the allocation size
        constexpr Description& setAllocationSize(size_t v)
        {
            allocationSize = v;
            return *this;
        }

        // Set debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        bool isTopLevel = false;
        size_t allocationSize = 0;
        const char* name = nullptr;
    };

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    virtual qhwi::Buffer* buffer() const = 0;
};

using AccelerationStructureDesc = AccelerationStructure::Description;
using AccelStruct = AccelerationStructure;
using AccelStructDesc = AccelerationStructureDesc;

class AccelerationStructureBuildInfo
{
public:
    uint64_t accelerationStructureSize = 0;
    uint64_t updateScratchSize = 0;
    uint64_t buildScratchSize = 0;
};

struct DescriptorSetLayoutBinding
{
    // Describes a binding slot in a descriptor set layout.
    // This data structure should match the declarations in the shader

    bool operator<(const DescriptorSetLayoutBinding& v) const { return ((this->type < v.type) || ((this->type == v.type) && (this->shaderRegister < v.shaderRegister))); }

    // The shader register as declared in the shader code
    uint32_t shaderRegister = 0;
    // The descriptor type
    DescriptorType type;
    // The number of descriptors
    uint32_t count = 1;
    // The shader stages that access this binding
    ShaderStageMask shaderStages;
    // Additional flags
    DescriptorBindingFlags flags = 0;
};

struct DescriptorBufferInfo
{
    // Describes a buffer binding in a descriptor set

    // The buffer
    qre::RefCountPtr<Buffer> buffer;
    // The buffer offset
    uint64_t offset = 0;
    // The range of the buffer data
    uint64_t range = 0;
    // The buffer stride (bytes between consecutive elements)
    uint64_t stride = 0;
};

struct DescriptorTextureInfo
{
    // Describes a descriptor set texture binding

    // The texture
    qre::RefCountPtr<Texture> texture;
    // The subresource range of the texture that is accessed
    SubresourceRange subresourceRange = EntireResource;
    // The image view type
    TextureType imageViewType = TextureType::eUndefined;
};

struct DescriptorSamplerInfo
{
    // Describes a descriptor set sampler binding

    // The sampler
    qre::RefCountPtr<Sampler> sampler;
};

struct DescriptorAccelerationStructureInfo
{
    // Describes a descriptor set ray tracing acceleration structure binding

    // The acceleration structure
    qre::RefCountPtr<AccelerationStructure> accelerationStructure;
};

struct DescriptorSetBinding : public qre::RefCounterDelete
{
    // Describes the resources at a specific binding in a descriptor set

    DescriptorSetBinding()
        : RefCounterDelete()
    {
    }

    virtual ~DescriptorSetBinding()
    {
        delete[] m_bufferInfos;
        delete[] m_textureInfos;
        delete[] m_samplerInfos;
        delete[] m_accelStructInfos;
    }

    // Initialize as buffers
    void initAsBufferInfos(uint32_t count, DescriptorType type, uint32_t shaderRegister)
    {
        m_count = count;
        m_type = type;
        m_shaderRegister = shaderRegister;
        m_bufferInfos = new DescriptorBufferInfo[count];
    }

    // Initialize as textures
    void initAsTextureInfos(uint32_t count, DescriptorType type, uint32_t shaderRegister)
    {
        m_count = count;
        m_type = type;
        m_shaderRegister = shaderRegister;
        m_textureInfos = new DescriptorTextureInfo[count];
    }

    // Initialize as samplers
    void initAsSamplerInfos(uint32_t count, uint32_t shaderRegister)
    {
        m_count = count;
        m_type = DescriptorType::eSampler;
        m_shaderRegister = shaderRegister;
        m_samplerInfos = new DescriptorSamplerInfo[count];
    }

    // Initialize as acceleration structures
    void initAsAccelStructInfos(uint32_t count, uint32_t shaderRegister)
    {
        m_count = count;
        m_type = DescriptorType::eAccelerationStructure;
        m_shaderRegister = shaderRegister;
        m_accelStructInfos = new DescriptorAccelerationStructureInfo[count];
    }

    DescriptorBufferInfo& bufferInfo(uint32_t index)
    {
        QHWI_ASSERT(index < m_count);
        return m_bufferInfos[index];
    }

    DescriptorTextureInfo& textureInfo(uint32_t index)
    {
        QHWI_ASSERT(index < m_count);
        return m_textureInfos[index];
    }

    DescriptorSamplerInfo& samplerInfo(uint32_t index)
    {
        QHWI_ASSERT(index < m_count);
        return m_samplerInfos[index];
    }

    DescriptorAccelerationStructureInfo& accelStructInfo(uint32_t index)
    {
        QHWI_ASSERT(index < m_count);
        return m_accelStructInfos[index];
    }

    uint32_t count() const { return m_count; }

    DescriptorType type() const { return m_type; };

    uint32_t shaderRegister() const { return m_shaderRegister; };

    uint32_t m_count = 0;
    DescriptorBufferInfo* m_bufferInfos = nullptr;
    DescriptorTextureInfo* m_textureInfos = nullptr;
    DescriptorSamplerInfo* m_samplerInfos = nullptr;
    DescriptorAccelerationStructureInfo* m_accelStructInfos = nullptr;
    DescriptorType m_type{};
    uint32_t m_shaderRegister = 0;
};

struct PushConstantRange
{
    // Defines a range of memory that stores push constant data

    // The shader register, as declared in the shader
    uint32_t shaderRegister = 0;
    // The register space, as declared in the shader
    uint32_t registerSpace = 0;
    // The shader stages where this data is accessed
    ShaderStageMask shaderStages = (ShaderStageMask)0;
    // The offset into the push constant data
    uint32_t offset = 0;
    // The size (bytes)
    uint32_t size = 0;
};

#define QHWI_ADD_BINDING_FN0(descType)                                                                                                                                             \
    DescriptorSetLayout::Description& add##descType(uint32_t shaderRegister, ShaderStageMask shaderStages, DescriptorBindingFlags flags = 0)                                       \
    {                                                                                                                                                                              \
        addBinding({ .shaderRegister = shaderRegister, .type = DescriptorType::e##descType, .count = 1, .shaderStages = shaderStages, .flags = flags });                           \
        return *this;                                                                                                                                                              \
    }

#define QHWI_ADD_BINDING_FN1(descType)                                                                                                                                             \
    DescriptorSetLayout::Description& add##descType##s(uint32_t shaderRegister, ShaderStageMask shaderStages, uint32_t count = 1, DescriptorBindingFlags flags = 0)                \
    {                                                                                                                                                                              \
        addBinding({ .shaderRegister = shaderRegister, .type = DescriptorType::e##descType, .count = count, .shaderStages = shaderStages, .flags = flags });                       \
        return *this;                                                                                                                                                              \
    }

#define QHWI_ADD_BINDING_FN(descType)                                                                                                                                              \
    QHWI_ADD_BINDING_FN0(descType)                                                                                                                                                 \
    QHWI_ADD_BINDING_FN1(descType)

using BindingSetOffsets = std::array<uint32_t, size_t(BindingOffset::eCount)>;

class DescriptorSetLayout : public qre::RefCounterDelete
{
    // Describes the layout of a descriptor set, including the descriptors used at each binding.
    // This layout must match the shader declaration (and the declaration in the shader bytecode).
    // Supports binding resources to a shader.

    // Some notes regarding shading languages: Different shading languages have different conventions
    // for specifying resource bindings. Currently these interfaces map more closely to HLSL.
    // QHWI has some additional logic to map HLSL-style bindings to GLSL.
    // In HLSL: there's multiple register types (e.g. t, s, b).
    // The user declares a shader register (e.g. t0, s3, b4), and register space parameter (e.g. space = 2).
    // In GLSL: the user declares a binding and set parameter (e.g. binding = 3, set = 6).
    // QHWI uses binding offsets to map different HLSL register types to unique GLSL binding values to avoid
    // collisions.
public:
    DescriptorSetLayout()
        : RefCounterDelete()
    {
    }

    virtual ~DescriptorSetLayout() {}

    struct Description
    {
        Description(uint32_t registerSpace = 0)
            : registerSpace(registerSpace)
        {
        }

        // Add a binding entry
        Description& addBinding(const DescriptorSetLayoutBinding& b)
        {
            bindings.push_back(b);
            return *this;
        }

        // Set debug name
        Description& setName(const char* value)
        {
            name = value;
            return *this;
        }

        // Set the register space, as declared in the shader.
        constexpr Description& setRegisterSpace(uint32_t v)
        {
            registerSpace = v;
            return *this;
        };

        // Customize the binding offsets, which define the mapping of different register types to a binding value in a unified space.
        constexpr Description& setBindingSetOffsets(uint32_t bShift, uint32_t sShift, uint32_t tShift, uint32_t uShift)
        {
            bindingSetOffsets[int(BindingOffset::eBShift)] = bShift;
            bindingSetOffsets[int(BindingOffset::eTShift)] = tShift;
            bindingSetOffsets[int(BindingOffset::eUShift)] = uShift;
            bindingSetOffsets[int(BindingOffset::eSShift)] = sShift;

            return *this;
        }

        constexpr Description& setBindingSetOffsets(const BindingSetOffsets& v)
        {
            bindingSetOffsets = v;
            return *this;
        }

        // Clear the binding offsets
        constexpr Description& clearBindingSetOffsets()
        {
            setBindingSetOffsets(0, 0, 0, 0);
            return *this;
        }

        uint32_t getBindingSetOffset(DescriptorType type) const;
        uint32_t getBindingLayoutIndex(DescriptorType type, uint32_t shaderRegister) const;

        // Declare add binding helper APIs e.g. addConstantBuffer, addByteAddressBuffer, etc.

        QHWI_ADD_BINDING_FN(ConstantBuffer)
        QHWI_ADD_BINDING_FN(ByteAddressBuffer)
        QHWI_ADD_BINDING_FN(RWByteAddressBuffer)
        QHWI_ADD_BINDING_FN(RWStructuredBuffer)
        QHWI_ADD_BINDING_FN(StorageBuffer)
        QHWI_ADD_BINDING_FN(RWTypedBuffer)
        QHWI_ADD_BINDING_FN(StructuredBuffer)
        QHWI_ADD_BINDING_FN(TypedBuffer)
        QHWI_ADD_BINDING_FN(Texture)
        QHWI_ADD_BINDING_FN(SampleWeightImageQCOM)
        QHWI_ADD_BINDING_FN(Sampler)
        QHWI_ADD_BINDING_FN(RWTexture)
        QHWI_ADD_BINDING_FN(StorageImage)
        QHWI_ADD_BINDING_FN(InputAttachment)
        QHWI_ADD_BINDING_FN(AccelerationStructure)

        qre::StaticVector<DescriptorSetLayoutBinding, MaxDescriptorSetLayoutBindings> bindings;
        // by default, use the binding set offsets from the descriptor set layout
        BindingSetOffsets bindingSetOffsets = DefaultBindingSetOffsets;
        const char* name = nullptr;
        uint32_t registerSpace = 0;
    };

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;
};

using DescriptorSetLayoutDesc = DescriptorSetLayout::Description;

class DescriptorSet : public qre::RefCounterDelete
{
    // Supports binding a set of resources for access from a shader
    // Each descriptor set has a corresponding descriptor set layout that describes
    // the layout of the resource bindings
public:
    DescriptorSet()
        : RefCounterDelete()
    {
    }

    virtual ~DescriptorSet() {}

    struct Description
    {
        Description() {}

        Description(DescriptorSetLayout* v)
            : layout(v)
        {
        }

        // Add a constant buffer as read-only
        Description& addConstantBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eConstantBuffer, 0, offsetBytes, rangeBytes);
        }

        // Add a dynamic constant buffer as read-only
        Description& addDynamicConstantBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eDynamicConstantBuffer, 0, offsetBytes, rangeBytes);
        }

        // Add a structured buffer as read-only
        Description& addStructuredBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t strideBytes, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eStructuredBuffer, strideBytes, offsetBytes, rangeBytes);
        }

        // Add a structured buffer as read-write
        Description& addRWStructuredBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t strideBytes, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eRWStructuredBuffer, strideBytes, offsetBytes, rangeBytes);
        }

        Description& addStorageBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t strideBytes, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addRWStructuredBuffer(shaderRegister, buffer, strideBytes, offsetBytes, rangeBytes);
        };

        // Add a byte address buffer as read-only
        Description& addByteAddressBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eByteAddressBuffer, 0, offsetBytes, rangeBytes);
        }

        // Add a byte address buffer with read/write access
        Description& addRWByteAddressBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eRWByteAddressBuffer, 0, offsetBytes, rangeBytes);
        }

        // Add a typed buffer as read-only
        Description& addTypedBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eTypedBuffer, 0, offsetBytes, rangeBytes);
        }

        // Add a typed buffer as read-write
        Description& addRWTypedBuffer(uint32_t shaderRegister, Buffer* buffer, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            return addBuffer(shaderRegister, buffer, DescriptorType::eRWTypedBuffer, 0, offsetBytes, rangeBytes);
        }

        // Add a texture as an input attachment
        Description& addInputAttachment(uint32_t shaderRegister, Texture* texture, SubresourceRange subresourceRange = EntireResource, TextureType viewType = TextureType::e2D)
        {
            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            b->initAsTextureInfos(1, DescriptorType::eInputAttachment, shaderRegister);
            auto& textureInfo = b->textureInfo(0);
            textureInfo.texture = texture;
            textureInfo.subresourceRange = subresourceRange;
            textureInfo.imageViewType = viewType;
            bindings.push_back(b);
            return *this;
        }

        // Add multiple textures as an array of textures (not texture array) as read-only, for sampling.
        // If subresourceRanges is not specified function assumes its EntireResource
        Description& addTextures(uint32_t shaderRegister,
                                 std::span<Texture*> textures,
                                 std::span<SubresourceRange> subresourceRanges = {},
                                 TextureType viewType = TextureType::eUndefined)
        {
            // subresourceRanges can be empty or it needs to be 1-1 mapped to the textures
            QRE_ASSERT((textures.size() == subresourceRanges.size()) || subresourceRanges.empty());

            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            const uint32_t totalTextures = uint32_t(textures.size());

            b->initAsTextureInfos(totalTextures, DescriptorType::eTexture, shaderRegister);
            for (uint32_t j = 0; j < totalTextures; j++)
            {
                auto& textureInfo = b->textureInfo(j);
                textureInfo.texture = textures[j];
                textureInfo.subresourceRange = (subresourceRanges.empty()) ? EntireResource : subresourceRanges[j];
                textureInfo.imageViewType = viewType;
            }
            bindings.push_back(b);
            return *this;
        }

        // Add a texture as read-only, for sampling
        Description& addTexture(uint32_t shaderRegister, Texture* texture, SubresourceRange subresourceRange = EntireResource, TextureType viewType = TextureType::eUndefined)
        {
            std::vector<Texture*> tex = { texture };
            std::vector<SubresourceRange> range = { subresourceRange };
            return addTextures(shaderRegister, tex, range, viewType);
        }

        // Add a texture as read-only, for sampling
        Description& addSampleWeightImageQCOM(uint32_t shaderRegister,
                                              Texture* texture,
                                              SubresourceRange subresourceRange = EntireResource,
                                              TextureType viewType = TextureType::eUndefined)
        {
            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            b->initAsTextureInfos(1, DescriptorType::eSampleWeightImageQCOM, shaderRegister);
            auto& textureInfo = b->textureInfo(0);
            textureInfo.texture = texture;
            textureInfo.subresourceRange = subresourceRange;
            textureInfo.imageViewType = viewType;
            bindings.push_back(b);
            return *this;
        }

        // Add a texture with read/write access
        Description& addRWTexture(uint32_t shaderRegister, Texture* texture, SubresourceRange subresourceRange = EntireResource, TextureType viewType = TextureType::eUndefined)
        {
            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            b->initAsTextureInfos(1, DescriptorType::eRWTexture, shaderRegister);
            auto& textureInfo = b->textureInfo(0);
            textureInfo.texture = texture;
            textureInfo.subresourceRange = subresourceRange;
            textureInfo.imageViewType = viewType;
            bindings.push_back(b);
            return *this;
        }

        Description& addStorageImage(uint32_t shaderRegister, Texture* texture, SubresourceRange subresourceRange = EntireResource, TextureType viewType = TextureType::eUndefined)
        {
            return addRWTexture(shaderRegister, texture, subresourceRange, viewType);
        }

        // Add a sampler
        Description& addSampler(uint32_t shaderRegister, Sampler* sampler)
        {
            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            b->initAsSamplerInfos(1, shaderRegister);
            auto& samplerInfo = b->samplerInfo(0);
            samplerInfo.sampler = sampler;
            bindings.push_back(b);
            return *this;
        }

        // Add a ray-tracing acceleration structure
        Description& addAccelerationStructure(uint32_t shaderRegister, AccelerationStructure* accelStruct)
        {
            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            b->initAsAccelStructInfos(1, shaderRegister);
            auto& accelStructInfo = b->accelStructInfo(0);
            accelStructInfo.accelerationStructure = accelStruct;
            bindings.push_back(b);
            return *this;
        }

        Description& setVariableDescriptorCount(uint32_t value)
        {
            variableDescriptorCount = value;
            return *this;
        }

        Description& setName(const char* value)
        {
            name = value;
            return *this;
        }

        Description& setLayout(DescriptorSetLayout* value)
        {
            layout = value;
            return *this;
        }

        size_t numBindings() const { return bindings.size(); }

        DescriptorType bindingType(uint32_t index) const
        {
            QHWI_ASSERT(layout && layout->desc());
            QHWI_ASSERT(index < numBindings());
            return bindings[index]->m_type;
        }

        DescriptorSetBinding* findBinding(DescriptorType type, uint32_t shaderRegister)
        {
            if (variableDescriptorCount == 0)
            {
                for (auto& binding : bindings)
                {
                    if ((binding->type() == type) && (binding->shaderRegister() == shaderRegister))
                    {
                        return binding;
                    }
                }
            }

            return nullptr;
        }

        DescriptorSetLayoutDesc* layoutDesc() { return layout->desc(); }

        const DescriptorSetLayoutDesc* layoutDesc() const { return layout->desc(); }

        void reorderBindings();
        qre::RefCountPtr<DescriptorSetLayout> layout;
        qre::StaticVector<qre::RefCountPtr<DescriptorSetBinding>, MaxDescriptorSetLayoutBindings> bindings;
        uint32_t variableDescriptorCount = 0;
        const char* name = nullptr;

    private:
        Description& addBuffer(uint32_t shaderRegister, Buffer* buffer, DescriptorType type, uint64_t strideBytes, uint64_t offsetBytes = 0, uint64_t rangeBytes = 0)
        {
            auto b = qre::MakeRefCountPtr(new DescriptorSetBinding);
            b->initAsBufferInfos(1, type, shaderRegister);
            auto& bufferInfo = b->bufferInfo(0);
            bufferInfo.buffer = buffer;
            bufferInfo.offset = offsetBytes;
            bufferInfo.range = rangeBytes;
            bufferInfo.stride = strideBytes;
            bindings.push_back(b);
            return *this;
        }
    };

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    DescriptorSetLayoutDesc* layoutDesc() { return desc()->layoutDesc(); }

    const DescriptorSetLayoutDesc* layoutDesc() const { return desc()->layoutDesc(); }
};

using DescriptorSetDesc = DescriptorSet::Description;

struct DescriptorSetWrite
{
    uint32_t shaderRegister = 0;
    uint32_t arrayIndex = 0;
    DescriptorBufferInfo* bufferInfos = nullptr;
    DescriptorTextureInfo* textureInfos = nullptr;
    DescriptorSamplerInfo* samplerInfos = nullptr;
    DescriptorAccelerationStructureInfo* accelStructInfos = nullptr;
    DescriptorType type;
    uint32_t count = 1;
};

class PipelineLayout;

class Pipeline : public qre::RefCounterDelete
{
public:
    Pipeline()
        : RefCounterDelete()
    {
    }

    PipelineLayout* getLayout() const { return m_layout.Get(); }

    virtual ~Pipeline() {}

protected:
    qre::RefCountPtr<PipelineLayout> m_layout;
};

class PipelineLayout : public qre::RefCounterDelete
{
    // Describes the resource layout of a GPU pipeline including the descriptor set layouts and
    // push constant ranges
public:
    struct Description
    {
        // Add a descriptor set layout
        Description& addDescriptorSetLayout(DescriptorSetLayout* layout)
        {
            descriptorSetLayouts.push_back(layout);
            return *this;
        }

        // Add a push constant range
        Description& addPushConstantRange(const PushConstantRange& range)
        {
            pushConstantRanges.push_back(range);
            return *this;
        }

        Description& setName(const char* value)
        {
            name = value;
            return *this;
        }

        int find(const DescriptorSetLayout* layout) const;

        qre::StaticVector<qre::RefCountPtr<DescriptorSetLayout>, MaxDescriptorSetLayoutsPerPipeline> descriptorSetLayouts;
        qre::StaticVector<PushConstantRange, MaxPushConstantRanges> pushConstantRanges;
        const char* name = nullptr;
        void* pD3DInternalExportHeader = nullptr;
    };

    PipelineLayout(const Description& desc)
        : RefCounterDelete()
        , m_pushConstantRanges(desc.pushConstantRanges)
    {
        for (const auto& s : desc.descriptorSetLayouts)
        {
            uint32_t set = s->desc()->registerSpace;
            m_descSetMask |= 1 << set;
        }
    }

    virtual ~PipelineLayout() {}

    uint32_t descSetMask() const { return m_descSetMask; }

    const PushConstantRange& pushConstantRange(uint32_t index) const { return m_pushConstantRanges[index]; }

protected:
    qre::StaticVector<PushConstantRange, MaxPushConstantRanges> m_pushConstantRanges;
    uint32_t m_descSetMask = 0;
};

using PipelineLayoutDesc = PipelineLayout::Description;

class ComputePipeline : public Pipeline
{
    // Supports configuring the GPU pipeline to run compute shaders
public:
    ComputePipeline()
        : Pipeline()
    {
    }

    virtual ~ComputePipeline() {}

    struct Description
    {
        // Set the compute shader
        Description& setComputeShader(Shader* v)
        {
            computeShader = v;
            return *this;
        }

        // Set the pipeline layout
        Description& setLayout(PipelineLayout* v)
        {
            layout = v;
            return *this;
        }

        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        bool operator==(const Description& s) const = default;
        qre::RefCountPtr<Shader> computeShader;
        qre::RefCountPtr<PipelineLayout> layout;
        const char* name = nullptr;
        void* pD3DInternalExportHeader = nullptr;
    };
};

using ComputePipelineDesc = ComputePipeline::Description;

class GraphicsPipeline : public Pipeline
{
    // Supports configuring the GPU pipeline to run graphics pipelines,
    // including vertex and pixel shaders, mesh and amplification shaders,
    // geometry shaders, and tesselation shaders
    // Describes the entire GPU pipeline state including the rasterizer, blend state, etc.

public:
    GraphicsPipeline()
        : Pipeline()
    {
    }

    virtual ~GraphicsPipeline() {}

    struct Description
    {
        bool operator==(const Description& s) const = default;

        // Set the vertex shader
        Description& setVertexShader(Shader* v)
        {
            vertexShader = v;
            return *this;
        }

        // Set the pixel shader
        Description& setPixelShader(Shader* v)
        {
            pixelShader = v;
            return *this;
        }

        // Set the domain shader
        Description& setDomainShader(Shader* v)
        {
            domainShader = v;
            return *this;
        }

        // Set the hull shader
        Description& setHullShader(Shader* v)
        {
            hullShader = v;
            return *this;
        }

        // Set the geometry shader
        Description& setGeometryShader(Shader* v)
        {
            geometryShader = v;
            return *this;
        }

        // Set the amplification shader
        Description& setAmplificationShader(Shader* v)
        {
            amplificationShader = v;
            return *this;
        }

        // Set the mesh shader
        Description& setMeshShader(Shader* v)
        {
            meshShader = v;
            return *this;
        }

        // Set the blend state
        Description& setBlendState(const BlendState& v)
        {
            blendState = v;
            return *this;
        }

        // Set the rasterizer state
        Description& setRasterizerState(const RasterizerState& v)
        {
            rasterizerState = v;
            return *this;
        }

        // Set the multisample state
        Description& setMultisampleState(const MultisampleState& v)
        {
            multisampleState = v;
            return *this;
        }

        // Set the depth / stencil state
        Description& setDepthStencilState(const DepthStencilState& v)
        {
            depthStencilState = v;
            return *this;
        }

        // Set the vertex input state
        Description& setVertexInputState(const VertexInputState& v)
        {
            vertexInputState = v;
            return *this;
        }

        // Set the viewport state
        Description& setViewportState(const ViewportState& v)
        {
            viewportState = v;
            return *this;
        }

        // Set the scissor state
        Description& setScissorState(const ScissorState& v)
        {
            scissorState = v;
            return *this;
        }

        // Set the pipeline layout
        Description& setLayout(PipelineLayout* v)
        {
            layout = v;
            return *this;
        }

        // Set the primitive topology (points, lines, line strips, etc.)
        Description& setPrimitiveTopology(PrimitiveTopology v)
        {
            primitiveTopology = v;
            return *this;
        }

        // Set the color attachment pixel formats
        Description& setColorAttachmentFormats(const qre::StaticVector<Format, MaxColorAttachments>& v)
        {
            colorAttachmentFormats = v;
            return *this;
        }

        // Set the depth / stencil attachment format
        Description& setDepthStencilFormat(Format v)
        {
            depthStencilFormat = v;
            return *this;
        }

        // Set the mesh shader parameters
        Description& setMeshShaderParams(const MeshShaderParams& v)
        {
            meshShaderParams = v;
            return *this;
        }

        // Set multiview state
        Description& setMultiViewState(const MultiViewState& v)
        {
            multiViewState = v;
            return *this;
        }

        // Set color attachments remapping
        // This allows to remap the locations of color attachments to different fragment shader output locations
        // Reference: https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkRenderingAttachmentLocationInfoKHR.html
        Description& setColorAttachmentRemapping(qhwi::Device* devicePtr, const qre::StaticVector<uint32_t, MaxColorAttachments>& v)
        {
            colorAttachmentRemapping = v;
            return *this;
        }

        // Set primitive restart value
        Description& setPrimitiveRestartValue(PrimitiveRestartValue v)
        {
            primitiveRestartValue = v;
            return *this;
        };

        Description& setCustomResolve(bool v)
        {
            customResolve = v;
            return *this;
        }

        // Set debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        qre::RefCountPtr<Shader> vertexShader;
        qre::RefCountPtr<Shader> pixelShader;
        qre::RefCountPtr<Shader> domainShader;
        qre::RefCountPtr<Shader> hullShader;
        qre::RefCountPtr<Shader> geometryShader;
        qre::RefCountPtr<Shader> amplificationShader;
        qre::RefCountPtr<Shader> meshShader;
        BlendState blendState;
        RasterizerState rasterizerState;
        MultisampleState multisampleState;
        DepthStencilState depthStencilState;
        VertexInputState vertexInputState;
        ViewportState viewportState;
        ScissorState scissorState;
        qre::RefCountPtr<PipelineLayout> layout;
        PrimitiveTopology primitiveTopology = PrimitiveTopology::eTriangleStrip;
        PrimitiveRestartValue primitiveRestartValue = PrimitiveRestartValue::eFFFFFFFF;

        size_t numColorAttachments() const { return colorAttachmentFormats.size(); }

        size_t numUsedColorAttachments() const
        {
            if (colorAttachmentRemapping.empty())
            {
                return numColorAttachments();
            }
            uint32_t numAttachments = 0;
            for (const auto& v : colorAttachmentRemapping)
            {
                if (v != AttachmentUnused)
                {
                    numAttachments++;
                }
            }
            return numAttachments;
        }

        qre::StaticVector<Format, MaxColorAttachments> colorAttachmentFormats;
        Format depthStencilFormat = Format::eUnknown;

        MeshShaderParams meshShaderParams;
        MultiViewState multiViewState;

        qre::StaticVector<uint32_t, MaxColorAttachments> colorAttachmentRemapping;

        bool customResolve = false;

        qre::StaticVector<Format, MaxColorAttachments> customResolveAttachmentFormats;

        const char* name = nullptr;
    };
};

using GraphicsPipelineDesc = GraphicsPipeline::Description;

class RayTracingPipeline : public Pipeline
{
public:
    RayTracingPipeline()
        : Pipeline()
    {
    }

    virtual ~RayTracingPipeline() {}

    struct Description
    {
        Description() {}

        // Set shaders
        Description& setShaders(std::span<const Shader*> v)
        {
            shaders = v;
            return *this;
        };

        // Set shader groups
        Description& setShaderGroups(std::span<const RayTracingPipelineShaderGroup> v)
        {
            shaderGroups = v;
            return *this;
        };

        // Set ray tracing pipeline max ray recursion depth
        Description& setMaxRayRecursionDepth(uint32_t v)
        {
            maxRayRecursionDepth = v;
            return *this;
        };

        // Set pipeline layout
        Description& setLayout(PipelineLayout* v)
        {
            layout = v;
            return *this;
        };

        // Set name
        Description& setName(const char* v)
        {
            name = v;
            return *this;
        };

        // Set max payload size (bytes)
        Description& setMaxPayloadSize(uint32_t v)
        {
            maxPayloadSize = v;
            return *this;
        };

        // Set max attribute size (bytes)
        Description& setMaxAttributeSize(uint32_t v)
        {
            maxAttributeSize = v;
            return *this;
        };

        std::span<const Shader*> shaders;
        std::span<const RayTracingPipelineShaderGroup> shaderGroups;
        uint32_t maxRayRecursionDepth = 1;
        uint32_t maxPayloadSize = 0;
        uint32_t maxAttributeSize = 0;
        qre::RefCountPtr<PipelineLayout> layout = nullptr;
        const char* name = nullptr;
    };
};

using RayTracingPipelineDesc = RayTracingPipeline::Description;

struct BottomLevelTrianglesDescriptor
{
    // Describes the resources provided as input for building a bottom level acceleration structure

    // The vertex buffer
    Buffer* vertexBuffer = nullptr;
    // The vertex buffer offset (bytes)
    uint64_t vertexOffset = 0;
    // The number of bytes between consecutive elements in the vertex buffer
    uint32_t vertexStride = 0;
    // The number of vertices
    uint32_t vertexCount = 0;
    // The vertex buffer format
    Format vertexFormat = Format::eUnknown;

    // The index buffer
    Buffer* indexBuffer = nullptr;
    // The index buffer offset (bytes)
    uint64_t indexOffset = 0;
    // The number of indices
    uint32_t indexCount = 0;
    // The index buffer format
    IndexFormat indexFormat = IndexFormat::eUint16;

    // TODO: Support BLAS build-time transform
};

// Describes contents of a bottom-level acceleration structure
struct BottomLevelDescriptor
{
    enum class ContentsType
    {
        // TODO: Add support for bounding boxes
        Triangles = 0
    };

    BottomLevelDescriptor()
        : contentsType(ContentsType::Triangles)
        , triangles()
    {
    }

    ContentsType contentsType;

    union
    {
        // TODO: Add support for bounding boxes
        BottomLevelTrianglesDescriptor triangles;
    };
};

// Describes a single instance of a BLAS
class TopLevelDescriptor
{
public:
    TopLevelDescriptor()
        : instanceID(0)
        , instanceMask(0)
        , instanceContributionToHitGroupIndex(0)
        , flags(0)
        , blasIndex(0)
    {
    }

    float transform[12] = { 0 };
    uint32_t instanceID                          : 24;
    uint32_t instanceMask                        : 8;
    uint32_t instanceContributionToHitGroupIndex : 24;
    uint32_t flags                               : 8;
    uint32_t blasIndex;
};

class UploadPool
{
    // Provides a pool of staging buffers used by the upload manager.
    // QHWI provides an upload manager as a convenience API to simplify
    // buffer uploads.
public:
    UploadPool() { m_entries.reserve(InitialNumEntries); }

    void destroy() { m_entries.clear(); }

    // Compact the upload pool
    void compact(size_t threshold = UploadPoolCompactThreshold);

    // Clear all the entries used by a commandList
    void reset(CommandList* commandList);

    // Finds an unused block of memory in a staging buffer, that serves as the source buffer for an upload
    // Parameters:
    // - device: a pointer to the device
    // - commandList: the command list doing the upload.
    //   The upload pool keeps track of which command list is using which entries.
    // - size: The size of the data being uploaded
    // - buffer (return parameter): The resulting staging buffer
    // - offset (return parameter): the resulting staging buffer offset (bytes)
    // - alignment (optional): the size alignment (bytes).  The requested size is aligned to a multiple of the alignment parameter.

    bool get(Device* device, CommandList* commandList, size_t size, Buffer*& buffer, size_t& offset, size_t alignment = 1);

private:
    static constexpr size_t MinBufferSize = 65536;

    struct Entry
    {
        // Keeps track of which portion of a staging buffer is used by which command list

        // The staging buffer
        qre::RefCountPtr<Buffer> buffer;
        // The size of the entry in use
        size_t size = 0;
        // The offset (in bytes) to the end of the entry
        size_t end = 0;
    };

    static constexpr uint32_t InitialNumEntries = 256;
    std::unordered_map<CommandList*, std::vector<Entry>> m_entries;
    size_t m_size = 0;
};

class TimerQuery;

struct DispatchMeshParams
{
    // Parameters for dispatching a mesh shader workload
    // The number of amplification shader threadgroups
    std::array<uint32_t, 3> numAmplificationShaderThreadGroups;
    // The number of threads per amplification shader thread group
    std::array<uint32_t, 3> numThreadsPerAmplificationShaderThreadGroup;
    // The number of threads per mesh thread group
    std::array<uint32_t, 3> numThreadsPerMeshThreadGroup;
};

struct CopyBufferRegion
{
    // Parameters for copying from a source to destination buffer
    // The destination buffer offset (bytes)
    size_t dstOffset = 0;
    // The source buffer offset
    size_t srcOffset = 0;
    // The size of the source data (bytes)
    size_t srcSize = 0;
};

struct CopyBufferToTextureRegion
{
    // Parameters for copying from a source buffer to destination texture
    // The destination subresource
    Subresource* dstSubresource = nullptr;
    // The destination origin in the texture
    uvec3 dstOrigin;
    // The destination extent in the texture
    Extent3D dstExtent;
    // The source buffer offset (bytes)
    size_t srcOffset = 0;
};

struct CopyTextureRegion
{
    // Parameters for copying from a source to destination texture
    // The destination subresource
    Subresource* dstSubresource = nullptr;
    // The destination origin
    uvec3 dstOrigin;
    // The source subresource
    Subresource* srcSubresource = nullptr;
    // The source origin
    uvec3 srcOrigin;
    // The source extent
    // The destination extent matches the source extent (no stretching)
    Extent3D srcExtent;
};

struct CopyTextureToBufferRegion
{
    // Parameters for copying from a source texture to destination buffer
    // The destination offset (bytes)
    uint32_t dstOffset = 0;
    // The source extent
    Extent3D srcExtent;
    // The source subresource
    Subresource* srcSubresource = nullptr;
    // The source origin
    uvec3 srcOrigin;
};

struct UpdateBufferRegion
{
    uint32_t dstOffset = 0;
    const void* srcData = nullptr;
    size_t srcSize = 0;
};

struct UpdateTextureRegion
{
    uint32_t arraySlice = 0;
    uint32_t mipLevel = 0;
    const void* srcData = nullptr;
    size_t srcRowPitch = 0;
    size_t srcDepthPitch = 0;
};

struct TileShadingInfoQCOM
{
    constexpr TileShadingInfoQCOM& setEnableTileShading(bool v)
    {
        enableTileShading = v;
        return *this;
    }

    constexpr TileShadingInfoQCOM& setTileApronSize(qre::math::Vec2<uint32_t> v)
    {
        tileApronSize = v;
        return *this;
    }

    bool enableTileShading = false;
    uint32_t totalTileTransientBufferSize = 0;
    qre::math::Vec2<uint32_t> tileApronSize = { 0, 0 };
};

class CommandList : public qre::RefCounterDelete
{
    // Records commands, which are then submitted to the GPU via a queue and
    // executed on the GPU.  For maximum performance, the user should be careful to avoid redundant calls.
    // Additionally, the user should use batch APIs whenever possible.
public:
    CommandList();
    virtual ~CommandList();

    struct Description
    {
        // Set the command list type (primary or secondary).
        constexpr Description& setType(CommandListType v)
        {
            type = v;
            return *this;
        }

        // Set debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Enable support for automatic resource barriers
        constexpr Description& setSupportsAutomaticBarriers(bool v)
        {
            supportsAutomaticBarriers = v;
            return *this;
        }

        // Enable support for opticalFlow
        constexpr Description& setSupportsOpticalFlow(bool v)
        {
            supportsOpticalFlow = v;
            return *this;
        }

        CommandListType type = CommandListType::ePrimary;
        const char* name = nullptr;

        bool supportsAutomaticBarriers = false;
        bool supportsOpticalFlow = false;
    };

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    // Event annotation
    virtual void beginMarker(const char* name) = 0;
    virtual void endMarker() = 0;
    [[nodiscard]] ScopedMarker scopedMarker(const char* name);

    // Begin recording
    virtual void begin() = 0;

    void open() { begin(); }

    // End recording
    virtual void end() = 0;

    void close() { end(); }

    // Returns true if the command list can be reused across frames
    virtual bool isReusable() const { return true; }

    // Begin a graphics rendering pass
    // Each time the user wants to render to a different color attachment, or depth / stencil attachment (or both),
    // the user should begin a new render pass.  Additionally, if the parameters of a color or depth / stencil attachment
    // changes (e.g. the load or store operation, etc.) the user should start a new render pass. On the same token, the user
    // should try to avoid unnecessary extra passes and merge render passes when rendering to the same attachments.
    // Additionally, the user is not allowed to record resource barriers inside a render pass, this must be done before
    // the render pass is started.  This is mainly to accomodate tiled GPU architectures.
    // The Vulkan backends has similar restrictions.  On a tiled GPU, the render pass is executed on a per-tile basis,
    // so the resource barrier would need to be executed per tile instead of once.
    // Parameters:
    // - numColorAttachments: the number of color attachments
    // - colorAttachments: the color attachment descriptions including the load and store operation, etc.
    // - depthStencilAttachment (optional): the depth / stencil attachment
    // TODO: support subpasses

    void beginRenderPass(uint32_t numColorAttachments,
                         const ColorAttachmentDescription* colorAttachments,
                         const DepthStencilAttachmentDescription* depthStencilAttachment = nullptr,
                         const uint32_t* colorAttachmentRemapping = nullptr,
                         bool customResolve = false)
    {
        beginRenderPass(numColorAttachments, colorAttachments, depthStencilAttachment, nullptr, nullptr, colorAttachmentRemapping, nullptr, customResolve);
    }

    // Begin a graphics rendering pass
    // Parameters:
    // - numColorAttachments: the number of color attachments
    // - colorAttachments: the color attachment descriptions including the load and store operation, etc.
    // - depthStencilAttachment: the depth / stencil attachment
    // - multiViewState: the multiview state
    // - multiViewPerViewRenderAreasQCOM (optional): per-view render areas when using VK_QCOM_MULTIVIEW_PER_VIEW_RENDER_AREAS extension
    virtual void beginRenderPass(uint32_t numColorAttachments,
                                 const ColorAttachmentDescription* colorAttachments,
                                 const DepthStencilAttachmentDescription* depthStencilAttachment,
                                 const MultiViewState* multiViewState,
                                 const Rect2D* multiViewPerViewRenderAreasQCOM = nullptr,
                                 const uint32_t* colorAttachmentRemapping = nullptr,
                                 const TileShadingInfoQCOM* tileShadingInfoQCOM = nullptr,
                                 bool customResolve = false)
    {
        QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot start a render pass inside another render pass.");
        QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot start a render pass inside a compute pass.");
        QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot start a render pass inside a transfer pass.");
        QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot start a render pass inside an acceleration structure pass.");

        m_inRenderPass = true;
    }

    // End the render pass
    virtual void endRenderPass() = 0;

    // Begin a custom resolve operation. Draw calls issued after beginCustomResolve() perform a resolve operation if the backend supports a custom resolve pass.
    // Except Vulkan, no other backends support the custom resolve pass.
    virtual void beginCustomResolve() {}

    // Begin a compute pass
    // The user is required to call begin / end compute pass when recording any compute work

    virtual void beginComputePass() = 0;
    // End the compute pass
    virtual void endComputePass() = 0;
    // Begin a transfer pass
    // The user is required to call begin / end transfer pass when recording any transfer (blit) operations
    virtual void beginTransferPass() = 0;

    // End the transfer pass
    virtual void endTransferPass() = 0;
    // Begin acceleration structure pass
    // The user is required to call begin / end transfer pass when recording any acceleration structure operations,
    // such as building the acceleration structure
    virtual void beginAccelerationStructurePass() = 0;

    // End the acceleration structure
    virtual void endAccelerationStructurePass() = 0;

    // Dispatch compute kernel
    // The workload is divided into "threadgroups".
    // Each threadgroup is a collection of multiple threads.
    // The number of threads per threadgroup is specified in the compute shader itself.
    // Multiple threads are scheduled on the GPU to run in parallel.
    // These are referred to as sets (also as warps or wavefronts).

    virtual void dispatch(uint32_t numThreadGroupsX,
                          uint32_t numThreadGroupsY,
                          uint32_t numThreadGroupsZ,
                          uint32_t numThreadsPerThreadgroupX,
                          uint32_t numThreadsPerThreadgroupY,
                          uint32_t numThreadsPerThreadgroupZ) = 0;

    void dispatch(const uint32_t numThreadGroups[3], const uint32_t numThreadsPerThreadgroup[3])
    {
        dispatch(numThreadGroups[0], numThreadGroups[1], numThreadGroups[2], numThreadsPerThreadgroup[0], numThreadsPerThreadgroup[1], numThreadsPerThreadgroup[2]);
    }

    // Dispatch mesh shader
    // Parameters:
    // - dispatchMeshParams: the mesh shader dispatch parameters
    virtual void dispatchMesh(const DispatchMeshParams& p) = 0;
    // Draw primitives
    // Parameters:
    // - vertexCount: the number of vertices
    // - instanceCount: the number of instances
    // - firstVertex: the index of the first vertex to draw
    // - first instance: the instance ID of the first instance to draw
    virtual void draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0) = 0;
    // Draw primitives using indexing
    // Parameters:
    // - indexCount: the number of indices
    // - instanceCount: the number of instances
    // - firstIndex: the base index in the index buffer
    // - vertexOffset: the base offset in the vertex buffer
    // - firstInstance: the instance ID of the first instance to draw
    virtual void drawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0) = 0;
    // Dispatch ray generation shader where the thread count is defined by width * height * depth
    // Parameters:
    // - raygenShaderBindingTable: Shader binding table for the ray generation shader stage
    // - missShaderBindingTable: Shader binding table for the miss shader stage
    // - hitShaderBindingTable: Shader binding table for the hit shader stage
    // - callableShaderBindingTable: Shader binding table for the callable shader stage
    // - width: Width of raygen shader thread group
    // - height: Height of raygen shader thread group
    // - depth: Depth of raygen shader thread group
    virtual void dispatchRays(const StridedDeviceAddressRegion& raygenShaderBindingTable,
                              const StridedDeviceAddressRegion& missShaderBindingTable,
                              const StridedDeviceAddressRegion& hitShaderBindingTable,
                              const StridedDeviceAddressRegion& callableShaderBindingTable,
                              uint32_t width,
                              uint32_t height,
                              uint32_t depth) = 0;
    // Dispatch ray generation shader where the thread count is defined by width * height * depth
    // Parameters:
    // - buffer: Indirect Buffer
    virtual void dispatchRaysIndirect(Buffer* buffer) = 0; // Dispatch compute work indirectly using parameters recorded in a GPU buffer
    // The GPU buffer encodes the number of threadgroups
    // Parameters:
    // - numThreadsPerThreadGroup: the number of threads per thread group (in each dimension)
    // - buffer: the GPU buffer containing the number of threadgroups parameter
    // - offset: the offset into the GPU buffer (in bytes)
    // - dispatchCount: the number of dispatch operations encoded
    virtual void dispatchIndirect(uint32_t numThreadsPerThreadgroupX,
                                  uint32_t numThreadsPerThreadgroupY,
                                  uint32_t numThreadsPerThreadgroupZ,
                                  Buffer* buffer,
                                  size_t offset = 0,
                                  uint32_t dispatchCount = 1) = 0;

    void dispatchIndirect(const uint32_t numThreadsPerThreadgroup[3], Buffer* buffer, size_t offset = 0, uint32_t dispatchCount = 1)
    {
        dispatchIndirect(numThreadsPerThreadgroup[0], numThreadsPerThreadgroup[1], numThreadsPerThreadgroup[2], buffer, offset, dispatchCount);
    }

    // Record draw commands indirectly using parameters recorded in a GPU buffer
    // The GPU buffer encodes the same parameters as the draw API parameters
    // Parameters:
    // - buffer: the GPU buffer containing the draw parameters
    // - offset: the offset into the GPU buffer (in bytes)
    // - drawCount: the number of draw calls encoded indirectly in the buffer
    virtual void drawIndirect(Buffer* buffer, size_t offset = 0, uint32_t drawCount = 1) = 0;
    // Record draw indexed commands indirectly using parameters recorded in a GPU buffer
    // The GPU buffer encodes the same parameters as the draw indexed API parameters
    // Parameters:
    // - buffer: the GPU buffer containing the draw parameters
    // - offset: the offset into the GPU buffer (in bytes)
    // - drawCount: the number of draw calls encoded indirectly in the buffer
    virtual void drawIndexedIndirect(Buffer* buffer, size_t offset = 0, uint32_t drawCount = 1) = 0;

    // Binds a buffer containing per-vertex or per-instance data, for consumption by the vertex shader
    // Parameters:
    // - buffer: the vertex buffer
    // - slot: the binding slot where the buffer is bound (should match the declaration in the shader)
    // - offset: the offset (in bytes) into the buffer
    // - stride: the number of bytes between consecutive elements
    virtual void bindVertexBuffer(Buffer* buffer, uint32_t slot, size_t offset = 0, size_t stride = 0) = 0;

    // Binds a buffer containing indices, for drawing with indexing
    // buffer: the index buffer
    // indexFormat: the format of the indices
    // offset: the offset (in bytes) into the buffer
    virtual void bindIndexBuffer(Buffer* buffer, IndexFormat indexFormat = IndexFormat::eUint32, size_t offset = 0) = 0;

    PipelineLayout* getCurrentPipelineLayout()
    {
        QHWI_ASSERT_MSG(m_currentPipeline, "pipeline layout parameter is null and current pipeline is null");
        return m_currentPipeline->getLayout();
    }

    // Push constant data
    // The GPU has a small portion of high speed memory (128 bytes) that is useful for storing
    // data such as transformation matrices, etc.

    virtual void pushConstants(PipelineLayout* pipelineLayout, PipelineBindPoint pipelineBindPoint, const void* data, uint32_t index = 0) = 0;

    void pushConstants(PipelineBindPoint pipelineBindPoint, const void* data, uint32_t index = 0) { pushConstants(getCurrentPipelineLayout(), pipelineBindPoint, data, index); }

    void pushComputeConstants(PipelineLayout* pipelineLayout, const void* data, uint32_t index = 0) { pushConstants(pipelineLayout, PipelineBindPoint::eCompute, data, index); }

    void pushComputeConstants(const void* data, uint32_t index = 0) { pushConstants(getCurrentPipelineLayout(), PipelineBindPoint::eCompute, data, index); }

    void pushGraphicsConstants(PipelineLayout* pipelineLayout, const void* data, uint32_t index = 0) { pushConstants(pipelineLayout, PipelineBindPoint::eGraphics, data, index); }

    void pushGraphicsConstants(const void* data, uint32_t index = 0) { pushConstants(getCurrentPipelineLayout(), PipelineBindPoint::eGraphics, data, index); }

    // Bind a descriptor set
    // The descriptor set allows the GPU shader to read from various resources
    // Note: in order to obtain maximum performance, the user should avoid redundant calls to
    // bind descriptor set (and other API calls in general)
    virtual void bindDescriptorSet(DescriptorSet* descriptorSet,
                                   PipelineBindPoint pipelineBindPoint,
                                   PipelineLayout* pipelineLayout,
                                   uint32_t numDynamicOffsets = 0,
                                   const uint32_t* dynamicOffsets = nullptr) = 0;

    void bindDescriptorSet(DescriptorSet* descriptorSet, PipelineBindPoint pipelineBindPoint, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, pipelineBindPoint, getCurrentPipelineLayout(), numDynamicOffsets, dynamicOffsets);
    }

    void bindGraphicsDescriptorSet(DescriptorSet* descriptorSet, PipelineLayout* pipelineLayout, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, PipelineBindPoint::eGraphics, pipelineLayout, numDynamicOffsets, dynamicOffsets);
    }

    void bindGraphicsDescriptorSet(DescriptorSet* descriptorSet, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, PipelineBindPoint::eGraphics, getCurrentPipelineLayout(), numDynamicOffsets, dynamicOffsets);
    }

    void bindComputeDescriptorSet(DescriptorSet* descriptorSet, PipelineLayout* pipelineLayout, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, PipelineBindPoint::eCompute, pipelineLayout, numDynamicOffsets, dynamicOffsets);
    }

    void bindComputeDescriptorSet(DescriptorSet* descriptorSet, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, PipelineBindPoint::eCompute, getCurrentPipelineLayout(), numDynamicOffsets, dynamicOffsets);
    }

    void bindRayTracingDescriptorSet(DescriptorSet* descriptorSet, PipelineLayout* pipelineLayout, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, PipelineBindPoint::eRayTracing, pipelineLayout, numDynamicOffsets, dynamicOffsets);
    }

    void bindRayTracingDescriptorSet(DescriptorSet* descriptorSet, uint32_t numDynamicOffsets = 0, const uint32_t* dynamicOffsets = nullptr)
    {
        bindDescriptorSet(descriptorSet, PipelineBindPoint::eRayTracing, getCurrentPipelineLayout(), numDynamicOffsets, dynamicOffsets);
    }

    // Clear a buffer
    // Parameters:
    // - value (optional): the clear value
    virtual void clear(Buffer* dst, uint32_t value = 0) = 0;
    // Clears a texture
    // Parameters:
    // - value (optional): the clear color value
    virtual void clear(Texture* dst, Color value = Color()) = 0;

    // Resets the state.
    // Note: this API call is mainly useful when integrating 3rd party libraries
    // that modify the command list state.  In most cases the user should not need
    // to call this API.  Currently, this function is only implemented in the d3d12 backend.
    // The d3d12 backend implementation currently clears the commandlist state and sets the
    // default descriptor heap.
    virtual void resetState() {};

    // Copy data from a source buffer to destination buffer.
    // QHWI also provides an upload manager to facilitate buffer uploads.
    // The upload manager is leveraged internally via the updateBuffer and updateTexture APIs.
    // Parameters:
    // - dst: the destination buffer
    // - src: the source buffer
    // - region: the copy region
    virtual void copyBuffer(Buffer* dst, Buffer* src, std::span<const CopyBufferRegion> regions) = 0;

    void copyBuffer(Buffer* dst, Buffer* src, const CopyBufferRegion& region = {}) { copyBuffer(dst, src, { &region, 1 }); }

    // Copy data from a source buffer to a destination texture
    // Parameters:
    // - dst: the destination buffer
    // - src: the source buffer
    // - region: the source offset and destination region
    virtual void copyBufferToTexture(Texture* dst, Buffer* src, std::span<const CopyBufferToTextureRegion> regions) = 0;

    void copyBufferToTexture(Texture* dst, Buffer* src, const CopyBufferToTextureRegion& region = {}) { copyBufferToTexture(dst, src, { &region, 1 }); }

    // Copy from a source texture to a destination texture
    // Parameters:
    // - dst: the destination texture
    // - src: the source texture
    // - region: the source and destination region
    virtual void copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion> regions) = 0;

    void copyTexture(Texture* dst, Texture* src, const CopyTextureRegion& region = {}) { copyTexture(dst, src, { &region, 1 }); }

    // Copy from a source texture to a destination buffer
    // Paramters:
    // - dst: the destination buffer
    // - src: the source texture
    // - region: the source region and destination offset
    virtual void copyTextureToBuffer(Buffer* dst, Texture* src, std::span<const CopyTextureToBufferRegion> regions) = 0;

    virtual void copyTextureToBuffer(Buffer* dst, Texture* src, const CopyTextureToBufferRegion& region = {}) { copyTextureToBuffer(dst, src, { &region, 1 }); }

    virtual void updateBuffer(Buffer* dst, std::span<const UpdateBufferRegion> regions);

    // Uploads data to a GPU buffer.
    // This internally uses an upload manager, and provides a simpler mechanism for the user to record uploads.
    // The upload manager maintains a pool of staging buffers.  It will first upload the CPU data to a staging buffer,
    // then record a command to copy the data from the staging buffer to the destination buffer.
    // Parameters:
    // - dst: The destination GPU buffer
    // - dstOffset: The destination offset
    // - srcData: The source data
    // - srcSize: the size of the source data (in bytes)
    void updateBuffer(Buffer* dst, uint32_t dstOffset, const void* srcData, size_t srcSize)
    {
        UpdateBufferRegion r{ dstOffset, srcData, srcSize };
        updateBuffer(dst, { &r, 1 });
    }

    // Batch upload data to a GPU texture
    virtual void updateTexture(Texture* dst, std::span<const UpdateTextureRegion> regions) = 0;

    // Uploads data to a GPU texture.
    // This internally uses an upload manager, and provides a simpler mechanism for the user to record uploads.
    // The upload manager maintains a pool of staging buffers.  It will first upload the CPU data to a staging buffer,
    // then record a command to copy the data from the staging buffer to the destination texture.
    // Parameters:
    // - dst: The destination GPU texture
    // - arraySlice: The destination array slice
    // - mipLevel: The destination mip level
    // - srcData: The source data
    // - srcRowPitch: The bytes per row in the source data
    // - srcDepthPitch: The size (in bytes) of one depth slice of source data
    void updateTexture(Texture* dst, uint32_t arraySlice, uint32_t mipLevel, const void* srcData, size_t srcRowPitch, size_t srcDepthPitch = 0)
    {
        UpdateTextureRegion r{ arraySlice, mipLevel, srcData, srcRowPitch, srcDepthPitch };
        updateTexture(dst, { &r, 1 });
    }

    // Bind a compute pipeline
    virtual void bindComputePipeline(ComputePipeline* pipeline) = 0;
    // Bind a graphics pipeline
    virtual void bindGraphicsPipeline(GraphicsPipeline* pipeline) = 0;
    // Bind a ray tracing pipeline
    virtual void bindRayTracingPipeline(RayTracingPipeline* pipeline) = 0;

    // Sets the current viewport
    virtual void setViewport(const Viewport& viewport) { setViewports(1, &viewport); }

    // Sets the current viewports
    virtual void setViewports(uint32_t numViewports, const Viewport* viewports) = 0;

    // Sets the scissor rectangle
    void setScissor(const ScissorRect& scissorRect) { setScissors(1, &scissorRect); }

    // Sets the scissor rectangles
    virtual void setScissors(uint32_t numScissorRects, const ScissorRect* scissorRects) = 0;

    ScissorRect calculateScissorForViewport(const Viewport& viewport)
    {
        uint32_t x = uint32_t(viewport.x);
        uint32_t y = uint32_t(viewport.y);
        return {
            x,                                           // Rounded down
            y,                                           // Rounded down
            uint32_t(ceil(viewport.x + viewport.w)) - x, // Add fractional part of x and round up
            uint32_t(ceil(viewport.y + viewport.h)) - y  // Add fractional part of y and round up
        };
    }

    // Set scissor large enough to cover an entire viewport
    void setScissorForViewport(const Viewport& vp) { setScissor(calculateScissorForViewport(vp)); }

    // Set viewports and corresponding scissors that are large enough to cover the respective viewports
    void setViewportsAndScissors(uint32_t numViewports, const Viewport* viewports)
    {
        setViewports(numViewports, viewports);
        ScissorRect r[MaxScissors];
        for (uint32_t j = 0; j < numViewports; j++)
        {
            r[j] = calculateScissorForViewport(viewports[j]);
        }
        setScissors(numViewports, r);
    }

    void setViewportAndScissor(const Viewport& viewport) { setViewportsAndScissors(1, &viewport); }

    // Set stencil reference value
    virtual void setStencilReference(uint32_t reference) = 0;

    // Add resource barriers
    // Note: the user cannot record barriers inside a render pass (see beginRenderPass).
    // Barriers record a memory dependency between previous and subsequent commands.
    // There are three types of hazards: RAW (read after write), WAW (write after write),
    // and WAR (write after read).  RAW: the previous command writes to the resource and
    // the next command reads from the resource.  The next command must wait for the previous command
    // to finish writes before reading. WAW: the previous and next command write to the resource.
    // The writes must occur in the correct order.  WAR: the previous command reads from the resource
    // and the next command writes to it.  The GPU has some logic to reschedule commands.  The GPU
    // must wait for the previous command to finish reading from the resource before the next command
    // writes to it.
    // There are three types of barriers: buffer, texture, and global barriers.
    virtual void addBarriers(uint32_t numBufferBarriers,
                             const BufferBarrier* bufferBarriers,
                             uint32_t numTextureBarriers,
                             const TextureBarrier* textureBarriers,
                             uint32_t numGlobalBarriers = 0,
                             const GlobalBarrier* globalBarriers = nullptr,
                             DependencyFlags flags = 0)
    {
    }

    void addBufferBarrier(const BufferBarrier& barrier, DependencyFlags flags = 0) { addBarriers(1, &barrier, 0, nullptr, flags); }

    void addTextureBarrier(const TextureBarrier& barrier, DependencyFlags flags = 0) { addBarriers(0, nullptr, 1, &barrier, flags); }

    void addGlobalBarrier(const GlobalBarrier& barrier, DependencyFlags flags = 0) { addBarriers(0, nullptr, 0, nullptr, 1, &barrier, flags); }

    // Note: this should only be used to debug synchronization issues
    void addDebugBarrier(DependencyFlags flags = 0)
    {
        addGlobalBarrier({ .srcStages = PipelineStage_AllCommands,
                           .srcAccess = AccessMode::AccessMode_MemoryReadWrite,
                           .dstStages = PipelineStage_AllCommands,
                           .dstAccess = AccessMode::AccessMode_MemoryReadWrite },
                         flags);
    }

    // Build bottom level acceleration structure
    virtual void buildBottomLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                                       Buffer* scratchBuffer,
                                                       BottomLevelDescriptor* bottomLevelDescriptors,
                                                       size_t descriptorCount,
                                                       void* scratchMemory,
                                                       AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                       AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) = 0;
    // Build top level acceleration structure
    virtual void buildTopLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                                    Buffer* instanceBuffer,
                                                    Buffer* scratchBuffer,
                                                    size_t instanceCount,
                                                    AccelerationStructure** blasArray,
                                                    size_t blasCount,
                                                    AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                    AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) = 0;

    // Begin timer query
    // Timer queries are useful for profiling the time spent executing work on the GPU.
    // Currently, QHWI supports profiling the GPU time of an entire pass: render, compute, transfer, or acceleration structure pass.
    // The main reason QHWI doesn't currently offer higher granularity is because on a tiled GPU architecture,
    // each draw or dispatch is done on a per-tile basis so the driver may not support measuring the total time spent across
    // all the tiles.
    virtual void beginTimerQuery(TimerQuery* timerQuery) = 0;
    // End timer query
    virtual void endTimerQuery(TimerQuery* timerQuery) = 0;
    // Resolve timer query data
    virtual void resolveTimerQueryData(TimerQuery* timerQuery) = 0;
    // Reset timer query pool
    virtual void resetTimerQueryPool(TimerQuery* timerQuery) = 0;
    virtual Device* device() = 0;

    struct GraphicsResourceState
    {
    public:
        void setVertexBuffer(uint32_t slot, Buffer* b);
        void setIndexBuffer(Buffer* b);
        void setIndirectBuffer(Buffer* b);
        void setDescSet(uint32_t slot, DescriptorSet* ds);

        static constexpr uint32_t MaxVertexBuffers = 8;
        Buffer* vertexBuffers[MaxVertexBuffers] = {};
        Buffer* indexBuffer = nullptr;
        Buffer* indirectBuffer = nullptr;
        DescriptorSet* descSets[MaxDescriptorSetsPerPipeline] = {};
    };

    struct ComputeResourceState
    {
    public:
        void setIndirectBuffer(Buffer* b);
        void setDescSet(uint32_t slot, DescriptorSet* ds);
        Buffer* indirectBuffer = nullptr;
        DescriptorSet* descSets[MaxDescriptorSetsPerPipeline] = {};
    };

    struct RayTracingResourceState
    {
    public:
        void setIndirectBuffer(Buffer* b);
        void setDescSet(uint32_t slot, DescriptorSet* ds);
        Buffer* indirectBuffer = nullptr;
        DescriptorSet* descSets[MaxDescriptorSetsPerPipeline] = {};
    };

    const BufferState* getState(Buffer* buffer) const
    {
        auto it = m_bufferState.find(buffer);
        return (it == m_bufferState.end()) ? nullptr : &it->second.currentState;
    }

    const TextureState* getState(Texture* texture, Subresource subresource = {})
    {
        const auto it = m_textureState.find(texture);

        return (it == m_textureState.end()) ? nullptr : &it->second.getState(subresource).currentState;
    }

    // TODO: add buffer subregion
    virtual void setState(Buffer* buffer, BufferState nextState);
    virtual void setState(Texture* texture, TextureState nextState, SubresourceRange subrresourceRange = EntireResource);
    void notifyState(Buffer* buffer, BufferState nextState);
    void notifyState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange = EntireResource);

    // Notify QHWI when inside a renderpass.  In most cases, the user should not have to call this API.
    void notifyInRenderPass(bool v) { m_inRenderPass = v; }

    // Notify QHWI when inside a transfer pass.  In most cases, the user should not have to call this API.
    void notifyInTransferPass(bool v) { m_inTransferPass = v; }

    // Notify QHWI when inside a compute pass.  In most cases, the user should not have to call this API.
    void notifyInComputePass(bool v) { m_inComputePass = v; }

    // Notify QHWI when inside an acceleration structure pass.  In most cases, the user should not have to call this API.
    void notifyInAccelerationStructurePass(bool v) { m_inAccelerationStructurePass = v; }

protected:
    void onBegin();

    void onBeginRenderPass()
    {
        QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot start a render pass inside another render pass.");
        QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot start a render pass inside a compute pass.");
        QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot start a render pass inside a transfer pass.");
        QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot start a render pass inside an acceleration structure pass.");

        m_inRenderPass = true;
    }

    void onEndRenderPass()
    {
        QHWI_ASSERT_MSG(m_inRenderPass, "A render pass has not started.");
        m_inRenderPass = false;
    }

    void onBeginComputePass()
    {
        QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot start a compute pass inside a render pass.");
        QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot start a compute pass inside another compute pass.");
        QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot start a compute pass inside a transfer pass.");
        QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot start a compute pass inside an acceleration structure pass.");

        m_inComputePass = true;
    }

    void onEndComputePass()
    {
        QHWI_ASSERT_MSG(m_inComputePass, "A compute pass has not started.");
        m_inComputePass = false;
    }

    void onBeginTransferPass()
    {
        QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot start a transfer pass inside a render pass.");
        QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot start a transfer pass inside a compute pass.");
        QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot start a transfer pass inside another transfer pass.");
        QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot start a transfer pass inside an acceleration structure pass.");

        m_inTransferPass = true;
    }

    void onEndTransferPass()
    {
        QHWI_ASSERT_MSG(m_inTransferPass, "A transfer pass has not started.");
        m_inTransferPass = false;
    }

    void onBeginAccelerationStructurePass()
    {
        QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot start an acceleration structure pass inside a render pass.");
        QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot start an acceleration structure pass inside a compute pass.");
        QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot start an acceleration structure pass inside a transfer pass.");
        QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot start a compute pass inside another acceleration structure pass.");

        m_inAccelerationStructurePass = true;
    }

    void onEndAccelerationStructurePass()
    {
        QHWI_ASSERT_MSG(m_inAccelerationStructurePass, "An acceleration structure pass has not started.");
        m_inAccelerationStructurePass = false;
    }

    void markResourceAsUsed(qre::RefCounterDelete* object);
    void markDescSetResourcesAsUsed(DescriptorSet* descSet);
    PipelineStageMask toPipelineStageMask(ShaderStageMask m0);
    AccessModeMask toAccessModeMask(DescriptorType type);
    ImageLayout toImageLayout(DescriptorType type);
    virtual void setGraphicsState(bool enableIndexBuffer = true, bool enableIndirectBuffer = true);
    virtual void setComputeState();
    virtual void setRayTracingState();
    virtual void setNextState(const GraphicsResourceState* rs = nullptr, bool clear = false, bool enableIndexBuffer = true, bool enableIndirectBuffer = true);
    virtual void setNextState(const ComputeResourceState* rs = nullptr, bool clear = false);
    virtual void setNextState(const RayTracingResourceState* rs = nullptr, bool clear = false);
    virtual void setNextState(const DescriptorSet* ds, bool clear);
    virtual void setState(const DescriptorSet* ds);

    bool m_stateChanged = false;
    bool m_inRenderPass = false;
    bool m_inTransferPass = false;
    bool m_inComputePass = false;
    bool m_inAccelerationStructurePass = false;
    bool m_hasPrevSubpass = false;
    bool m_hasNextSubpass = false;
    TimerQuery* m_scheduledTimerQuery = nullptr;

    PipelineLayout* m_currentPipelineLayout = nullptr;
    Pipeline* m_currentPipeline = nullptr;
    static constexpr uint32_t ResourcesUsedByHw_InitialSize = 1024;
    std::unordered_set<qre::RefCounterDelete*> m_resourcesUsedByHw;
    uint32_t m_usedResourceCount = 0;

    TimerQuery* m_referencedTimerQueries[MaxTimerQueries];
    uint32_t m_referencedTimerQueriesIndex = 0;

    GraphicsResourceState m_currentGraphicsState, m_nextGraphicsState;
    ComputeResourceState m_currentComputeState, m_nextComputeState;
    RayTracingResourceState m_currentRayTracingState, m_nextRayTracingState;

    struct BufferState1
    {
        BufferState nextState, currentState;
    };

    struct TextureStateTracker
    {
        TextureState nextState, currentState;

        bool operator==(const TextureStateTracker& v) const { return this->currentState == v.currentState && this->nextState == v.nextState; }
    };

    class TextureSubresourceStateTrackers final
    {
    public:
        TextureSubresourceStateTrackers() = delete;
        TextureSubresourceStateTrackers(const Texture* texture);

        void setCurrentState(TextureState state, SubresourceRange subresourceRange = EntireResource);
        void setNextState(TextureState state, SubresourceRange subresourceRange = EntireResource);

        const TextureStateTracker& getState(Subresource subresource = {}) const;

        bool isUnifiedState() const { return m_isUnifiedState; }

    private:
        std::vector<TextureStateTracker> m_subresourceStates;
        const Texture* m_texture;
        bool m_isUnifiedState = true; // true if all subresources share the same state
    };

    struct TextureAttachmentFinalStateData
    {
        TextureAttachmentData textureData;
        TextureState finalState;
    };

    BufferState1& bufferState(Buffer* buffer);
    TextureSubresourceStateTrackers& textureSubresourceStateTrackers(Texture* texture);
    void setNextState(Texture* texture, TextureState nextState, SubresourceRange = EntireResource);
    void setCurrentState(Texture* texture, TextureState currentState, SubresourceRange = EntireResource);
    bool hasPrevSubpass(std::span<const ColorAttachmentDescription> cbDescs);
    bool hasNextSubpass(std::span<const ColorAttachmentDescription> cbDescs);
    std::unordered_map<Buffer*, BufferState1> m_bufferState;
    std::unordered_map<Texture*, TextureSubresourceStateTrackers> m_textureState;
};

bool operator==(const CommandList::GraphicsResourceState& lhs, const CommandList::GraphicsResourceState& rhs);
bool operator==(const CommandList::ComputeResourceState& lhs, const CommandList::ComputeResourceState& rhs);
using CommandListDesc = CommandList::Description;

struct ScopedMarker
{
    // This struct allows the user to record event markers
    // in the command list.  This is useful when using
    // debugging tools such as PIX to annotate the trace.

    ScopedMarker(CommandList* cmdList, const char* name)
        : cmdList(cmdList)
    {
        cmdList->beginMarker(name);
    }

    ~ScopedMarker()
    {
        if (cmdList != nullptr)
        {
            cmdList->endMarker();
        }
    }

    ScopedMarker(ScopedMarker&& other)
        : cmdList(std::exchange(other.cmdList, nullptr))
    {
    }

    ScopedMarker& operator=(ScopedMarker&&) = delete;

    CommandList* cmdList = nullptr;
};

inline ScopedMarker CommandList::scopedMarker(const char* name)
{
    return ScopedMarker(this, name);
}

class Fence;

class CommandQueue : public qre::RefCounterDelete
{
    // Provides support for GPU command queues
    // The GPU may have several command queues.  Each queue
    // may have different capabilities such as graphics, compute, and
    // transfer.  For optimal performance, consider using multiple queues
    // in parallel.
public:
    CommandQueue();
    virtual ~CommandQueue();

    struct Description
    {
        // Set the requested capabilities
        constexpr Description& setCapabilities(CommandQueueCapabilities v)
        {
            capabilities = v;
            return *this;
        }

        CommandQueueCapabilities capabilities = CommandQueueCapability_Graphics | CommandQueueCapability_Compute | CommandQueueCapability_Transfer;
    };

    uint64_t submit(Fence* fence) { return submit(0, nullptr, fence, nullptr); }

    uint64_t executeCommandLists(uint32_t numCommandLists, CommandList** commandLists, Fence* fence = nullptr, ext::Swapchain* swapchain = nullptr)
    {
        return submit(numCommandLists, commandLists, fence, swapchain);
    }

    uint64_t executeCommandList(CommandList* commandList, Fence* fence = nullptr, ext::Swapchain* swapchain = nullptr) { return submit(1, &commandList, fence, swapchain); }

    // Submit work
    // Parameters:
    // - numCommandLists: the number of command lists to execute (can be zero)
    // - commandLists: the command lists (can be nullptr if numCommandLists is zero)
    // - fence (optional): a fence to signal
    // - swapchain (optional): the current swapchain
    // Return value:
    // - the submission ID
    virtual uint64_t submit(uint32_t numCommandLists, CommandList** commandLists, Fence* fence = nullptr, ext::Swapchain* swapchain = nullptr) = 0;

    // Wait for the GPU to be idle (finish executing all commands in flight)
    virtual void waitForIdle() = 0;

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    virtual void* getNativeQueue() = 0;

    // Get the ID of the most recent comamnd list submission
    constexpr uint64_t getLastSubmittedID() { return m_lastSubmittedID; };

    // Get the ID of the most recent command list that completed
    constexpr uint64_t getLastCompletedID()
    {
        updateLastCompletedID();
        return m_lastCompletedID;
    };

    // Release resources that are pending for deletion, that are no longer in use by the GPU
    void reclaimResources();

protected:
    virtual void updateLastCompletedID() = 0;

    uint64_t m_lastSubmittedID = 0;
    uint64_t m_lastCompletedID = 0;

    struct CommandListTracker
    {
        CommandList* commandList;
        uint64_t submissionId;
    };

    // TODO: Use a qre data structure
    std::queue<CommandListTracker> m_commandListsInFlight;

    void trackCommandLists(uint32_t numCommandLists, CommandList** commandLists, uint64_t submissionID);
};

using CommandQueueDesc = CommandQueue::Description;

class Fence : public qre::RefCounterDelete
{
    // Provide GPU - CPU synchronization
    // Allos the user to wait on CPU side for GPU work to complete
    // or query the current status of a fence
public:
    Fence()
        : RefCounterDelete()
    {
    }

    virtual ~Fence() {}

    // Wait for the fence to be signaled
    virtual Result wait() = 0;
    // Get the current state
    virtual FenceState getCurrentState() = 0;
    // Reset the fence to the initial state
    // Note: the user can set the initial state as signaled or unsignaled via the createFence API
    virtual void reset() = 0;
};

class QueryHeap
{
    // Stores timer query data
public:
    virtual ~QueryHeap() {}

    virtual void init(Device* device, const char* name = "") = 0;

    int32_t allocate() { return freeList.Alloc(); };

    void free(int32_t index) { freeList.Free(index); };

protected:
    qre::FreeList freeList;
};

class TimerQuery : public qre::RefCounterDelete
{
    // Provides support for GPU timer queries for profiling GPU performance
public:
    TimerQuery(QueryHeap* pQueryHeap, uint32_t beginIndex, uint32_t endIndex)
        : RefCounterDelete()
        , beginQueryIndex(beginIndex)
        , endQueryIndex(endIndex)
        , timerState(TimerState::eReset)
        , m_queryHeap(pQueryHeap)
    {
    }

    virtual ~TimerQuery() { m_queryHeap->free(beginQueryIndex >> 1); }

    uint32_t beginQueryIndex;
    uint32_t endQueryIndex;
    TimerState timerState;

private:
    QueryHeap* m_queryHeap;
};

class Context;

class Device : public qre::RefCounterDelete
{
    // Provides an interface to a GPU device.
    // On systems with both a discrete and integrated GPU, QHWI will automatically choose the discrete GPU.
    // TODO: expose more control to the user to query the available GPUs and choose the GPU
public:
#if defined(__clang__) || defined(__GNUC__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996) // ignore [[deprecated]] attribute
#endif
    struct Description
    {
        // Set debug name
        constexpr Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        // Emulate UMA (unified memory architecture)
        // This is useful for development, and emulating a UMA device with shared CPU/GPU memory
        constexpr Description& setEmulateUma(bool v)
        {
            emulateUma = v;
            return *this;
        }

        constexpr Description& SetEnableMeshShaders(bool v)
        {
            enableMeshShaders = v;
            return *this;
        }

        constexpr Description& SetEnableRayQuery(bool v)
        {
            enableRayQuery = v;
            return *this;
        }

        constexpr Description& SetEnableD3dEnhancedBarriers(bool v)
        {
            enableD3dEnhancedBarriers = v;
            return *this;
        }

        constexpr Description& SetEnableVkDynamicRendering(bool v)
        {
            enableVkDynamicRendering = v;
            return *this;
        }

        constexpr Description& SetEnableVkQcomTileMemoryHeap(bool v)
        {
            enableVkQcomTileMemoryHeap = v;
            return *this;
        }

        constexpr Description& SetEnableOpacityMicroMaps(bool v)
        {
            enableOpacityMicroMaps = v;
            return *this;
        }

        constexpr Description& SetEnableCustomResolve(bool v)
        {
            enableCustomResolve = v;
            return *this;
        }

        // Set the maximum number of srv (shader resource view) descriptors
        [[deprecated]] constexpr Description& setMaxSrvDescriptors(uint32_t v)
        {
            maxSrvDescriptors = (v > MaxSrvDescriptors) ? MaxSrvDescriptors : v;
            return *this;
        }

        // Set the maximum number of cbv (constant buffer view) descriptors
        [[deprecated]] constexpr Description& setMaxCbvDescriptors(uint32_t v)
        {
            maxCbvDescriptors = (v > MaxCbvDescriptors) ? MaxCbvDescriptors : v;
            return *this;
        }

        // Set the maximum number of uav (unordered access view) descriptors
        [[deprecated]] constexpr Description& setMaxUavDescriptors(uint32_t v)
        {
            maxUavDescriptors = (v > MaxUavDescriptors) ? MaxUavDescriptors : v;
            return *this;
        }

        // Set the maximum number of sampler descriptors
        [[deprecated]] constexpr Description& setMaxSamplerDescriptors(uint32_t v)
        {
            maxSamplerDescriptors = (v > MaxSamplerDescriptors) ? MaxSamplerDescriptors : v;
            return *this;
        }

        // Set the maximum number of dsv (depth/stencil view) descriptors
        [[deprecated]] constexpr Description& setMaxDsvDescriptors(uint32_t v)
        {
            maxDsvDescriptors = (v > MaxDsvDescriptors) ? MaxDsvDescriptors : v;
            return *this;
        }

        const char* name = nullptr;

        [[deprecated]] uint32_t maxCbvDescriptors = MaxCbvDescriptors;
        [[deprecated]] uint32_t maxSrvDescriptors = MaxSrvDescriptors;
        [[deprecated]] uint32_t maxUavDescriptors = MaxUavDescriptors;
        [[deprecated]] uint32_t maxSamplerDescriptors = MaxSamplerDescriptors;
        [[deprecated]] uint32_t maxDsvDescriptors = MaxDsvDescriptors;

        bool emulateUma = false;
        bool enableMeshShaders = true;
        bool enableRayQuery = true;
        bool enableD3dEnhancedBarriers = true;
        bool enableVkDynamicRendering = true;
        bool enableVkQcomTileMemoryHeap = false;
        bool enableOpacityMicroMaps = false;
        bool enableCustomResolve = false;
    };
#if defined(__clang__) || defined(__GNUC__)
#    pragma clang diagnostic pop
#elif defined(_MSC_VER)
#    pragma warning(pop)
#endif
    Device(Context* ctx)
        : RefCounterDelete()
        , m_context(ctx)
    {
    }

    virtual ~Device() {}

    virtual void* getNativeDevice() = 0;

    // Create a pipeline layout
    virtual qre::RefCountPtr<PipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) = 0;

    // Create a compute pipeline
    virtual qre::RefCountPtr<ComputePipeline> createComputePipeline(const ComputePipelineDesc& desc) = 0;

    // Create a graphics pipeline
    virtual qre::RefCountPtr<GraphicsPipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    // Create a ray tracing pipeline
    virtual qre::RefCountPtr<RayTracingPipeline> createRayTracingPipeline(const RayTracingPipelineDesc& desc) = 0;

    // Create a buffer
    virtual qre::RefCountPtr<Buffer> createBuffer(const BufferDesc& desc) = 0;
    // Create a texture
    virtual qre::RefCountPtr<Texture> createTexture(const TextureDesc& desc) = 0;
    // Create a texture from an existing handle
    // This will wrap an existing texture created externally as a QHWI texture object
    virtual qre::RefCountPtr<Texture> createTextureFromHandle(const TextureDesc& desc, Texture::Handle handle, Texture::Handle handle2 = nullptr) = 0;

    // Create a sampler
    virtual qre::RefCountPtr<Sampler> createSampler(const SamplerDesc& desc) = 0;

    // Create a GPU shader from bytecode
    // Parameters:
    // - byteCode: the platform-specific shader bytecode (DXIL, SPIRV, or AIR)
    // - length: the size of the data (in bytes)
    virtual qre::RefCountPtr<Shader> createShader(const ShaderDesc& desc, const void* byteCode, size_t length) = 0;

    // Create a descriptor set layout
    virtual qre::RefCountPtr<DescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;
    // Create a descriptor set
    virtual qre::RefCountPtr<DescriptorSet> createDescriptorSet(const DescriptorSetDesc& desc) = 0;
    // Create a descriptor set and corresponding layout
    // The layout is inferred based on the descriptor set description
    void createDescriptorSetAndLayout(const DescriptorSetDesc& descriptorSetDesc,
                                      ShaderStageMask shaderStages,
                                      uint32_t registerSpace,
                                      qre::RefCountPtr<DescriptorSetLayout>& descriptorSetLayout,
                                      qre::RefCountPtr<DescriptorSet>& descriptorSet,
                                      const BindingSetOffsets& bindingSetOffsets = DefaultBindingSetOffsets);

    // Create a command list
    virtual qre::RefCountPtr<CommandList> createCommandList(CommandQueue* commandQueue, const CommandListDesc& desc = {}) = 0;

    // Create a command queue
    virtual qre::RefCountPtr<CommandQueue> createCommandQueue(const CommandQueueDesc& desc = {}) = 0;

    virtual qre::RefCountPtr<AccelerationStructure> createAccelerationStructure(const AccelerationStructureDesc& desc) = 0;

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    virtual qre::RefCountPtr<compressedstorage::File> createStorageFile() { return nullptr; }
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

    qre::RefCountPtr<ComputePipeline> create(const ComputePipelineDesc& desc) { return createComputePipeline(desc); }

    qre::RefCountPtr<GraphicsPipeline> create(const GraphicsPipelineDesc& desc) { return createGraphicsPipeline(desc); }

    qre::RefCountPtr<Buffer> create(const BufferDesc& desc) { return createBuffer(desc); }

    qre::RefCountPtr<Texture> create(const TextureDesc& desc) { return createTexture(desc); }

    qre::RefCountPtr<Texture> create(const TextureDesc& desc, Texture::Handle handle) { return createTextureFromHandle(desc, handle); }

    qre::RefCountPtr<Sampler> create(const SamplerDesc& desc) { return createSampler(desc); }

    qre::RefCountPtr<Shader> create(const ShaderDesc& desc, const void* byteCode, size_t length) { return createShader(desc, byteCode, length); }

    qre::RefCountPtr<DescriptorSetLayout> create(DescriptorSetLayoutDesc& desc) { return createDescriptorSetLayout(desc); }

    qre::RefCountPtr<DescriptorSet> create(const DescriptorSetDesc& desc) { return createDescriptorSet(desc); }

    qre::RefCountPtr<CommandList> create(CommandQueue* commandQueue, const CommandListDesc& desc = {}) { return createCommandList(commandQueue, desc); }

    qre::RefCountPtr<CommandQueue> create(const CommandQueueDesc& desc = {}) { return createCommandQueue(desc); }

    qre::RefCountPtr<AccelerationStructure> create(const AccelerationStructureDesc& desc) { return createAccelerationStructure(desc); }

    qre::RefCountPtr<PipelineLayout> create(const PipelineLayoutDesc& desc) { return createPipelineLayout(desc); }

    struct FeaturesInfo
    {
        bool supportsRayQuery() const { return (mask & Feature_RayQuery); }

        bool supportsMeshShaders() const { return (mask & Feature_MeshShaders); }

        bool supportsRayPipelines() const { return (mask & Feature_RayPipelines); }

        bool supportsRayTracing() const { return supportsRayQuery() || supportsRayPipelines(); }

        bool supportsConservativeRasterization() const { return (mask & Feature_ConservativeRasterization); }

        bool supportsShaderFloat16() const { return (mask & Feature_ShaderFloat16); }

        bool supportsTextureCompressionAstc() const { return (mask & Feature_TextureCompressionAstc); }

        bool supportsTextureCompressionBc() const { return (mask & Feature_TextureCompressionBc); }

        bool supportsTextureCompressionEtc() const { return (mask & Feature_TextureCompressionEtc); }

        bool supportsHdrMetadata() const { return (mask & Feature_HdrMetadata); }

        bool supportsBarycentric() const { return (mask & Feature_Barycentric); }

        bool supportsStorageQueue() const { return (mask & Feature_StorageQueue); }

        bool supportsDispatchRaysIndirect() const { return (mask & Feature_RtTraceRaysIndirect); }

        bool supportsRayTracingInvocationReorder() const { return (mask & Feature_RtInvocationReorder); }

        bool supportsOpacityMicromaps() const { return (mask & Feature_OpacityMicromaps); }

        bool supportsCustomResolve() const { return (mask & Feature_CustomResolve); }

        FeaturesMask mask = 0;
    };

    const FeaturesInfo& getFeatures() const { return m_featuresInfo; }

    bool supportsFeature(Feature feature) const { return (m_featuresInfo.mask & feature); }

    bool supportsRayQuery() const { return m_featuresInfo.supportsRayQuery(); }

    bool supportsMeshShaders() const { return m_featuresInfo.supportsMeshShaders(); }

    bool supportsRayPipelines() const { return m_featuresInfo.supportsRayPipelines(); }

    bool supportsRayTracingInvocationReorder() const { return m_featuresInfo.supportsRayTracingInvocationReorder(); }

    bool supportsConservativeRasterization() const { return m_featuresInfo.supportsConservativeRasterization(); };

    bool supportsRayTracing() const { return m_featuresInfo.supportsRayTracing(); }

    bool supportsHdrMetadata() const { return m_featuresInfo.supportsHdrMetadata(); }

    bool supportsStorageQueue() const { return m_featuresInfo.supportsStorageQueue(); };

    struct D3dInternalOptions
    {
        void setGenerateUpscalerHeader(bool input) { enableUpscalerHeaderGeneration = input; }

        bool getGenerateUpscalerHeader() const { return enableUpscalerHeaderGeneration; }

        bool enableUpscalerHeaderGeneration = false;
    };

    const D3dInternalOptions& getD3dInternalOptions() const { return m_d3dInternalOptions; }

    void setGenerateUpscalerHeader(bool input) { m_d3dInternalOptions.enableUpscalerHeaderGeneration = input; }

    bool getGenerateUpscalerHeader() const { return m_d3dInternalOptions.enableUpscalerHeaderGeneration; }

    virtual bool supportsImageProcessingQCOM() const = 0;
    virtual bool supportsMultiView() const = 0;

    virtual bool supportsMultiViewPerViewViewportsQCOM() const { return false; }

    virtual bool supportsMultiViewPerViewRenderAreasQCOM() const { return false; };

    virtual bool supportsQueuePerfHintQCOM() const { return false; }

    virtual bool supportsElapsedTimerQueryQCOM() const { return false; }

    virtual bool supportsOpticalFlowNV() const { return false; }

    virtual bool supportsBarycentric() const { return m_featuresInfo.supportsBarycentric(); }

    virtual bool supportsDispatchRaysIndirect() const { return m_featuresInfo.supportsDispatchRaysIndirect(); }

    virtual bool supportsOpacityMicromaps() const { return m_featuresInfo.supportsOpacityMicromaps(); }

    virtual bool supportsCustomResolve() const { return false; }

    struct LimitsInfo
    {
        float maxAnisotropy = 0.0f;
        uint32_t maxTextureDimension1D = 0;
        uint32_t maxTextureDimension2D = 0;
        uint32_t maxTextureDimension3D = 0;
        uint32_t maxTextureDimensionCube = 0;
        uint32_t maxRenderTargets = 0;
        uint32_t minWaveSize = 0;
        uint32_t maxWaveSize = 0;
    };

    const LimitsInfo getLimits() const { return m_limitsInfo; }

    // Update descriptor set
    // Parameters:
    // - descriptorSet: the descriptor set to update
    // - writes: the descriptor set writes
    // - numWrites: the number of writes
    virtual void updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSetWrite* writes, uint32_t numWrites = 1);

    void updateDescriptorSet(DescriptorSet* descriptorSet,
                             DescriptorTextureInfo* textureInfos,
                             DescriptorType type,
                             uint32_t shaderRegister = 0,
                             uint32_t arrayIndex = 0,
                             uint32_t count = 0)
    {
        if (count == 0)
        {
            return;
        }
        QHWI_ASSERT(textureInfos);
        DescriptorSetWrite descSetWrite = {
            .shaderRegister = shaderRegister,
            .arrayIndex = arrayIndex,
            .bufferInfos = nullptr,
            .textureInfos = textureInfos,
            .samplerInfos = nullptr,
            .accelStructInfos = nullptr,
            .type = type,
            .count = count,
        };
        updateDescriptorSet(descriptorSet, &descSetWrite);
    }

    void updateDescriptorSet(DescriptorSet* descriptorSet,
                             DescriptorBufferInfo* bufferInfos,
                             DescriptorType type,
                             uint32_t shaderRegister = 0,
                             uint32_t arrayIndex = 0,
                             uint32_t count = 0)
    {
        if (count == 0)
        {
            return;
        }
        QHWI_ASSERT(bufferInfos);
        DescriptorSetWrite descSetWrite = { .shaderRegister = shaderRegister,
                                            .arrayIndex = arrayIndex,
                                            .bufferInfos = bufferInfos,
                                            .textureInfos = nullptr,
                                            .samplerInfos = nullptr,
                                            .accelStructInfos = nullptr,
                                            .type = type,
                                            .count = count };
        updateDescriptorSet(descriptorSet, &descSetWrite);
    }

    void updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSamplerInfo* samplerInfos, uint32_t shaderRegister = 0, uint32_t arrayIndex = 0, uint32_t count = 0)
    {
        if (count == 0)
        {
            return;
        }
        QHWI_ASSERT(samplerInfos);
        DescriptorSetWrite descSetWrite = { .shaderRegister = shaderRegister,
                                            .arrayIndex = arrayIndex,
                                            .bufferInfos = nullptr,
                                            .textureInfos = nullptr,
                                            .samplerInfos = samplerInfos,
                                            .accelStructInfos = nullptr,
                                            .type = DescriptorType::eSampler,
                                            .count = count };
        updateDescriptorSet(descriptorSet, &descSetWrite);
    }

    void updateDescriptorSet(DescriptorSet* descriptorSet,
                             DescriptorAccelerationStructureInfo* accelStructInfos,
                             uint32_t shaderRegister = 0,
                             uint32_t arrayIndex = 0,
                             uint32_t count = 0)
    {
        if (count == 0)
        {
            return;
        }
        QHWI_ASSERT(accelStructInfos);
        DescriptorSetWrite descSetWrite = { .shaderRegister = shaderRegister,
                                            .arrayIndex = arrayIndex,
                                            .bufferInfos = nullptr,
                                            .textureInfos = nullptr,
                                            .samplerInfos = nullptr,
                                            .accelStructInfos = accelStructInfos,
                                            .type = DescriptorType::eAccelerationStructure,
                                            .count = count };
        updateDescriptorSet(descriptorSet, &descSetWrite);
    }

    // Set the window surface
    virtual void setWindowSurface(const ext::WindowSurface& windowSurface) = 0;

    // Create a swapchain
    virtual ext::Swapchain* createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc) = 0;

    // Create a fence
    virtual qre::RefCountPtr<Fence> createFence(FenceState initialState = FenceState_Unsignaled, const char* name = "") = 0;

    // Returns true if the device is UMA (unified memory architecture), e.g. shared CPU / GPU memory
    virtual bool isUnifiedMemoryArchitecture() = 0;

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    // Query whether a format is supported
    virtual FormatSupportMask queryFormatSupport(Format format) const = 0;

    // Get required alignment (bytes) for constant buffer descriptor offsets
    virtual size_t getConstantBufferOffsetAlignment() const = 0;

    // Get required alignment (bytes) for storage buffer descriptor offsets
    virtual size_t getMinStorageBufferOffsetAlignment() const = 0;

    virtual size_t getBottomLevelScratchSize(size_t descriptorCount,
                                             AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                             AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) = 0;
    virtual AccelerationStructureBuildInfo getAccelerationStructureBuildInfo(const BottomLevelDescriptor* bottomLevelDescriptors,
                                                                             size_t count,
                                                                             void* scratchMemory,
                                                                             AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                                             AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) = 0;
    virtual qre::RefCountPtr<Buffer> createTopLevelInstanceBuffer(const TopLevelDescriptor* topLevelDescriptors, AccelerationStructure** blasArray, size_t count) = 0;
    virtual AccelerationStructureBuildInfo getAccelerationStructureBuildInfo(size_t instanceCount,
                                                                             AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                                             AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) = 0;

    RayTracingPipelineProperties getRayTracingPipelineProperties() const { return m_rayTracingPipelineProperties; };

    virtual ResultCode getRayTracingShaderGroupHandles(const RayTracingPipeline* pipeline, uint32_t startGroup, uint32_t groupCount, size_t dstSize, void* dst) const = 0;

    // Returns true if the timer query result is ready
    [[deprecated]] virtual bool isTimerQueryResultReady(TimerQuery* timerQuery) = 0;
    // Get the timer query result in seconds
    // Parameters:
    // - timerQuery: the timer query
    // - commandQueue: the command queue
    // - blockUntilReady: if true, wait until the timer query result is ready

    virtual bool getTimerQueryTimeInSec(TimerQuery* timerQuery, CommandQueue* commandQueue, bool blockUntilReady, float& timeInSec) = 0;

    constexpr Context* getContext() { return m_context; }

    // Get the current graphics backend
    virtual GraphicsBackend getGraphicsBackend() const = 0;

    virtual qre::RefCountPtr<TimerQuery> createTimerQuery() = 0;

    // Queries if a depth / stencil format is supported
    // Paramters:
    // - fmt: the depth / stencil format
    bool supportsDepthStencilFormat(Format fmt) const;

    constexpr UploadPool& uploadPool() { return m_uploadPool; }

    // Compact the upload pool
    // Parameters:
    // - threshold (optional): if the upload pool size is less than the threshold, don't compact
    void reclaimUploadPoolResources(size_t threshold = UploadPoolCompactThreshold) { m_uploadPool.compact(threshold); }

    virtual void logMemoryReport() {};
    virtual void saveMemoryAllocations(const char* fileName) {};

    void setDedicatedVideoMemory(size_t memory) { m_dedicatedVideoMemory = memory; }

    size_t getDedicatedVideoMemory() const { return m_dedicatedVideoMemory; }

    void setSharedSystemMemory(size_t memory) { m_sharedSystemMemory = memory; }

    size_t getSharedSystemMemory() const { return m_sharedSystemMemory; }

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    const compressedstorage::Factory* getStorageFactory() const { return m_storageFactory; }
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

protected:
    Context* m_context = nullptr;
    UploadPool m_uploadPool;
    void setFeature(Feature feature, bool supported);
    FeaturesInfo m_featuresInfo;
    D3dInternalOptions m_d3dInternalOptions;
    LimitsInfo m_limitsInfo;
    RayTracingPipelineProperties m_rayTracingPipelineProperties;
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    qre::RefCountPtr<compressedstorage::Factory> m_storageFactory;
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

    size_t m_dedicatedVideoMemory = 0;
    size_t m_sharedSystemMemory = 0;
};

using DeviceDesc = Device::Description;

class Context : public qre::RefCounterDelete
{
public:
    virtual ~Context() {}

    struct Description
    {
        // Set the requested graphics backend
        constexpr Description& setGraphicsBackend(GraphicsBackend v)
        {
            graphicsBackend = v;
            return *this;
        }

        // Enable / disable debugging
        constexpr Description& setDebug(bool v)
        {
            debug = v;
            return *this;
        }

        // Enable / disable GPU validation (significantly reduces performance)
        constexpr Description& setEnableGpuValidation(bool v)
        {
            enableGPUValidation = v;
            return *this;
        }

        GraphicsBackend graphicsBackend = GraphicsBackend::eAuto;
        bool debug = false;
        bool apiDump = false;
        // Enable GPU validation
        //  Note: this significantly reduces performance
        bool enableGPUValidation = false;
        bool enableD3dDebugMessageCallback = true;
        // Disable VK_QCOM_TILE_MEMORY_HEAP extension by default
        // When enabled and supported, QHWI Vulkan backend
        // will automatically query and allocate all tile memory available
        // from the tile memory heap.  Note: this can negatively
        // impact performance since it prevents the driver from using the
        // memory and requires the user to explicitly manage it.
        // Note, this only allocates tile memory from the user-exposed tile memory heap.
        // There is still additional tile memory that is reserved and not exposed to
        // the user.
        bool enableVkQcomTileMemoryHeap = false;
    };

    // Create a device
    virtual qre::RefCountPtr<Device> createDevice(const DeviceDesc& desc = {}) = 0;
    // Create a window
    ext::Window* createWindow(const ext::WindowDesc& desc = {});
    // Create a window that wraps an existing surface handle
    ext::Window* createWindowFromHandle(const ext::WindowDesc& desc, const ext::WindowSurface& surface);

    virtual Description* desc() = 0;
    virtual const Description* desc() const = 0;

    virtual ResultCode init() = 0;
};

using ContextDesc = Context::Description;

constexpr bool supportsSpecializationConstants(GraphicsBackend b)
{
    return (b != GraphicsBackend::eDirectX12);
}

// Create a QHWI context
extern qre::RefCountPtr<Context> createContext(const ContextDesc& desc = {});

extern const char* getVendorName(uint32_t id);

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
namespace compressedstorage
{
class File;

struct FileInfo
{
    size_t sizeInBytes = 0U;
};

// Source types
struct SourceMemory
{
    void const* sourceMemoryPtr = nullptr;
    size_t size = 0;

    SourceMemory& setSource(void const* v)
    {
        sourceMemoryPtr = v;
        return *this;
    }

    SourceMemory& setSize(size_t v)
    {
        size = v;
        return *this;
    }
};

struct SourceFile
{
    const File* file = nullptr;
    size_t offset = 0;
    size_t size = 0;

    SourceFile& setSource(const File* v)
    {
        file = v;
        return *this;
    }

    SourceFile& setOffset(size_t v)
    {
        offset = v;
        return *this;
    }

    SourceFile& setSize(size_t v)
    {
        size = v;
        return *this;
    }
};

// Destination types
struct DestinationMemory
{
    void* destinationMemoryPtr = nullptr;
    size_t size = 0;

    DestinationMemory& setDestination(void* v)
    {
        destinationMemoryPtr = v;
        return *this;
    }

    DestinationMemory& setSize(size_t v)
    {
        size = v;
        return *this;
    }
};

struct DestinationBuffer
{
    Buffer* buffer = nullptr;
    size_t offset = 0;
    size_t size = 0;

    DestinationBuffer& setDestination(Buffer* v)
    {
        buffer = v;
        return *this;
    }

    DestinationBuffer& setOffset(size_t v)
    {
        offset = v;
        return *this;
    }

    DestinationBuffer& setSize(size_t v)
    {
        size = v;
        return *this;
    }
};

struct DestinationTextureRegion
{
    Texture* texture = nullptr;
    uint32_t subresourceIndex = 0;
    uvec3 origin = {};
    Extent3D extent = {};

    DestinationTextureRegion& setDestination(Texture* v)
    {
        texture = v;
        return *this;
    }

    DestinationTextureRegion& setSubresourceIndex(uint32_t v)
    {
        subresourceIndex = v;
        return *this;
    }

    DestinationTextureRegion& setOrigin(uvec3 v)
    {
        origin = v;
        return *this;
    }

    DestinationTextureRegion& setExtent(Extent3D v)
    {
        extent = v;
        return *this;
    }
};

struct DestinationMultipleSubresources
{
    Texture* texture = nullptr;
    uint32_t firstSubresource = 0;

    DestinationMultipleSubresources& setDestination(Texture* v)
    {
        texture = v;
        return *this;
    }

    DestinationMultipleSubresources& setFirstSubresource(uint32_t v)
    {
        firstSubresource = v;
        return *this;
    }
};

struct DestinationTiles
{
    // TODO
};

struct Request
{
    StorageRequestSourceType sourceType;
    StorageRequestDestinationType destinationType;
    std::variant<SourceMemory, SourceFile> source;
    std::variant<DestinationBuffer, DestinationMemory, DestinationMultipleSubresources, DestinationTextureRegion, DestinationTiles> destination;
    CompressionFormat compressionFormat;
    size_t cancellationTag;
    size_t uncompressedSize;
    const char* name;
};

class StorageStatusArray : public qre::RefCounterDelete
{
public:
    virtual ResultCode getResult(uint32_t index = 0) const { return ResultCode::eUnsupported; }

    virtual bool isComplete(uint32_t index = 0) const { return false; }

    virtual ~StorageStatusArray() = default;
};

class File : public qre::RefCounterDelete
{
public:
    virtual ~File() = default;

    virtual void close() {};

    virtual void getFileInformation(FileInfo& fileInfo) const { fileInfo = {}; }
};

class StorageQueue : public qre::RefCounterDelete
{
public:
    struct Description
    {
        StorageRequestSourceType sourceType = StorageRequestSourceType::eFile;
        StoragePriority priority = StoragePriority::eNormal;
        const char* name = nullptr;
        Device* device = nullptr;

        Description& setSourceType(StorageRequestSourceType v)
        {
            sourceType = v;
            return *this;
        }

        Description& setPriority(StoragePriority v)
        {
            priority = v;
            return *this;
        }

        Description& setName(const char* v)
        {
            name = v;
            return *this;
        }

        Description& setDevice(Device* v)
        {
            device = v;
            return *this;
        }
    };

    uint32_t getNumEnqueuedRequests() const { return m_numEnqueuedRequests; }

    virtual void enqueueRequest(const Request&) {}

    virtual void enqueueStatus(StorageStatusArray* array, uint32_t index = 0) {}

    virtual void enqueueSignal(Fence* fence, uint64_t value) {}

    virtual void submit() {}

    // Based on DX12's texture pitch and subresource alignment
    // Other backends have more relaxed requirements
    static constexpr uint32_t s_RowPitchAlignment = 256;
    static constexpr uint32_t s_DataPlacementAlignment = 512;

protected:
    uint32_t m_numEnqueuedRequests = 0;
};

using StorageQueueDesc = StorageQueue::Description;

class Factory : public qre::RefCounterDelete
{
public:
    Factory() {}

    virtual ~Factory() {}

    virtual ResultCode init() { return ResultCode::eUnsupported; };

    virtual ResultCode openFile(const char* fileToLoad, File* file) const { return ResultCode::eUnsupported; };

    virtual qre::RefCountPtr<StorageQueue> createQueue(const StorageQueueDesc& desc) const { return nullptr; }

    virtual qre::RefCountPtr<StorageStatusArray> createStatusArray(uint32_t capacity = 1, const char* name = nullptr) const { return nullptr; };
};
} // namespace compressedstorage
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

} // namespace qhwi
#endif
