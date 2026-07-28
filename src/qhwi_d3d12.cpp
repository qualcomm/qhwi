//
//
//  qhwi_d3d12.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_d3d12.h>
#include <qhwi/qhwi_util.h>
#include <qhwi/qhwi_log_util.h>
#include <system_error>
#include <dxgidebug.h>
#include <sstream>
#include <qhwi/qhwi_profiling.h>

// Silence warnings from pix3.h
#if defined(__clang__) || defined(__GNUC__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
#include <pix3.h>
#if defined(__clang__) || defined(__GNUC__)
#    pragma clang diagnostic pop
#endif

#include <qre/math/hash.h>
#include <qre/memory/align.h>
#include <qre/text/textutil.h>
#include <qre/text/unicode.h>

using WString = wchar_t[qhwi::MaxStringSize];
using namespace qhwi_d3d_internal_extensions;

// Agility SDK Params

#ifdef QHWI_USE_AGILITY_SDK_717_PREVIEW
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 717;
}
#else
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 616;
}
#endif
extern "C"
{
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE qhwi::D3dRootSignature::D3D12SerializeVersionedRootSignature = nullptr;

#define V(func)                                                                                                                                                                    \
    {                                                                                                                                                                              \
        HRESULT hResult = func;                                                                                                                                                    \
        if (FAILED(hResult))                                                                                                                                                       \
        {                                                                                                                                                                          \
            QHWI_ERR("function: %s, result: 0x%08X message: %s", #func, hResult, std::system_category().message(hResult).c_str());                                                 \
        }                                                                                                                                                                          \
    }

#ifdef QHWI_ENABLE_DEBUG_BARRIERS
static std::string to_string(D3D12_BARRIER_SYNC s)
{
    if (s == D3D12_BARRIER_SYNC_NONE)
    {
        return "D3D12_BARRIER_SYNC_NONE";
    }
    const std::array v0 = { D3D12_BARRIER_SYNC_ALL,
                            D3D12_BARRIER_SYNC_DRAW,
                            D3D12_BARRIER_SYNC_INDEX_INPUT,
                            D3D12_BARRIER_SYNC_VERTEX_SHADING,
                            D3D12_BARRIER_SYNC_PIXEL_SHADING,
                            D3D12_BARRIER_SYNC_DEPTH_STENCIL,
                            D3D12_BARRIER_SYNC_RENDER_TARGET,
                            D3D12_BARRIER_SYNC_COMPUTE_SHADING,
                            D3D12_BARRIER_SYNC_RAYTRACING,
                            D3D12_BARRIER_SYNC_COPY,
                            D3D12_BARRIER_SYNC_RESOLVE,
                            D3D12_BARRIER_SYNC_EXECUTE_INDIRECT,
                            D3D12_BARRIER_SYNC_PREDICATION,
                            D3D12_BARRIER_SYNC_ALL_SHADING,
                            D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
                            D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO,
                            D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW,
                            D3D12_BARRIER_SYNC_VIDEO_DECODE,
                            D3D12_BARRIER_SYNC_VIDEO_PROCESS,
                            D3D12_BARRIER_SYNC_VIDEO_ENCODE,
                            D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
                            D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE,
                            D3D12_BARRIER_SYNC_SPLIT };
    const std::array v1 = { "ALL",
                            "DRAW",
                            "INDEX_INPUT",
                            "VERTEX_SHADING",
                            "PIXEL_SHADING",
                            "DEPTH_STENCIL",
                            "RENDER_TARGET",
                            "COMPUTE_SHADING",
                            "RAYTRACING",
                            "COPY",
                            "RESOLVE",
                            "EXECUTE_INDIRECT",
                            "PREDICATION",
                            "ALL_SHADING",
                            "NON_PIXEL_SHADING",
                            "EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO",
                            "CLEAR_UNORDERED_ACCESS_VIEW",
                            "VIDEO_DECODE",
                            "VIDEO_PROCESS",
                            "VIDEO_ENCODE",
                            "BUILD_RAYTRACING_ACCELERATION_STRUCTURE",
                            "COPY_RAYTRACING_ACCELERATION_STRUCTURE",
                            "SPLIT" };
    std::string str = "";
    for (uint32_t j = 0; j < v0.size(); j++)
    {
        if (s & v0[j])
        {
            str += (str.empty() ? "" : "|") + std::string(v1[j]);
        }
    }
    return "D3D12_BARRIER_SYNC(" + str + ")";
}

static std::string to_string(D3D12_BARRIER_ACCESS s)
{
    if (s == D3D12_BARRIER_ACCESS_COMMON)
    {
        return "D3D12_BARRIER_ACCESS_COMMON";
    }
    const std::array v0 = { D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
                            D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,
                            D3D12_BARRIER_ACCESS_INDEX_BUFFER,
                            D3D12_BARRIER_ACCESS_RENDER_TARGET,
                            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
                            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
                            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
                            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                            D3D12_BARRIER_ACCESS_STREAM_OUTPUT,
                            D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT,
                            D3D12_BARRIER_ACCESS_PREDICATION,
                            D3D12_BARRIER_ACCESS_COPY_DEST,
                            D3D12_BARRIER_ACCESS_COPY_SOURCE,
                            D3D12_BARRIER_ACCESS_RESOLVE_DEST,
                            D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
                            D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
                            D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
                            D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE,
                            D3D12_BARRIER_ACCESS_VIDEO_DECODE_READ,
                            D3D12_BARRIER_ACCESS_VIDEO_DECODE_WRITE,
                            D3D12_BARRIER_ACCESS_VIDEO_PROCESS_READ,
                            D3D12_BARRIER_ACCESS_VIDEO_PROCESS_WRITE,
                            D3D12_BARRIER_ACCESS_VIDEO_ENCODE_READ,
                            D3D12_BARRIER_ACCESS_VIDEO_ENCODE_WRITE };
    const std::array v1 = { "VERTEX_BUFFER",
                            "CONSTANT_BUFFER",
                            "INDEX_BUFFER",
                            "RENDER_TARGET",
                            "UNORDERED_ACCESS",
                            "DEPTH_STENCIL_WRITE",
                            "DEPTH_STENCIL_READ",
                            "SHADER_RESOURCE",
                            "STREAM_OUTPUT",
                            "INDIRECT_ARGUMENT",
                            "PREDICATION",
                            "COPY_DEST",
                            "COPY_SOURCE",
                            "RESOLVE_DEST",
                            "RESOLVE_SOURCE",
                            "RAYTRACING_ACCELERATION_STRUCTURE_READ",
                            "RAYTRACING_ACCELERATION_STRUCTURE_WRITE",
                            "SHADING_RATE_SOURCE",
                            "VIDEO_DECODE_READ",
                            "VIDEO_DECODE_WRITE",
                            "VIDEO_PROCESS_READ",
                            "VIDEO_PROCESS_WRITE",
                            "VIDEO_ENCODE_READ",
                            "VIDEO_ENCODE_WRITE",
                            "NO_ACCESS" };
    std::string str = "";
    for (uint32_t j = 0; j < v0.size(); j++)
    {
        if (s & v0[j])
        {
            str += (str.empty() ? "" : "|") + std::string(v1[j]);
        }
    }
    return "D3D12_BARRIER_ACCESS(" + str + ")";
}

static std::string to_string(D3D12_BARRIER_LAYOUT s)
{
    if (s == D3D12_BARRIER_LAYOUT_UNDEFINED)
    {
        return "D3D12_BARRIER_LAYOUT_UNDEFINED";
    }
    const std::array v1 = { "COMMON|PRESENT",
                            "GENERIC_READ",
                            "RENDER_TARGET",
                            "UNORDERED_ACCESS",
                            "DEPTH_STENCIL_WRITE",
                            "DEPTH_STENCIL_READ",
                            "SHADER_RESOURCE",
                            "COPY_SOURCE",
                            "COPY_DEST",
                            "RESOLVE_SOURCE",
                            "RESOLVE_DEST",
                            "SHADING_RATE_SOURCE",
                            "VIDEO_DECODE_READ",
                            "VIDEO_DECODE_WRITE",
                            "VIDEO_PROCESS_READ",
                            "VIDEO_PROCESS_WRITE",
                            "VIDEO_ENCODE_READ",
                            "VIDEO_ENCODE_WRITE",
                            "DIRECT_QUEUE_COMMON",
                            "DIRECT_QUEUE_GENERIC_READ",
                            "DIRECT_QUEUE_UNORDERED_ACCESS",
                            "DIRECT_QUEUE_SHADER_RESOURCE",
                            "DIRECT_QUEUE_COPY_SOURCE",
                            "DIRECT_QUEUE_COPY_DEST",
                            "COMPUTE_QUEUE_COMMON",
                            "COMPUTE_QUEUE_GENERIC_READ",
                            "COMPUTE_QUEUE_UNORDERED_ACCESS",
                            "COMPUTE_QUEUE_SHADER_RESOURCE",
                            "COMPUTE_QUEUE_COPY_SOURCE",
                            "COMPUTE_QUEUE_COPY_DEST",
                            "VIDEO_QUEUE_COMMON" };
    QHWI_ASSERT(size_t(s) < v1.size());
    return "D3D12_BARRIER_LAYOUT_" + std::string(v1[s]);
}

static std::string to_string(D3D12_RESOURCE_STATES s)
{
    if (s == 0)
    {
        return "D3D12_RESOURCE_STATE_COMMON";
    };
    const std::array v0 = { D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                            D3D12_RESOURCE_STATE_INDEX_BUFFER,
                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                            D3D12_RESOURCE_STATE_DEPTH_WRITE,
                            D3D12_RESOURCE_STATE_DEPTH_READ,
                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                            D3D12_RESOURCE_STATE_STREAM_OUT,
                            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                            D3D12_RESOURCE_STATE_PREDICATION,
                            D3D12_RESOURCE_STATE_COPY_DEST,
                            D3D12_RESOURCE_STATE_COPY_SOURCE,
                            D3D12_RESOURCE_STATE_RESOLVE_DEST,
                            D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                            D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
                            D3D12_RESOURCE_STATE_GENERIC_READ,
                            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                            D3D12_RESOURCE_STATE_PRESENT,
                            D3D12_RESOURCE_STATE_PREDICATION,
                            D3D12_RESOURCE_STATE_VIDEO_DECODE_READ,
                            D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE,
                            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ,
                            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE,
                            D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ,
                            D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE };
    const std::array v1 = { "VERTEX_AND_CONSTANT_BUFFER",
                            "INDEX_BUFFER",
                            "RENDER_TARGET",
                            "UNORDERED_ACCESS",
                            "DEPTH_WRITE",
                            "DEPTH_READ",
                            "NON_PIXEL_SHADER_RESOURCE",
                            "PIXEL_SHADER_RESOURCE",
                            "STREAM_OUT",
                            "INDIRECT_ARGUMENT",
                            "COPY_DEST",
                            "COPY_SOURCE",
                            "RESOLVE_DEST",
                            "RESOLVE_SOURCE",
                            "RAYTRACING_ACCELERATION_STRUCTURE",
                            "SHADING_RATE_SOURCE",
                            "GENERIC_READ",
                            "ALL_SHADER_RESOURCE",
                            "PRESENT",
                            "PREDICATION",
                            "VIDEO_DECODE_READ",
                            "VIDEO_DECODE_WRITE",
                            "VIDEO_PROCESS_READ",
                            "VIDEO_PROCESS_WRITE",
                            "VIDEO_ENCODE_READ",
                            "VIDEO_ENCODE_WRITE" };
    std::string str = "";
    for (uint32_t j = 0; j < v0.size(); j++)
    {
        if (s & v0[j])
        {
            str += (str.empty() ? "" : "|") + std::string(v1[j]);
        }
    }
    return "D3D12_RESOURCE_STATES(" + str + ")";
}

static std::string to_string(const D3D12_RESOURCE_TRANSITION_BARRIER& s)
{
    std::stringstream ss;
    ss << "D3D12_RESOURCE_TRANSITION_BARRIER(" << "pResource: " << s.pResource << " " << "stateBefore: " << to_string(s.StateBefore) << " "
       << "stateAfter: " << to_string(s.StateAfter) << " " << "subresource: " << s.Subresource << ")";
    return ss.str();
}

static std::string to_string(const D3D12_RESOURCE_ALIASING_BARRIER& s)
{
    std::stringstream ss;
    ss << "D3D12_RESOURCE_ALIASING_BARRIER(" << "pResourceBefore: " << s.pResourceBefore << " " << "pResourceAfter: " << s.pResourceAfter << ")";
    return ss.str();
}

static std::string to_string(const D3D12_RESOURCE_UAV_BARRIER& s)
{
    std::stringstream ss;
    ss << "D3D12_RESOURCE_UAV_BARRIER(" << "pResource: " << s.pResource << ")";
    return ss.str();
}

static std::string to_string(const D3D12_RESOURCE_BARRIER& s)
{
    switch (s.Type)
    {
    case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
        return to_string(s.Transition);
    case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
        return to_string(s.Aliasing);
    case D3D12_RESOURCE_BARRIER_TYPE_UAV:
        return to_string(s.UAV);
    default:
        return "";
    }
}
#endif

static void d3d12MessageCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR pDescription, void* pContext)
{
    if (severity <= D3D12_MESSAGE_SEVERITY_WARNING)
    {
        QHWI_LOGW("D3D12 %s: %s", (severity <= D3D12_MESSAGE_SEVERITY_ERROR) ? "ERROR" : "WARNING", pDescription);
    }
    else
    {
        QHWI_LOGI("D3D12 %s %s", (severity <= D3D12_MESSAGE_SEVERITY_INFO) ? "INFO" : "MESSAGE", pDescription);
    }
}

void qhwi::D3dUniqueDescriptorHandle::reset(D3dDescriptorHeap* heap, D3dDescriptorHandle handle, D3dDescriptorHeapManager* heapManager)
{
    m_heap = heap;
    m_heapManager = heapManager;
    m_handle = handle;
}

qhwi::D3dUniqueDescriptorHandle::~D3dUniqueDescriptorHandle()
{
    destroy();
}

void qhwi::D3dUniqueDescriptorHandle::destroy()
{
    if (!m_handle)
    {
        return;
    }
    if (m_heapManager)
    {
        m_heapManager->releaseHandle(m_handle);
    }

    if (m_heap)
    {
        m_heap->releaseHandle(m_handle);
    }
    m_handle.clear();
}

DXGI_HDR_METADATA_HDR10 qhwi::ext::DxgiSwapchainUtil::hdrMetadata(qhwi::ext::DisplayGamut displayGamut)
{
    DXGI_HDR_METADATA_HDR10 s{};
    DisplayChromaticityValues v = DisplayChromaticities[displayGamut];
    //https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10
    s.RedPrimary[0] = static_cast<UINT16>(v.RedPrimaryX * 50000.0f);
    s.RedPrimary[1] = static_cast<UINT16>(v.RedPrimaryY * 50000.0f);
    s.GreenPrimary[0] = static_cast<UINT16>(v.GreenPrimaryX * 50000.0f);
    s.GreenPrimary[1] = static_cast<UINT16>(v.GreenPrimaryY * 50000.0f);
    s.BluePrimary[0] = static_cast<UINT16>(v.BluePrimaryX * 50000.0f);
    s.BluePrimary[1] = static_cast<UINT16>(v.BluePrimaryY * 50000.0f);
    s.WhitePoint[0] = static_cast<UINT16>(v.WhitePointX * 50000.0f);
    s.WhitePoint[1] = static_cast<UINT16>(v.WhitePointY * 50000.0f);
    s.MaxMasteringLuminance = static_cast<UINT>(DefaultHdrMetadata::MaxLuminance * 10000.0f);
    s.MinMasteringLuminance = static_cast<UINT>(DefaultHdrMetadata::MinLuminance * 10000.0f);
    s.MaxContentLightLevel = static_cast<UINT16>(DefaultHdrMetadata::MaxContentLightLevel);
    s.MaxFrameAverageLightLevel = static_cast<UINT16>(DefaultHdrMetadata::MaxFrameAverageLightLevel);
    return s;
}

std::optional<DXGI_OUTPUT_DESC1> qhwi::ext::DxgiSwapchainUtil::getContainingOutputDesc(IDXGISwapChain* swapchain)
{
    ComPtr<IDXGIOutput> dxgiOutput;
    HRESULT hr = swapchain->GetContainingOutput(&dxgiOutput);
    if (FAILED(hr))
    {
        QHWI_LOGW("GetContainingOutput failed with HRESULT 0x%08X. No output descriptor available.", hr);
        return std::nullopt;
    }

    ComPtr<IDXGIOutput6> pBestDxgiOutput6;
    V(dxgiOutput->QueryInterface(IID_PPV_ARGS(&pBestDxgiOutput6)));

    DXGI_OUTPUT_DESC1 desc1;
    V(pBestDxgiOutput6->GetDesc1(&desc1));
    return desc1;
}

void qhwi::ext::DxgiSwapchainUtil::initDxgiSwapChainDesc(D3dDevice* device, const Swapchain::Description& desc0, DXGI_SWAP_CHAIN_DESC1& desc)
{
    uint32_t numImages = desc0.numImages;
    if (numImages > MaxSwapchainImages)
    {
        QHWI_FATAL("numImages (%d) > MaxSwapchainImages (%d)", numImages, MaxSwapchainImages);
    }

    desc = {};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = numImages;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    auto windowSurface = device->getWindowSurface();
    desc.Width = windowSurface->width;
    desc.Height = windowSurface->height;
}

qhwi::ext::DxgiSwapchain::DxgiSwapchain(D3dDevice* device, D3dCommandQueue* commandQueue, const Description& desc)
    : m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("DxgiSwapchain::DxgiSwapchain", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    QHWI_ASSERT(commandQueue);
    m_commandQueue = commandQueue;
    m_autoBarrier = desc.automaticBarriers;

    DXGI_SWAP_CHAIN_DESC1 dsDesc;
    DxgiSwapchainUtil::initDxgiSwapChainDesc(device, desc, dsDesc);
    chooseSurfaceFormat(dsDesc);

    init(m_device, commandQueue, desc, dsDesc);
}

qhwi::ext::DxgiSwapchain::DxgiSwapchain(D3dDevice* device, D3dCommandQueue* commandQueue, const Description& desc, const DXGI_SWAP_CHAIN_DESC1& dsDesc)
{
    init(device, commandQueue, desc, dsDesc);
}

void qhwi::ext::DxgiSwapchain::init(D3dDevice* device, D3dCommandQueue* commandQueue, const Description& desc, const DXGI_SWAP_CHAIN_DESC1& dsDesc)
{
    m_device = device;
    QHWI_ASSERT(device);
    QHWI_ASSERT(commandQueue);
    m_commandQueue = commandQueue;
    m_autoBarrier = desc.automaticBarriers;

    auto factory = m_device->getD3dContext()->getFactory();
    QHWI_ASSERT(factory);
    ComPtr<IDXGISwapChain1> swapchain;

    m_colorFormat = dsDesc.Format;
    auto windowSurface = m_device->getWindowSurface();
    QHWI_ASSERT(windowSurface);
    HWND hwnd = windowSurface->hwnd;
    QHWI_ASSERT(hwnd);
    V(factory->CreateSwapChainForHwnd(commandQueue->get(), hwnd, &dsDesc, nullptr, nullptr, &swapchain));
    V(swapchain.As(&m_swapchain));
    V(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES));
    getBuffers(desc.name, dsDesc, desc.automaticBarriers);
    createRenderTargetViews(dsDesc);
}

void qhwi::ext::DxgiSwapchain::resize(int width, int height)
{
    qhwi::ext::WindowSurface windowSurface = *m_device->getWindowSurface();
    windowSurface.width = width;
    windowSurface.height = height;
    resize(windowSurface);
}

void qhwi::ext::DxgiSwapchain::resize(const WindowSurface& surface)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("DxgiSwapchain::resize", QHWI_COLOR_GRAPHICS);
    m_device->setWindowSurface(surface);
    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        m_rtvDescriptorHandles[j].destroy();
        m_colorSwapchainTextures[j] = nullptr;
    }
    m_commandQueue->reclaimResources();
    V(m_swapchain->ResizeBuffers(m_numSwapchainImages, surface.width, surface.height, m_colorFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING));
    DXGI_SWAP_CHAIN_DESC1 dsDesc{};
    dsDesc.BufferCount = m_numSwapchainImages;
    dsDesc.Width = surface.width;
    dsDesc.Height = surface.height;
    dsDesc.Format = m_colorFormat;
    getBuffers("", dsDesc, m_autoBarrier);
    createRenderTargetViews(dsDesc);
}

qhwi::ext::DxgiSwapchain::~DxgiSwapchain()
{
    destroy();
}

qhwi::Format qhwi::ext::DxgiSwapchain::getFormat() const
{
    return qhwi::qhwi(m_colorFormat);
}

DXGI_SWAP_CHAIN_DESC1 qhwi::ext::DxgiSwapchain::getDesc1() const
{
    DXGI_SWAP_CHAIN_DESC1 desc1{};
    V(m_swapchain->GetDesc1(&desc1));
    return desc1;
}

void qhwi::ext::DxgiSwapchain::chooseSurfaceFormat(DXGI_SWAP_CHAIN_DESC1& dxgiSwapchainDesc)
{
    QHWI_ASSERT(m_device);
    auto d3dDevice = static_cast<D3dDevice*>(m_device)->get();
    for (size_t j = 0; j < DefaultFormatCandidates.size(); j++)
    {
        auto targetFormat = qhwi::d3d(DefaultFormatCandidates[j]);
        // Format requirements: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/ns-dxgi1_2-dxgi_swap_chain_desc1
        if (targetFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || targetFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        {
            continue;
        }
        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{};
        formatSupport.Format = targetFormat;
        if (SUCCEEDED(d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport))))
        {
            if (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_DISPLAY)
            {
                dxgiSwapchainDesc.Format = targetFormat;
                return;
            }
        }
    }
    QHWI_FATAL("choose swapchain surface format failed: cadidate formats not supported");
}

void qhwi::ext::DxgiSwapchain::getBuffers(const char* name1, const DXGI_SWAP_CHAIN_DESC1& dxgiSwapchainDesc, bool automaticBarriers)
{
    m_numSwapchainImages = dxgiSwapchainDesc.BufferCount;
    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        ComPtr<ID3D12Resource> buffer = nullptr;
        V(m_swapchain->GetBuffer(j, IID_PPV_ARGS(&buffer)));

        char name[MaxStringSize];
        qre::PrintfSafe(name, "%s_%d", (const char*)name1, j);

        auto desc = qhwi::Texture::Description()
                        .setFormat(qhwi::qhwi(dxgiSwapchainDesc.Format))
                        .setWidth(dxgiSwapchainDesc.Width)
                        .setHeight(dxgiSwapchainDesc.Height)
                        .setMipLevels(1)
                        .setUsage(TextureUsage_ColorAttachment)
                        .setName(name)
                        .setAutomaticBarriers(automaticBarriers);
        m_colorSwapchainTextures[j] = m_device->createTextureFromD3d12Resource(desc, buffer.Get()).CastTo<D3dTexture>();
    }
}

void qhwi::ext::DxgiSwapchain::createRenderTargetViews(const DXGI_SWAP_CHAIN_DESC1& dxgiSwapchainDesc)
{
    QHWI_ASSERT(m_device);
    auto d3dDevice = m_device->get();
    int numBuffers = dxgiSwapchainDesc.BufferCount;
    auto descriptorHeap = m_device->getRtvDescriptorHeap();
    for (int j = 0; j < numBuffers; j++)
    {
        std::optional<D3dDescriptorHandle> rtvDescriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(rtvDescriptorHandle.has_value(), "Invalid descriptor handle");
        d3dDevice->CreateRenderTargetView(m_colorSwapchainTextures[j]->get(), nullptr, rtvDescriptorHandle->cpuHandle);
        m_rtvDescriptorHandles[j].reset(descriptorHeap, *rtvDescriptorHandle);
    }
}

void qhwi::ext::DxgiSwapchain::acquireNextImage()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("DxgiSwapchain::acquireNextImage", QHWI_COLOR_GRAPHICS);
    m_currentImageIndex = m_swapchain->GetCurrentBackBufferIndex();
}

void qhwi::ext::DxgiSwapchain::presentImage(CommandQueue*)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("DxgiSwapchain::presentImage", QHWI_COLOR_GRAPHICS);
    V(m_swapchain->Present(1, 0));
}

void qhwi::ext::DxgiSwapchain::present(UINT syncInterval, UINT flags)
{
    V(m_swapchain->Present(syncInterval, flags));
}

qhwi::Texture* qhwi::ext::DxgiSwapchain::getColorImage(uint32_t index) const
{
    QRE_ASSERT(index < MaxSwapchainImages);
    return m_colorSwapchainTextures[index];
}

D3D12_CPU_DESCRIPTOR_HANDLE qhwi::ext::DxgiSwapchain::getAttachmentDescriptor(int index) const
{
    return m_rtvDescriptorHandles[index].get().cpuHandle;
}

void qhwi::ext::DxgiSwapchain::setColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace)
{
    V(m_swapchain->SetColorSpace1(colorSpace));
}

void qhwi::ext::DxgiSwapchain::destroy()
{
    if (!m_swapchain)
    {
        return;
    }

    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        m_rtvDescriptorHandles[j].destroy();

        // Explicitly release all texture resources
        if (m_colorSwapchainTextures[j])
        {
            m_device->onDestroyResource(m_colorSwapchainTextures[j]->get());
            m_colorSwapchainTextures[j] = nullptr;
        }
    }

    if (m_commandQueue)
    {
        m_commandQueue->reclaimResources();
    }

    m_swapchain.Reset();
}

void qhwi::setName(ID3D12Object* object, const char* name)
{
    if (!name)
    {
        return;
    }
    WString wname;
    qre::CharToWideChar(wname, name);
    V(object->SetName(wname));
}

qhwi::D3dCommandList::Description::Description(const qhwi::CommandList::Description& desc)
    : base(desc)
{
    d3d.type = qhwi::d3d(base.type);
}

qhwi::D3dCommandList::D3dCommandList(D3dDevice* device, const Description& desc)
    : CommandList()
    , m_device(device)
    , m_desc(desc)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dCommandList::D3dCommandList", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    auto d3dDevice = device->get();
    V(d3dDevice->CreateCommandAllocator(m_desc.d3d.type, IID_PPV_ARGS(&m_commandAllocator)));
    V(d3dDevice->CreateCommandList(0, m_desc.d3d.type, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    setName(m_commandList.Get(), m_desc.base.name);
    V(m_commandList->Close());

    m_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

void qhwi::D3dCommandList::submitted(CommandQueue* commandQueue, ID3D12Fence* fence, uint64_t lastSubmittedID)
{
    fence->SetEventOnCompletion(lastSubmittedID, m_event);

    for (uint32_t i = 0; i < m_referencedTimerQueriesIndex; ++i)
    {
        auto d3dTimerQuery = static_cast<D3dTimerQuery*>(m_referencedTimerQueries[i]);
        d3dTimerQuery->cmdListCompletedEvent = m_event;
        d3dTimerQuery->lastSubmittedID = lastSubmittedID;

        m_referencedTimerQueries[i] = nullptr;
    }

    m_referencedTimerQueriesIndex = 0;
}

void qhwi::D3dCommandList::addBarriers(uint32_t numBufferBarriers,
                                       const BufferBarrier* bufferBarriers,
                                       uint32_t numTextureBarriers,
                                       const TextureBarrier* textureBarriers,
                                       uint32_t numGlobalBarriers,
                                       const GlobalBarrier* globalBarriers,
                                       DependencyFlags flags)
{
    if (numBufferBarriers == 0 && numTextureBarriers == 0 && numGlobalBarriers == 0)
    {
        return;
    }

    for (uint32_t j = 0; j < numBufferBarriers; j++)
    {
        const BufferBarrier& b = bufferBarriers[j];
        if (!b.buffer->automaticBarriersEnabled())
        {
            continue;
        }
        auto& s = bufferState(b.buffer);
        s.currentState = b.dst;
    }
    for (uint32_t j = 0; j < numTextureBarriers; j++)
    {
        const TextureBarrier& b = textureBarriers[j];
        if (!b.texture->automaticBarriersEnabled())
        {
            continue;
        }
        setCurrentState(b.texture, b.dst, b.subresourceRange);
    }

    QHWI_ASSERT(numBufferBarriers < MaxBarriers);
    QHWI_ASSERT(numTextureBarriers < MaxBarriers);
    QHWI_ASSERT(numGlobalBarriers < MaxBarriers);
    CD3DX12_BUFFER_BARRIER d3dBufferBarriers[MaxBarriers];
    CD3DX12_TEXTURE_BARRIER d3dTextureBarriers[MaxBarriers];
    CD3DX12_GLOBAL_BARRIER d3dGlobalBarriers[MaxBarriers];
    const D3D12_RESOURCE_DESC* d3dTextureDescs[MaxBarriers];
    auto d3dBufferBarriersPtr = d3dBufferBarriers;
    auto d3dTextureBarriersPtr = d3dTextureBarriers;
    auto d3dGlobalBarriersPtr = d3dGlobalBarriers;
    CD3DX12_BARRIER_GROUP d3dBarrierGroups[3];
    uint32_t numBarrierGroups = 0;
    auto d3dBarrierGroupsPtr = d3dBarrierGroups;
    if (numBufferBarriers > 0)
    {
        *d3dBarrierGroupsPtr++ = CD3DX12_BARRIER_GROUP(numBufferBarriers, d3dBufferBarriers);
        numBarrierGroups++;
    }
    if (numTextureBarriers > 0)
    {
        *d3dBarrierGroupsPtr++ = CD3DX12_BARRIER_GROUP(numTextureBarriers, d3dTextureBarriers);
        numBarrierGroups++;
    }
    if (numGlobalBarriers > 0)
    {
        *d3dBarrierGroupsPtr++ = CD3DX12_BARRIER_GROUP(numGlobalBarriers, d3dGlobalBarriers);
        numBarrierGroups++;
    }
    for (uint32_t j = 0; j < numBufferBarriers; j++)
    {
        const BufferBarrier& b = bufferBarriers[j];
        QHWI_ASSERT(b.buffer);
        auto d3dBuffer = static_cast<D3dBuffer*>(b.buffer);
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
        QHWI_LOGI("barrier buffer: %p srcSync: %s srcAccess %s dstSync: %s dstAccess: %s",
                  b.buffer,
                  to_string(qhwi::d3dBarrierSync(b.src.stages, b.src.access)).c_str(),
                  to_string(qhwi::d3dBarrierAccess(b.src.stages, b.src.access)).c_str(),
                  to_string(qhwi::d3dBarrierSync(b.dst.stages, b.dst.access)).c_str(),
                  to_string(qhwi::d3dBarrierAccess(b.dst.stages, b.dst.access)).c_str());
#endif
        *d3dBufferBarriersPtr++ = CD3DX12_BUFFER_BARRIER(qhwi::d3dBarrierSync(b.src.stages, b.src.access),
                                                         qhwi::d3dBarrierSync(b.dst.stages, b.dst.access),
                                                         qhwi::d3dBarrierAccess(b.src.stages, b.src.access),
                                                         qhwi::d3dBarrierAccess(b.dst.stages, b.dst.access),
                                                         d3dBuffer->get());
    }
    for (uint32_t j = 0; j < numTextureBarriers; j++)
    {
        const TextureBarrier& b = textureBarriers[j];
        QHWI_ASSERT(b.texture);
        auto d3dTexture = static_cast<D3dTexture*>(b.texture);
        const auto& subresourceRange = b.subresourceRange;
        CD3DX12_BARRIER_SUBRESOURCE_RANGE d3dSubresourceRange = subresourceRange.isEntireResource() ?
                                                                    CD3DX12_BARRIER_SUBRESOURCE_RANGE(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) :
                                                                    CD3DX12_BARRIER_SUBRESOURCE_RANGE(subresourceRange.start.mipSlice,
                                                                                                      subresourceRange.end.mipSlice - subresourceRange.start.mipSlice + 1,
                                                                                                      subresourceRange.start.arraySlice,
                                                                                                      subresourceRange.end.arraySlice - subresourceRange.start.arraySlice + 1);
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
        QHWI_LOGI("barrier texture: %p srcSync: %s srcAccess %s srcLayout: %s dstSync: %s dstAccess: %s dstLayout: %s subresourceRange: (%d,%d,%d) to (%d,%d,%d)",
                  b.texture,
                  to_string(qhwi::d3dBarrierSync(b.src.stages, b.src.access)).c_str(),
                  to_string(qhwi::d3dBarrierAccess(b.src.stages, b.src.access)).c_str(),
                  to_string(qhwi::d3dBarrierLayout(b.src.access, b.src.layout)).c_str(),
                  to_string(qhwi::d3dBarrierSync(b.dst.stages, b.src.access)).c_str(),
                  to_string(qhwi::d3dBarrierAccess(b.dst.stages, b.dst.access)).c_str(),
                  to_string(qhwi::d3dBarrierLayout(b.dst.access, b.dst.layout)).c_str(),
                  b.subresourceRange.start.mipSlice,
                  b.subresourceRange.start.arraySlice,
                  b.subresourceRange.start.planeSlice,
                  b.subresourceRange.end.mipSlice,
                  b.subresourceRange.end.arraySlice,
                  b.subresourceRange.end.planeSlice);
#endif
        *d3dTextureBarriersPtr++ = CD3DX12_TEXTURE_BARRIER(qhwi::d3dBarrierSync(b.src.stages, b.src.access),
                                                           qhwi::d3dBarrierSync(b.dst.stages, b.dst.access),
                                                           qhwi::d3dBarrierAccess(b.src.stages, b.src.access),
                                                           qhwi::d3dBarrierAccess(b.dst.stages, b.dst.access),
                                                           qhwi::d3dBarrierLayout(b.src.access, b.src.layout),
                                                           qhwi::d3dBarrierLayout(b.dst.access, b.dst.layout),
                                                           d3dTexture->get(),
                                                           d3dSubresourceRange);
        d3dTextureDescs[j] = &d3dTexture->resourceDesc();
    }
    for (uint32_t j = 0; j < numGlobalBarriers; j++)
    {
        const GlobalBarrier& b = globalBarriers[j];
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
        QHWI_LOGI("barrier global: srcSync: %s srcAccess %s dstSync: %s dstAccess: %s",
                  to_string(qhwi::d3dBarrierSync(b.srcStages, b.srcAccess)).c_str(),
                  to_string(qhwi::d3dBarrierAccess(b.srcStages, b.srcAccess)).c_str(),
                  to_string(qhwi::d3dBarrierSync(b.dstStages, b.dstAccess)).c_str(),
                  to_string(qhwi::d3dBarrierAccess(b.dstStages, b.dstAccess)).c_str());
#endif
        *d3dGlobalBarriersPtr++ = CD3DX12_GLOBAL_BARRIER(qhwi::d3dBarrierSync(b.srcStages, b.srcAccess),
                                                         qhwi::d3dBarrierSync(b.dstStages, b.dstAccess),
                                                         qhwi::d3dBarrierAccess(b.srcStages, b.srcAccess),
                                                         qhwi::d3dBarrierAccess(b.dstStages, b.dstAccess));
    }
    addBarriers(numBarrierGroups, d3dBarrierGroups, d3dTextureDescs);
}

#define D3D12_RESOURCE_STATES_FROM_ACCESS(s, t)                                                                                                                                    \
    if (access & D3D12_BARRIER_ACCESS_##s)                                                                                                                                         \
    states |= D3D12_RESOURCE_STATE_##t
#define D3D12_RESOURCE_STATES_FROM_SYNC(s, t)                                                                                                                                      \
    if (sync & D3D12_BARRIER_SYNC_##s)                                                                                                                                             \
    states |= D3D12_RESOURCE_STATE_##t

static D3D12_RESOURCE_STATES toResourceStates(const D3D12_BARRIER_SYNC& sync, const D3D12_BARRIER_ACCESS& access)
{
    if (access == D3D12_BARRIER_ACCESS_COMMON)
    {
        return D3D12_RESOURCE_STATE_COMMON;
    }
    D3D12_RESOURCE_STATES states = (D3D12_RESOURCE_STATES)0;
    D3D12_RESOURCE_STATES_FROM_ACCESS(VERTEX_BUFFER, VERTEX_AND_CONSTANT_BUFFER);
    D3D12_RESOURCE_STATES_FROM_ACCESS(CONSTANT_BUFFER, VERTEX_AND_CONSTANT_BUFFER);
    D3D12_RESOURCE_STATES_FROM_ACCESS(INDEX_BUFFER, INDEX_BUFFER);
    D3D12_RESOURCE_STATES_FROM_ACCESS(RENDER_TARGET, RENDER_TARGET);
    D3D12_RESOURCE_STATES_FROM_ACCESS(UNORDERED_ACCESS, UNORDERED_ACCESS);
    D3D12_RESOURCE_STATES_FROM_ACCESS(DEPTH_STENCIL_WRITE, DEPTH_WRITE);
    D3D12_RESOURCE_STATES_FROM_ACCESS(DEPTH_STENCIL_READ, DEPTH_READ);
    D3D12_RESOURCE_STATES_FROM_ACCESS(STREAM_OUTPUT, STREAM_OUT);
    D3D12_RESOURCE_STATES_FROM_ACCESS(INDIRECT_ARGUMENT, INDIRECT_ARGUMENT);
    D3D12_RESOURCE_STATES_FROM_ACCESS(PREDICATION, PREDICATION);
    D3D12_RESOURCE_STATES_FROM_ACCESS(COPY_DEST, COPY_DEST);
    D3D12_RESOURCE_STATES_FROM_ACCESS(COPY_SOURCE, COPY_SOURCE);
    D3D12_RESOURCE_STATES_FROM_ACCESS(RESOLVE_DEST, RESOLVE_DEST);
    D3D12_RESOURCE_STATES_FROM_ACCESS(RESOLVE_SOURCE, RESOLVE_SOURCE);
    D3D12_RESOURCE_STATES_FROM_ACCESS(RAYTRACING_ACCELERATION_STRUCTURE_READ, NON_PIXEL_SHADER_RESOURCE);
    D3D12_RESOURCE_STATES_FROM_ACCESS(RAYTRACING_ACCELERATION_STRUCTURE_WRITE, UNORDERED_ACCESS);
    D3D12_RESOURCE_STATES_FROM_ACCESS(SHADING_RATE_SOURCE, SHADING_RATE_SOURCE);
    if (access & D3D12_BARRIER_ACCESS_SHADER_RESOURCE)
    {
        D3D12_RESOURCE_STATES_FROM_SYNC(PIXEL_SHADING, PIXEL_SHADER_RESOURCE);
        D3D12_RESOURCE_STATES_FROM_SYNC(NON_PIXEL_SHADING, NON_PIXEL_SHADER_RESOURCE);
        D3D12_RESOURCE_STATES_FROM_SYNC(BUILD_RAYTRACING_ACCELERATION_STRUCTURE, NON_PIXEL_SHADER_RESOURCE);
    }
    return states;
}

template <typename T>
static bool toResourceBarrier(const T& b, CD3DX12_RESOURCE_BARRIER& states, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
{
    D3D12_RESOURCE_STATES stateBefore = toResourceStates(b.SyncBefore, b.AccessBefore), stateAfter = toResourceStates(b.SyncAfter, b.AccessAfter);
    if (stateBefore == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && stateAfter == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        states = CD3DX12_RESOURCE_BARRIER::UAV(b.pResource);
        return true;
    }
    else if (stateBefore == stateAfter)
    {
        return false;
    }
    else
    {
        states = CD3DX12_RESOURCE_BARRIER::Transition(b.pResource, stateBefore, stateAfter, subresource);
        return true;
    }
}

void qhwi::D3dCommandList::addBarriers(uint32_t numBarrierGroups, const CD3DX12_BARRIER_GROUP* barriers, const D3D12_RESOURCE_DESC** textureDescs)
{
    if (m_device->supportsEnhancedBarriers())
    {
        m_commandList->Barrier(numBarrierGroups, barriers);
    }
    else
    {
        for (uint32_t j = 0; j < numBarrierGroups; j++)
        {
            const CD3DX12_BARRIER_GROUP& b = barriers[j];
            CD3DX12_RESOURCE_BARRIER resourceBarriers[MaxBarriers];
            auto resourceBarriersPtr = resourceBarriers;
            uint32_t numResourceBarriers = 0;
            if (b.Type == D3D12_BARRIER_TYPE_TEXTURE)
            {
                for (uint32_t k = 0; k < b.NumBarriers; k++)
                {
                    UINT subresourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    const auto& textureSubresource = b.pTextureBarriers[k].Subresources;
                    // D3D12_TEXTURE_BARRIER (enhanced barrier) supports subresource ranges while
                    // D3D12_RESOURCE_BARRIER only supports setting a barrier for one subresource;
                    // If only one subresource is present, get subresourceIndex
                    if (b.pTextureBarriers[k].Subresources.NumArraySlices == 1 && b.pTextureBarriers[k].Subresources.NumMipLevels == 1 &&
                        b.pTextureBarriers[k].Subresources.NumPlanes)
                    {
                        QHWI_ASSERT_MSG(textureDescs[j] != nullptr, "Invalid texture desc");
                        subresourceIndex = D3D12CalcSubresource(textureSubresource.IndexOrFirstMipLevel,
                                                                textureSubresource.FirstArraySlice,
                                                                textureSubresource.FirstPlane,
                                                                textureDescs[j]->MipLevels,
                                                                textureDescs[j]->DepthOrArraySize);
                    }

                    if (toResourceBarrier(b.pTextureBarriers[k], *resourceBarriersPtr, subresourceIndex))
                    {
                        resourceBarriersPtr++;
                        numResourceBarriers++;
                    }
                }
            }
            else if (b.Type == D3D12_BARRIER_TYPE_BUFFER)
            {
                for (uint32_t k = 0; k < b.NumBarriers; k++)
                {
                    if (toResourceBarrier(b.pBufferBarriers[k], *resourceBarriersPtr))
                    {
                        resourceBarriersPtr++;
                        numResourceBarriers++;
                    }
                }
            }
            if (numResourceBarriers > 0)
            {
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
                for (uint32_t j0 = 0; j0 < numResourceBarriers; j0++)
                {
                    QHWI_LOGI("resourceBarrier[%d]: %s", j0, to_string(resourceBarriers[j0]).c_str());
                }
#endif
                m_commandList->ResourceBarrier(numResourceBarriers, resourceBarriers);
            }
        }
    }
}

qhwi::D3dCommandList::~D3dCommandList()
{
    m_device->uploadPool().reset(this);
    CloseHandle(m_event);
}

void qhwi::D3dCommandList::beginMarker(const char* name)
{
    QHWI_ASSERT(name);
    if (defer())
    {
        m_rp.beginMarker(name);
        return;
    }
    PIXBeginEvent(m_commandList.Get(), 0, name);
}

void qhwi::D3dCommandList::endMarker()
{
    if (defer())
    {
        m_rp.endMarker();
        return;
    }
    PIXEndEvent(m_commandList.Get());
}

void qhwi::D3dCommandList::buildBottomLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                                                 Buffer* scratchBuffer,
                                                                 BottomLevelDescriptor* bottomLevelDescriptors,
                                                                 size_t descriptorCount,
                                                                 void* scratchMemory,
                                                                 AccelerationStructureBuildMode buildMode,
                                                                 AccelerationStructureBuildFlags flags)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::buildBottomLevelAccelerationStructure", QHWI_COLOR_RAYTRACING);
    QHWI_ASSERT(m_inAccelerationStructurePass);
    QHWI_ASSERT(accelerationStructure);
    QHWI_ASSERT(scratchBuffer);
    QHWI_ASSERT(scratchMemory);
    if (descriptorCount == 0)
    {
        return;
    }

    if (scratchBuffer->automaticBarriersEnabled())
    {
        setState(scratchBuffer, BufferState_ComputeShader_UnorderedAccess);
    }

    // Array of geometry descriptors, one per BLAS
    D3D12_RAYTRACING_GEOMETRY_DESC* descriptors = reinterpret_cast<D3D12_RAYTRACING_GEOMETRY_DESC*>(scratchMemory);

    for (size_t i = 0; i < descriptorCount; i++)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC& d3dDescriptor(descriptors[i]);
        const BottomLevelDescriptor& inputDescriptor(bottomLevelDescriptors[i]);
        D3dBuffer* d3dVertexBuffer = static_cast<D3dBuffer*>(inputDescriptor.triangles.vertexBuffer);

        // TODO: Support bounding boxes
        // TODO: Support BLAS transforms
        d3dDescriptor = D3D12_RAYTRACING_GEOMETRY_DESC();
        d3dDescriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        d3dDescriptor.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        d3dDescriptor.Triangles.Transform3x4 = NULL;
        d3dDescriptor.Triangles.VertexCount = inputDescriptor.triangles.vertexCount;
        d3dDescriptor.Triangles.VertexFormat = d3d(inputDescriptor.triangles.vertexFormat);
        d3dDescriptor.Triangles.VertexBuffer.StartAddress = d3dVertexBuffer->getDeviceAddress() + inputDescriptor.triangles.vertexOffset;
        d3dDescriptor.Triangles.VertexBuffer.StrideInBytes = inputDescriptor.triangles.vertexStride;

        // Required by BuildRaytracingAccelerationStructure()
        if (inputDescriptor.triangles.indexBuffer)
        {
            D3dBuffer* d3dIndexBuffer = static_cast<D3dBuffer*>(inputDescriptor.triangles.indexBuffer);

            d3dDescriptor.Triangles.IndexCount = inputDescriptor.triangles.indexCount;
            d3dDescriptor.Triangles.IndexFormat = d3d(inputDescriptor.triangles.indexFormat);
            d3dDescriptor.Triangles.IndexBuffer = d3dIndexBuffer->getDeviceAddress() + inputDescriptor.triangles.indexOffset;
        }
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};

    auto& buildInputs = buildDesc.Inputs;

    buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildInputs.NumDescs = UINT(descriptorCount);
    buildInputs.Flags = qhwi::d3dRaytracingAccelerationStructureBuildFlags(buildMode, flags);
    buildInputs.pGeometryDescs = descriptors;

    buildDesc.DestAccelerationStructureData = static_cast<D3dAccelerationStructure*>(accelerationStructure)->getDeviceAddress();
    buildDesc.SourceAccelerationStructureData = NULL;
    buildDesc.ScratchAccelerationStructureData = static_cast<D3dBuffer*>(scratchBuffer)->getDeviceAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    markResourceAsUsed(accelerationStructure);
    markResourceAsUsed(scratchBuffer);
}

void qhwi::D3dCommandList::buildTopLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                                              Buffer* instanceBuffer,
                                                              Buffer* scratchBuffer,
                                                              size_t instanceCount,
                                                              AccelerationStructure** blasArray,
                                                              size_t blasCount,
                                                              AccelerationStructureBuildMode buildMode,
                                                              AccelerationStructureBuildFlags flags)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::buildTopLevelAccelerationStructure", QHWI_COLOR_RAYTRACING);
    QHWI_ASSERT(m_inAccelerationStructurePass);
    QHWI_ASSERT(accelerationStructure);
    QHWI_ASSERT(scratchBuffer);

    if (scratchBuffer->automaticBarriersEnabled())
    {
        setState(scratchBuffer, BufferState_ComputeShader_UnorderedAccess);
    }

    D3dBuffer* d3dInstanceBuffer = static_cast<D3dBuffer*>(instanceBuffer);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs{};

    buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    buildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildInputs.NumDescs = UINT(instanceCount);
    buildInputs.Flags = qhwi::d3dRaytracingAccelerationStructureBuildFlags(buildMode, flags);
    buildInputs.InstanceDescs = d3dInstanceBuffer->getDeviceAddress();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
    buildDesc.DestAccelerationStructureData = static_cast<D3dAccelerationStructure*>(accelerationStructure)->getDeviceAddress();
    buildDesc.Inputs = buildInputs;
    buildDesc.SourceAccelerationStructureData = NULL;
    buildDesc.ScratchAccelerationStructureData = static_cast<D3dBuffer*>(scratchBuffer)->getDeviceAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    markResourceAsUsed(accelerationStructure);
    markResourceAsUsed(instanceBuffer);
    markResourceAsUsed(scratchBuffer);
}

void qhwi::D3dRenderTargetViewCache::init(D3dDevice* device)
{
    QHWI_ASSERT(device);
    m_device = device;
}

const qhwi::D3dRenderTargetViewCache::Entry& qhwi::D3dRenderTargetViewCache::get(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc)
{
    QHWI_ASSERT(m_device);
    QHWI_ASSERT(resource);
    QHWI_ASSERT(desc);

    auto& resDescIter = m_cache[resource];
    auto descEntryIter = resDescIter.find(*desc);
    if (descEntryIter == resDescIter.end())
    {
        auto& entry = resDescIter[*desc];
        entry.desc = *desc;

        auto descriptorHeap = m_device->getRtvDescriptorHeap();
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        auto d3dDevice = m_device->get();
        d3dDevice->CreateRenderTargetView(resource, &entry.desc, descriptorHandle->cpuHandle);

        entry.descriptorHandle.reset(descriptorHeap, *descriptorHandle);
        return entry;
    }
    else
    {
        return descEntryIter->second;
    }
}

size_t qhwi::D3dRenderTargetViewCache::RtvDescHash::operator()(const D3D12_RENDER_TARGET_VIEW_DESC& v) const
{
    uint64_t hash = qre::Fnv1a64Init();
    return hash_util::Fnv1a64Continue(hash, v);
}

void qhwi::D3dShaderResourceViewCache::init(D3dDevice* device)
{
    QHWI_ASSERT(device);
    m_device = device;
}

const qhwi::D3dShaderResourceViewCache::Entry& qhwi::D3dShaderResourceViewCache::get(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
    QHWI_ASSERT(m_device);
    QHWI_ASSERT(desc);

    auto& resDescIter = m_cache[resource];
    auto descEntryIter = resDescIter.find(*desc);
    if (descEntryIter == resDescIter.end())
    {
        auto& entry = resDescIter[*desc];
        entry.desc = *desc;

        auto descriptorHeap = m_device->getCbvSrvUavDescriptorHeap();
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        auto d3dDevice = m_device->get();
        d3dDevice->CreateShaderResourceView(resource, &entry.desc, descriptorHandle->cpuHandle);

        entry.descriptorHandle.reset(nullptr, *descriptorHandle, descriptorHeap);

        auto shaderVisibleDescriptorHeap = m_device->getCbvSrvUavShaderVisibleDescriptorHeap();
        std::optional<D3dDescriptorHandle> shaderVisibleDescriptorHandle = shaderVisibleDescriptorHeap->stage(entry.descriptorHandle.get());
        QHWI_ASSERT_MSG(shaderVisibleDescriptorHandle.has_value(), "Invalid descriptor handle");
        entry.shaderVisibleDescriptorHandle.reset(shaderVisibleDescriptorHeap, *shaderVisibleDescriptorHandle);

        return entry;
    }
    else
    {
        return descEntryIter->second;
    }
}

size_t qhwi::D3dShaderResourceViewCache::SrvDescHash::operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& v) const
{
    uint64_t hash = qre::Fnv1a64Init();
    return hash_util::Fnv1a64Continue(hash, v);
}

void qhwi::D3dUnorderedAccessViewCache::init(D3dDevice* device)
{
    QHWI_ASSERT(device);
    m_device = device;
}

const qhwi::D3dUnorderedAccessViewCache::Entry& qhwi::D3dUnorderedAccessViewCache::get(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    QHWI_ASSERT(m_device);
    QHWI_ASSERT(desc);

    auto& resDescIter = m_cache[resource];
    auto descEntryIter = resDescIter.find(*desc);
    if (descEntryIter == resDescIter.end())
    {
        auto& entry = resDescIter[*desc];
        entry.desc = *desc;

        auto descriptorHeap = m_device->getCbvSrvUavDescriptorHeap();
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        auto d3dDevice = m_device->get();
        d3dDevice->CreateUnorderedAccessView(resource, nullptr, &entry.desc, descriptorHandle->cpuHandle);

        entry.descriptorHandle.reset(nullptr, *descriptorHandle, descriptorHeap);

        auto shaderVisibleDescriptorHeap = m_device->getCbvSrvUavShaderVisibleDescriptorHeap();
        std::optional<D3dDescriptorHandle> shaderVisibleDescriptorHandle = shaderVisibleDescriptorHeap->stage(entry.descriptorHandle.get());
        QHWI_ASSERT_MSG(shaderVisibleDescriptorHandle.has_value(), "Invalid descriptor handle");
        entry.shaderVisibleDescriptorHandle.reset(shaderVisibleDescriptorHeap, *shaderVisibleDescriptorHandle);

        return entry;
    }
    else
    {
        return descEntryIter->second;
    }
}

size_t qhwi::D3dUnorderedAccessViewCache::UavDescHash::operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& v) const
{
    uint64_t hash = qre::Fnv1a64Init();
    return hash_util::Fnv1a64Continue(hash, v);
}

void qhwi::D3dDepthStencilViewCache::init(D3dDevice* device)
{
    QHWI_ASSERT(device);
    m_device = device;
}

const qhwi::D3dDepthStencilViewCache::Entry& qhwi::D3dDepthStencilViewCache::get(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
{
    QHWI_ASSERT(m_device);
    QHWI_ASSERT(resource);
    QHWI_ASSERT(desc);

    auto& resDescIter = m_cache[resource];
    auto descEntryIter = resDescIter.find(*desc);
    if (descEntryIter == resDescIter.end())
    {
        auto& entry = resDescIter[*desc];
        entry.desc = *desc;

        auto descriptorHeap = m_device->getDsvDescriptorHeap();
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        auto d3dDevice = m_device->get();
        d3dDevice->CreateDepthStencilView(resource, &entry.desc, descriptorHandle->cpuHandle);

        entry.descriptorHandle.reset(descriptorHeap, *descriptorHandle);

        return entry;
    }
    else
    {
        return descEntryIter->second;
    }
}

size_t qhwi::D3dDepthStencilViewCache::DsvDescHash::operator()(const D3D12_DEPTH_STENCIL_VIEW_DESC& v) const
{
    uint64_t hash = qre::Fnv1a64Init();
    return hash_util::Fnv1a64Continue(hash, v);
}

qhwi::D3dTexture::Description::Description(const Texture::Description& desc)
    : base(desc)
{
    d3d.heapFlags = qhwi::d3dHeapFlags(desc.heapFlags);
}

qhwi::D3dTexture::~D3dTexture()
{
    m_device->onDestroyResource(m_resource.Get());
    if (m_allocation)
    {
        m_allocation->Release();
    }
}

uint32_t qhwi::D3dTexture::pitch(uint16_t mipLevel) const
{
    return calculatePitch(m_desc.format, std::max(m_desc.w >> mipLevel, 1u));
}

uint32_t qhwi::D3dTexture::calculatePitch(Format format, uint32_t w) const
{
    return qre::AlignUp(FormatUtil::getRowPitch(format, w), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
}

void qhwi::D3dTexture::init(const Description& desc)
{
    const uint32_t w = desc.base.w, h = desc.base.h, d = desc.base.d, arrayLayers = desc.base.arrayLayers;
    const SampleCount numSamples = desc.base.numSamples;
    auto& mipLevels = m_desc.mipLevels;
    if (mipLevels == 0)
    {
        mipLevels = calculateMipLevels(desc.base.w, desc.base.h, desc.base.d);
    }
    const auto type = desc.base.type;
    auto usage = desc.base.usage;
    if (usage & TextureUsage_InputAttachment)
    {
        usage |= TextureUsage_Sampled;
    }
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;

    if (usage & TextureUsage_ColorAttachment)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (usage & TextureUsage_DepthStencilAttachment)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    if (usage & TextureUsage_Storage)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    auto d3dFormat = qhwi::d3d(desc.base.format);

    switch (type)
    {
    case TextureType::e1D:
        m_resourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(d3dFormat, w, UINT16(d * arrayLayers), UINT16(mipLevels), resourceFlags);
        break;

    case TextureType::e2D:
    case TextureType::e2DArray:
    case TextureType::eCube:
        m_resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(d3dFormat, w, h, UINT16(d * arrayLayers), UINT16(mipLevels), UINT(numSamples), 0, resourceFlags);
        break;

    case TextureType::e3D:
        m_resourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(d3dFormat, w, h, UINT16(d), UINT16(mipLevels), resourceFlags);
        break;

    default:
        QHWI_TODO();
        break;
    }

    if (usage & TextureUsage_Sampled)
    {
        m_defaultSrvDesc.Format = qhwi::d3dSrvFormat(d3dFormat);
        //TODO: support other types
        m_defaultSrvDesc.ViewDimension = qhwi::d3dSrvDimension(type, numSamples);
        m_defaultSrvDesc.Shader4ComponentMapping =
            D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(d3d(desc.base.rSwizzle), d3d(desc.base.gSwizzle), d3d(desc.base.bSwizzle), d3d(desc.base.aSwizzle));
        switch (type)
        {
        case TextureType::e1D:
        {
            if (numSamples == SampleCount::e1)
            {
                auto& p0 = m_defaultSrvDesc.Texture1D;
                p0.MipLevels = mipLevels;
                p0.MostDetailedMip = 0;
                p0.ResourceMinLODClamp = 0.0f;
            }
            break;
        }
        case TextureType::e2D:
        {
            if (numSamples == SampleCount::e1)
            {
                auto& p0 = m_defaultSrvDesc.Texture2D;
                p0.MipLevels = mipLevels;
                p0.MostDetailedMip = 0;
                p0.PlaneSlice = 0;
                p0.ResourceMinLODClamp = 0.0f;
            }
            break;
        }
        case TextureType::e2DArray:
        {
            if (numSamples == SampleCount::e1)
            {
                auto& p0 = m_defaultSrvDesc.Texture2DArray;
                p0.ArraySize = arrayLayers;
                p0.FirstArraySlice = 0;
                p0.MipLevels = mipLevels;
                p0.MostDetailedMip = 0;
                p0.PlaneSlice = 0;
                p0.ResourceMinLODClamp = 0.0f;
            }
            else
            {
                auto& p0 = m_defaultSrvDesc.Texture2DMSArray;
                p0.ArraySize = arrayLayers;
                p0.FirstArraySlice = 0;
            }
            break;
        }
        case TextureType::e3D:
        {
            auto& p0 = m_defaultSrvDesc.Texture3D;
            p0.MipLevels = mipLevels;
            p0.MostDetailedMip = 0;
            p0.ResourceMinLODClamp = 0.0f;
            break;
        }
        case TextureType::eCube:
        {
            auto& p0 = m_defaultSrvDesc.TextureCube;
            p0.MipLevels = mipLevels;
            p0.MostDetailedMip = 0;
            p0.ResourceMinLODClamp = 0.0f;
            break;
        }
        default:
            QHWI_TODO();
            break;
        }
    }
    if (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        m_defaultDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        m_defaultDsvDesc.Format = d3dFormat;

        switch (type)
        {
        case TextureType::e2D:
        {
            if (numSamples == SampleCount::e1)
            {
                m_defaultDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                auto& p0 = m_defaultDsvDesc.Texture2D;
                p0.MipSlice = 0;
            }
            else
            {
                m_defaultDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
            break;
        }

        case TextureType::e2DArray:
        {
            if (numSamples == SampleCount::e1)
            {
                m_defaultDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                auto& p0 = m_defaultDsvDesc.Texture2DArray;
                p0.MipSlice = 0;
                p0.ArraySize = arrayLayers;
                p0.FirstArraySlice = 0;
            }
            else
            {
                m_defaultDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                auto& p0 = m_defaultDsvDesc.Texture2DMSArray;
                p0.ArraySize = arrayLayers;
                p0.FirstArraySlice = 0;
            }
            break;
        }

        default:
            QHWI_FATAL("Invalid texture type");
            break;
        }
    }
    if (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        m_defaultRtvDesc.Format = d3dFormat;

        switch (type)
        {
        case TextureType::e2D:
        {
            if (numSamples == SampleCount::e1)
            {
                m_defaultRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                auto& p0 = m_defaultRtvDesc.Texture2D;
                p0.MipSlice = 0;
                p0.PlaneSlice = 0;
            }
            else
            {
                m_defaultRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
            break;
        }

        case TextureType::e2DArray:
        case TextureType::eCube:
        {
            if (numSamples == SampleCount::e1)
            {
                m_defaultRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                auto& p0 = m_defaultRtvDesc.Texture2DArray;
                p0.ArraySize = arrayLayers;
                p0.FirstArraySlice = 0;
                p0.MipSlice = 0;
                p0.PlaneSlice = 0;
            }
            else
            {
                m_defaultRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                auto& p0 = m_defaultRtvDesc.Texture2DMSArray;
                p0.ArraySize = arrayLayers;
                p0.FirstArraySlice = 0;
            }
            break;
        }

        default:
            QHWI_FATAL("Invalid texture type");
        }
    }
    if (resourceFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        m_defaultUavDesc.Format = d3dFormat;
        switch (type)
        {
        case TextureType::e1D:
            QRE_ASSERT(numSamples == SampleCount::e1);
            m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            m_defaultUavDesc.Texture1D = { .MipSlice = 0 };
            break;
        case TextureType::e1DArray:
            QRE_ASSERT(numSamples == SampleCount::e1);
            m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            m_defaultUavDesc.Texture1DArray = { .MipSlice = 0, .FirstArraySlice = 0, .ArraySize = arrayLayers };
            break;
        case TextureType::e2D:
            if (numSamples == SampleCount::e1)
            {
                m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                m_defaultUavDesc.Texture2D = { .MipSlice = 0, .PlaneSlice = 0 };
            }
            else
            {
                m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMS;
            }
            break;
        case TextureType::e2DArray:
            if (numSamples == SampleCount::e1)
            {
                m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                m_defaultUavDesc.Texture2DArray = { .MipSlice = 0, .FirstArraySlice = 0, .ArraySize = arrayLayers, .PlaneSlice = 0 };
            }
            else
            {
                m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY;
                m_defaultUavDesc.Texture2DMSArray = {
                    .FirstArraySlice = 0,
                    .ArraySize = arrayLayers,
                };
            }
            break;
        case TextureType::e3D:
            m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            m_defaultUavDesc.Texture3D = { .MipSlice = 0, .FirstWSlice = 0, .WSize = d };
            break;
        case TextureType::eCube:
            m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            m_defaultUavDesc.Texture2DArray = { .MipSlice = 0, .FirstArraySlice = 0, .ArraySize = 6, .PlaneSlice = 0 };
            break;
        case TextureType::eCubeArray:
            if (numSamples == SampleCount::e1)
            {
                m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                m_defaultUavDesc.Texture2DArray = { .MipSlice = 0, .FirstArraySlice = 0, .ArraySize = 6 * arrayLayers, .PlaneSlice = 0 };
            }
            else
            {
                m_defaultUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY;
                m_defaultUavDesc.Texture2DMSArray = { .FirstArraySlice = 0, .ArraySize = 6 * arrayLayers };
            }
            break;
        case TextureType::eUndefined:
            QHWI_ERR("Undefined texture type");
            break;
        }
    }
}

qhwi::D3dTexture::D3dTexture(D3dDevice* device, const Description& desc, ID3D12Resource* resource)
    : Texture(desc.base)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dTexture::D3dTexture", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    init(desc);

    auto d3dDevice = m_device->get();

    if (resource)
    {
        m_resource = resource;
    }
    else
    {
        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON;

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        if (desc.d3d.heapFlags & D3D12_HEAP_FLAG_SHARED)
        {
            allocationDesc.CustomPool = m_device->m_sharedAllocationsPool;
        }

        V(m_device->m_dmaAllocator->CreateResource(&allocationDesc, &m_resourceDesc, initialResourceState, nullptr, &m_allocation, IID_PPV_ARGS(&m_resource)));
    }

    setName(m_resource.Get(), desc.base.name);

    if (desc.d3d.heapFlags & D3D12_HEAP_FLAG_SHARED)
    {
        d3dDevice->CreateSharedHandle(m_resource.Get(), nullptr, GENERIC_ALL, nullptr, &m_sharedHandle);
    }
}

void qhwi::D3dTexture::updateFromHandle(Handle handle) {}

void qhwi::D3dSampler::init(const Sampler::Description& desc, D3D12_SAMPLER_DESC& d3dDesc)
{
    if (desc.maxAnisotropy > 1.0f)
    {
        d3dDesc.Filter = D3D12_FILTER_ANISOTROPIC;
    }
    else
    {
        d3dDesc.Filter = qhwi::d3d(desc.minFilter, desc.magFilter, desc.mipFilter, desc.compareOp);
    }
    d3dDesc.AddressU = qhwi::d3d(desc.addressModeU);
    d3dDesc.AddressV = qhwi::d3d(desc.addressModeV);
    d3dDesc.AddressW = qhwi::d3d(desc.addressModeW);
    d3dDesc.MipLODBias = desc.mipLodBias;
    d3dDesc.MaxAnisotropy = UINT(desc.maxAnisotropy);
    d3dDesc.ComparisonFunc = qhwi::d3d(desc.compareOp);
    d3dDesc.BorderColor[0] = 0.0f;
    d3dDesc.BorderColor[1] = 0.0f;
    d3dDesc.BorderColor[2] = 0.0f;
    d3dDesc.BorderColor[3] = 0.0f;
    d3dDesc.MinLOD = desc.minLod;
    d3dDesc.MaxLOD = desc.maxLod;
}

qhwi::D3dSampler::D3dSampler(D3dDevice* device, const Description& desc)
    : m_device(device)
{
    auto d3dDevice = m_device->get();
    auto descriptorHeap = m_device->getSamplerDescriptorHeap();
    std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
    QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

    D3D12_SAMPLER_DESC d3dDesc;
    init(desc, d3dDesc);
    d3dDevice->CreateSampler(&d3dDesc, descriptorHandle->cpuHandle);

    m_descriptorHandle.reset(descriptorHeap, *descriptorHandle);

    auto shaderVisibleDescriptorHeap = m_device->getSamplerShaderVisibleDescriptorHeap();
    std::optional<D3dDescriptorHandle> shaderVisibleDescriptorHandle = shaderVisibleDescriptorHeap->stage(*descriptorHandle);
    QHWI_ASSERT_MSG(shaderVisibleDescriptorHandle.has_value(), "Invalid descriptor handle");
    m_shaderVisibleDescriptorHandle.reset(shaderVisibleDescriptorHeap, *shaderVisibleDescriptorHandle);
}

qhwi::D3dShader::D3dShader(D3dDevice*, const Description& desc, const void* byteCode, size_t length)
{
    QHWI_ASSERT(byteCode);
    QHWI_ASSERT(length);
    m_desc = desc;
    m_shaderByteCodeStorage = new uint8_t[length];
    memcpy(m_shaderByteCodeStorage, byteCode, length);

    m_shaderByteCode.pShaderBytecode = m_shaderByteCodeStorage;
    m_shaderByteCode.BytecodeLength = length;
}

D3D12_SHADER_BYTECODE qhwi::d3d(Shader* shader)
{
    return shader ? static_cast<D3dShader*>(shader)->byteCode() : D3D12_SHADER_BYTECODE{};
}

D3D12_INDEX_BUFFER_STRIP_CUT_VALUE qhwi::d3d(PrimitiveRestartValue v)
{
    switch (v)
    {
    case PrimitiveRestartValue::eFFFF:
        return D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
    case PrimitiveRestartValue::eFFFFFFFF:
        return D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
    default:
        QHWI_FATAL("Invalid primitive restart value");
    }
}

D3D12_BLEND_DESC qhwi::d3d(const BlendState& blendState, const MultisampleState& multisampleState)
{
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.IndependentBlendEnable = TRUE;
    blendDesc.AlphaToCoverageEnable = multisampleState.enableAlphaToCoverage;
    for (uint32_t j = 0; j < blendState.numColorAttachments(); j++)
    {
        blendDesc.RenderTarget[j] = d3d(blendState.colorAttachmentBlendStates[j]);
    }
    return blendDesc;
}

D3D12_RENDER_TARGET_BLEND_DESC qhwi::d3d(const ColorAttachmentBlendState& s)
{
    D3D12_RENDER_TARGET_BLEND_DESC desc;
    desc.BlendEnable = s.enableBlend;
    desc.BlendOp = d3d(s.colorBlendOp);
    desc.BlendOpAlpha = d3d(s.alphaBlendOp);
    desc.DestBlend = d3d(s.dstColorBlendFactor);
    desc.DestBlendAlpha = d3d(s.dstAlphaBlendFactor);
    desc.RenderTargetWriteMask = s.colorWriteMask;
    desc.SrcBlend = d3d(s.srcColorBlendFactor);
    desc.SrcBlendAlpha = d3d(s.srcAlphaBlendFactor);
    desc.LogicOpEnable = s.enableLogicOp;
    desc.LogicOp = d3d(s.logicOp);
    return desc;
}

D3D12_DEPTH_STENCIL_DESC qhwi::d3d(const DepthStencilState& s)
{
    D3D12_DEPTH_STENCIL_DESC desc;
    desc.DepthEnable = s.enableDepthTest;
    desc.DepthWriteMask = s.enableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = d3d(s.depthCompareOp);
    desc.StencilEnable = s.enableStencilTest;
    desc.StencilReadMask = s.stencilReadMask;
    desc.StencilWriteMask = s.stencilWriteMask;
    desc.FrontFace = d3d(s.frontFace);
    desc.BackFace = d3d(s.backFace);
    return desc;
}

D3D12_DEPTH_STENCILOP_DESC qhwi::d3d(const StencilOpState& s)
{
    D3D12_DEPTH_STENCILOP_DESC desc;
    desc.StencilFailOp = d3d(s.failOp);
    desc.StencilDepthFailOp = d3d(s.depthFailOp);
    desc.StencilPassOp = d3d(s.passOp);
    desc.StencilFunc = d3d(s.compareOp);
    return desc;
}

D3D12_RASTERIZER_DESC qhwi::d3d(const RasterizerState& s, const MultisampleState& multisampleState)
{
    D3D12_RASTERIZER_DESC desc;
    desc.FillMode = d3d(s.fillMode);
    desc.CullMode = d3d(s.cullMode);
    desc.FrontCounterClockwise = s.frontFaceCounterClockwise;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.DepthClipEnable = s.enableDepthClipping;
    desc.MultisampleEnable = multisampleState.sampleCount != SampleCount::e1;
    desc.AntialiasedLineEnable = FALSE;
    desc.ForcedSampleCount = 0;
    desc.ConservativeRaster = s.enableConservativeRasterization ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return desc;
}

void qhwi::d3d(const VertexInputState& s, D3dInputLayoutDesc& r)
{
    r.desc.NumElements = UINT(s.vertexInputAttributeDescriptions.size());
    for (size_t j = 0; j < s.vertexInputAttributeDescriptions.size(); j++)
    {
        const auto& vertexInputAttrDesc = s.vertexInputAttributeDescriptions[j];
        D3D12_INPUT_CLASSIFICATION inputClassification = qhwi::d3d(vertexInputAttrDesc.inputRate);
        UINT instanceDataStepRate = (inputClassification == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA) ? 0 : 1;
        r.inputElementDescriptions[j] = { vertexInputAttrDesc.semanticName,
                                          vertexInputAttrDesc.semanticIndex,
                                          qhwi::d3d(vertexInputAttrDesc.format),
                                          vertexInputAttrDesc.binding,
                                          vertexInputAttrDesc.offset,
                                          inputClassification,
                                          instanceDataStepRate };
    }
    r.desc.pInputElementDescs = (r.desc.NumElements == 0) ? nullptr : r.inputElementDescriptions;
}

void qhwi::D3dRootSignature::init(D3dDevice* device, const Description& desc)
{
    m_device = device;
    auto d3dDevice = m_device->get();
    ComPtr<ID3DBlob> error, signature;
    ExportHeader* pExportHeader = nullptr;

    if (desc.pD3DInternalExportHeader)
    {
        pExportHeader = static_cast<ExportHeader*>(desc.pD3DInternalExportHeader);
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(desc.numRootParams, desc.rootParams, desc.numStaticSamplers, desc.staticSamplers, desc.rootSignatureFlags);
    V(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));

    if (pExportHeader)
    {
        pExportHeader->ExportRootSignature_1_1(rootSignatureDesc);
    }

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    V(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void qhwi::D3dPipelineLayout::init(const Description& desc)
{
    ExportHeader* pExportHeader = nullptr;
    D3dRootSignature::Description rootSignatureDesc;
    rootSignatureDesc.rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    uint32_t bindingIndex = 0;
    size_t numDescriptorSetLayouts = desc.descriptorSetLayouts.size();
    size_t numPushConstantRanges = desc.pushConstantRanges.size();
    uint32_t numRootParams = 0;
    uint32_t numStaticSamplers = 0;
    //TODO: support static samplers
    auto rootParamsPtr = rootSignatureDesc.rootParams;
    auto descriptorRangesPtr = rootSignatureDesc.descriptorRanges;
    //TODO: support multiple register spaces
    uint32_t rootParameterIndex = 0;
    if (numDescriptorSetLayouts > 0)
    {
        QHWI_ASSERT(!desc.descriptorSetLayouts.empty());
    }
    m_descriptorSetLayoutInfos.resize(numDescriptorSetLayouts);

    if (desc.pD3DInternalExportHeader != nullptr)
    {
        pExportHeader = static_cast<ExportHeader*>(desc.pD3DInternalExportHeader);
        pExportHeader->WriteFileHeader();
    }

    for (uint32_t descriptorSetLayoutIndex = 0; descriptorSetLayoutIndex < numDescriptorSetLayouts; descriptorSetLayoutIndex++)
    {
        QHWI_ASSERT(desc.descriptorSetLayouts[descriptorSetLayoutIndex]);
        const auto* descriptorSetLayout = desc.descriptorSetLayouts[descriptorSetLayoutIndex].Get();
        const auto* descriptorSetLayoutDesc = descriptorSetLayout->desc();
        UINT registerSpace = descriptorSetLayoutDesc->registerSpace;
        m_descriptorSetLayoutInfos[descriptorSetLayoutIndex] = { .p = descriptorSetLayout, .offset = rootParameterIndex };
        for (bindingIndex = 0; bindingIndex < descriptorSetLayoutDesc->bindings.size(); bindingIndex++)
        {
            auto& descriptorSetLayoutBinding = descriptorSetLayoutDesc->bindings[bindingIndex];
            auto descriptorType = descriptorSetLayoutBinding.type;
            uint32_t descriptorCount = descriptorSetLayoutBinding.count;
            UINT shaderRegister = descriptorSetLayoutBinding.shaderRegister;
            D3D12_SHADER_VISIBILITY shaderVisibility = qhwi::d3dShaderVisibility(descriptorSetLayoutBinding.shaderStages);
            if (descriptorType == DescriptorType::eSampler)
            {
                auto& rootParam = *rootParamsPtr++;
                rootParameterIndex++;
                CD3DX12_ROOT_PARAMETER1 p;
                CD3DX12_DESCRIPTOR_RANGE1 range;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, descriptorCount, shaderRegister, registerSpace, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                *descriptorRangesPtr = range;
                p.InitAsDescriptorTable(1, descriptorRangesPtr, shaderVisibility);
                rootParam = p;
                descriptorRangesPtr++;
                numRootParams++;

                if (pExportHeader)
                {
                    pExportHeader->ExportDescriptorRange1(bindingIndex, range);
                    pExportHeader->ExportRootParamAsDescriptorTable(bindingIndex, p);
                }
            }
            else if (descriptorType == DescriptorType::eConstantBuffer || descriptorType == DescriptorType::eDynamicConstantBuffer)
            {
                for (uint32_t descriptorIndex = 0; descriptorIndex < descriptorCount; descriptorIndex++)
                {
                    CD3DX12_ROOT_PARAMETER1 p;
                    auto& rootParam = *rootParamsPtr++;
                    rootParameterIndex++;
                    p.InitAsConstantBufferView(shaderRegister + descriptorIndex, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, shaderVisibility);
                    rootParam = p;
                    numRootParams++;

                    if (pExportHeader)
                    {
                        pExportHeader->ExportRootParamAsConstantBufferView(bindingIndex, p);
                    }
                }
            }
            else if (descriptorType == DescriptorType::eTexture || descriptorType == DescriptorType::eInputAttachment)
            {
                auto& rootParam = *rootParamsPtr++;
                rootParameterIndex++;
                CD3DX12_ROOT_PARAMETER1 p;
                CD3DX12_DESCRIPTOR_RANGE1 range;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptorCount, shaderRegister, registerSpace, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                *descriptorRangesPtr = range;
                p.InitAsDescriptorTable(1, descriptorRangesPtr, shaderVisibility);
                rootParam = p;
                descriptorRangesPtr++;
                numRootParams++;

                if (pExportHeader)
                {
                    pExportHeader->ExportDescriptorRange1(bindingIndex, range);
                    pExportHeader->ExportRootParamAsDescriptorTable(bindingIndex, p);
                }
            }
            else if (descriptorType == DescriptorType::eStructuredBuffer || descriptorType == DescriptorType::eByteAddressBuffer || descriptorType == DescriptorType::eTypedBuffer)
            {
                auto& rootParam = *rootParamsPtr++;
                rootParameterIndex++;
                CD3DX12_ROOT_PARAMETER1 p;
                CD3DX12_DESCRIPTOR_RANGE1 range;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptorCount, shaderRegister, registerSpace, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                *descriptorRangesPtr = range;
                p.InitAsDescriptorTable(1, descriptorRangesPtr, shaderVisibility);
                rootParam = p;
                descriptorRangesPtr++;
                numRootParams++;

                if (pExportHeader)
                {
                    pExportHeader->ExportDescriptorRange1(bindingIndex, range);
                    pExportHeader->ExportRootParamAsDescriptorTable(bindingIndex, p);
                }
            }
            else if (descriptorType == DescriptorType::eRWStructuredBuffer || descriptorType == DescriptorType::eRWByteAddressBuffer ||
                     descriptorType == DescriptorType::eRWTypedBuffer)
            {
                auto& rootParam = *rootParamsPtr++;
                rootParameterIndex++;
                CD3DX12_ROOT_PARAMETER1 p;
                CD3DX12_DESCRIPTOR_RANGE1 range;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, descriptorCount, shaderRegister, registerSpace, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                *descriptorRangesPtr = range;
                p.InitAsDescriptorTable(1, descriptorRangesPtr, shaderVisibility);
                rootParam = p;
                descriptorRangesPtr++;
                numRootParams++;

                if (pExportHeader)
                {
                    pExportHeader->ExportDescriptorRange1(bindingIndex, range);
                    pExportHeader->ExportRootParamAsDescriptorTable(bindingIndex, p);
                }
            }
            else if (descriptorType == DescriptorType::eAccelerationStructure)
            {
                for (uint32_t descriptorIndex = 0; descriptorIndex < descriptorCount; descriptorIndex++)
                {
                    auto& rootParam = *rootParamsPtr++;
                    rootParameterIndex++;
                    CD3DX12_ROOT_PARAMETER1 p;
                    p.InitAsShaderResourceView(shaderRegister + descriptorIndex, registerSpace, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, shaderVisibility);
                    rootParam = p;
                    numRootParams++;
                }
            }
            else if (descriptorType == DescriptorType::eRWTexture)
            {
                auto& rootParam = *rootParamsPtr++;
                rootParameterIndex++;
                CD3DX12_ROOT_PARAMETER1 p;
                CD3DX12_DESCRIPTOR_RANGE1 range;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, descriptorCount, shaderRegister, registerSpace, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                *descriptorRangesPtr = range;
                p.InitAsDescriptorTable(1, descriptorRangesPtr, shaderVisibility);
                rootParam = p;
                descriptorRangesPtr++;
                numRootParams++;

                if (pExportHeader)
                {
                    pExportHeader->ExportDescriptorRange1(bindingIndex, range);
                    pExportHeader->ExportRootParamAsDescriptorTable(bindingIndex, p);
                }
            }
            else
            {
                QHWI_TODO();
            }
        }
    }

    m_pushConstantInfos.resize(numPushConstantRanges);
    for (uint32_t j = 0; j < numPushConstantRanges; j++)
    {
        const auto& pushConstantRange = desc.pushConstantRanges[j];
        CD3DX12_ROOT_PARAMETER1 p;
        uint32_t num32BitValues = pushConstantRange.size / 4;
        D3D12_SHADER_VISIBILITY shaderVisibility = qhwi::d3dShaderVisibility(pushConstantRange.shaderStages);
        UINT shaderRegister = pushConstantRange.shaderRegister;
        UINT registerSpace = pushConstantRange.registerSpace;
        m_pushConstantInfos[j] = { .offset = rootParameterIndex, .num32BitValues = num32BitValues };
        auto& rootParam = *rootParamsPtr++;
        rootParameterIndex++;
        p.InitAsConstants(num32BitValues, shaderRegister, registerSpace, shaderVisibility);
        rootParam = p;
        numRootParams++;

        if (pExportHeader)
        {
            pExportHeader->ExportRootParamAsConstants(bindingIndex, p);
        }
    }

    rootSignatureDesc.numRootParams = numRootParams;
    rootSignatureDesc.numStaticSamplers = numStaticSamplers;
    rootSignatureDesc.pD3DInternalExportHeader = pExportHeader;

    m_rootSignature.init(m_device, rootSignatureDesc);
    setName(m_rootSignature.get(), desc.name);
}

qhwi::D3dPipelineLayout::D3dPipelineLayout(D3dDevice* device, const Description& desc)
    : PipelineLayout(desc)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dPipelineLayout::D3dPipelineLayout", QHWI_COLOR_RESOURCE_CREATE);
    init(desc);
}

qhwi::D3dGraphicsPipeline::Description::Description(const GraphicsPipeline::Description& desc)
    : base(desc)
{
    auto& pipelineStateStream = d3d.pipelineStateStream;

    pipelineStateStream = {};

    QHWI_ASSERT(base.layout);

    pipelineStateStream.pRootSignature = CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE(static_cast<D3dPipelineLayout*>(base.layout.Get())->rootSignature()->get());

    pipelineStateStream.VS = qhwi::d3d(base.vertexShader.Get());
    pipelineStateStream.PS = qhwi::d3d(base.pixelShader.Get());
    pipelineStateStream.DS = qhwi::d3d(base.domainShader.Get());
    pipelineStateStream.HS = qhwi::d3d(base.hullShader.Get());
    pipelineStateStream.GS = qhwi::d3d(base.geometryShader.Get());
    pipelineStateStream.AS = qhwi::d3d(base.amplificationShader.Get());
    pipelineStateStream.MS = qhwi::d3d(base.meshShader.Get());

    if (base.blendState.colorAttachmentBlendStates.empty())
    {
        base.blendState.colorAttachmentBlendStates.resize(base.numColorAttachments(), {});
    }

    qhwi::d3d(base.vertexInputState, d3d.inputLayoutDesc);

    pipelineStateStream.BlendState = CD3DX12_BLEND_DESC(qhwi::d3d(base.blendState, base.multisampleState));
    pipelineStateStream.RasterizerState = CD3DX12_RASTERIZER_DESC(qhwi::d3d(base.rasterizerState, base.multisampleState));
    pipelineStateStream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(qhwi::d3d(base.depthStencilState));
    pipelineStateStream.InputLayout = d3d.inputLayoutDesc.desc;
    pipelineStateStream.PrimitiveTopologyType = qhwi::d3dPrimitiveTopologyType(base.primitiveTopology);
    pipelineStateStream.IBStripCutValue = qhwi::d3d(base.primitiveRestartValue);

    D3D12_RT_FORMAT_ARRAY rtvFormatArray{};
    const auto& colorAttachmentRemap = base.colorAttachmentRemapping;
    uint32_t numRemaps = uint32_t(colorAttachmentRemap.size());
    rtvFormatArray.NumRenderTargets = 0;
    for (uint32_t j = 0; j < base.numColorAttachments(); j++)
    {
        uint32_t location = (numRemaps > j) ? colorAttachmentRemap[j] : j;
        if (location == AttachmentUnused)
        {
            continue;
        }
        rtvFormatArray.RTFormats[location] = qhwi::d3d(base.colorAttachmentFormats[j]);
        rtvFormatArray.NumRenderTargets++;
    }

    pipelineStateStream.RTVFormats = rtvFormatArray;
    pipelineStateStream.DSVFormat = qhwi::d3d(base.depthStencilFormat);
    pipelineStateStream.SampleMask = UINT_MAX;
    pipelineStateStream.SampleDesc = { UINT(base.multisampleState.sampleCount), 0 };

    uint32_t numViews = base.multiViewState.numViews;
    if (numViews != 0)
    {
        for (uint32_t j = 0; j < numViews; j++)
        {
            // TODO: support per-view viewports
            d3d.viewInstanceLocations[j] = { .ViewportArrayIndex = 0, .RenderTargetArrayIndex = j };
        }

        D3D12_VIEW_INSTANCING_FLAGS flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
        pipelineStateStream.ViewInstancingDesc = CD3DX12_VIEW_INSTANCING_DESC(2, d3d.viewInstanceLocations, flags);
    }
}

qhwi::D3dGraphicsPipeline::D3dGraphicsPipeline(D3dDevice* device, const Description& desc)
    : m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dGraphicsPipeline::D3dGraphicsPipeline", QHWI_COLOR_RESOURCE_CREATE);
    m_layout = desc.base.layout;
    m_primitiveTopology = qhwi::d3d(desc.base.primitiveTopology);
    m_enableStencilTest = desc.base.depthStencilState.enableStencilTest;
    m_stencilRef = desc.base.depthStencilState.stencilReference;

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
    streamDesc.pPipelineStateSubobjectStream = (void*)&desc.d3d.pipelineStateStream;
    streamDesc.SizeInBytes = sizeof(desc.d3d.pipelineStateStream);

    V(m_device->get()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_pipelineState)));

    setName(m_pipelineState.Get(), desc.base.name);
}

qhwi::D3dComputePipeline::Description::Description(const ComputePipeline::Description& desc)
    : base(desc)
{
    auto& cpDesc = d3d.stateDesc;
    cpDesc = {};
    cpDesc.CS = qhwi::d3d(base.computeShader.Get());
}

qhwi::D3dComputePipeline::D3dComputePipeline(D3dDevice* device, const Description& desc)
    : m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dComputePipeline::D3dComputePipeline", QHWI_COLOR_RESOURCE_CREATE);
    auto d3dDevice = m_device->get();
    m_layout = desc.base.layout;
    auto cpDesc = desc.d3d.stateDesc;
    cpDesc.pRootSignature = getRootSignature()->get();

    if (desc.base.pD3DInternalExportHeader)
    {
        auto pExportHeader = static_cast<ExportHeader*>(desc.base.pD3DInternalExportHeader);

        pExportHeader->ExportDxil(cpDesc.CS.pShaderBytecode, static_cast<UINT>(cpDesc.CS.BytecodeLength));
        pExportHeader->WriteFileFooter();
    }

    V(d3dDevice->CreateComputePipelineState(&cpDesc, IID_PPV_ARGS(&m_pipelineState)));

    setName(m_pipelineState.Get(), desc.base.name);
}

qhwi::D3dRayTracingPipeline::D3dRayTracingPipeline(D3dDevice* device, const Description& desc)
    : m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dRayTracingPipeline::D3dRayTracingPipeline", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(desc.base.shaders.size() > 0 && desc.base.shaderGroups.size() > 0);

    m_layout = desc.base.layout;

    CD3DX12_STATE_OBJECT_DESC rayTracingDesc = CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Add shaders to ray tracing pipeline
    for (const auto& shader : desc.base.shaders)
    {
        const D3dShader* d3dShader = static_cast<const D3dShader*>(shader);
        D3D12_SHADER_BYTECODE byteCode = d3dShader->byteCode();
        WString entryPointName;
        qre::CharToWideChar(entryPointName, shader->desc()->entryPointName);

        CD3DX12_DXIL_LIBRARY_SUBOBJECT* dxilLibrarySubobject = rayTracingDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

        dxilLibrarySubobject->SetDXILLibrary(&byteCode);
        dxilLibrarySubobject->DefineExport(entryPointName);
    }

    // Add ray pipeline config subobject
    CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* rayPipelineConfig = rayTracingDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    rayPipelineConfig->Config(desc.base.maxRayRecursionDepth);

    // Configure hit groups
    WString shaderGroupNames[MaxRayPipelineShaderGroups];
    uint32_t numShaderGroupNames = 0;

    for (const RayTracingPipelineShaderGroup& shaderGroup : desc.base.shaderGroups)
    {
        bool isHitShaderGroup = (shaderGroup.type == RayTracingShaderGroupType::eTriangleHit) || (shaderGroup.type == RayTracingShaderGroupType::eProceduralHit);

        if (isHitShaderGroup)
        {
            CD3DX12_HIT_GROUP_SUBOBJECT* hitGroupSubobject = rayTracingDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            D3D12_HIT_GROUP_TYPE d3dHitGroupType =
                (shaderGroup.type == RayTracingShaderGroupType::eTriangleHit) ? D3D12_HIT_GROUP_TYPE_TRIANGLES : D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;

            hitGroupSubobject->SetHitGroupType(d3dHitGroupType);

            if (shaderGroup.anyHitShader != ShaderUnused)
            {
                WString entryPointName;
                qre::CharToWideChar(entryPointName, desc.base.shaders[shaderGroup.anyHitShader]->desc()->entryPointName);
                hitGroupSubobject->SetAnyHitShaderImport(entryPointName);
            }

            if (shaderGroup.closestHitShader != ShaderUnused)
            {
                WString entryPointName;
                qre::CharToWideChar(entryPointName, desc.base.shaders[shaderGroup.closestHitShader]->desc()->entryPointName);
                hitGroupSubobject->SetClosestHitShaderImport(entryPointName);
            }

            if (shaderGroup.intersectionShader != ShaderUnused)
            {
                WString entryPointName;
                qre::CharToWideChar(entryPointName, desc.base.shaders[shaderGroup.intersectionShader]->desc()->entryPointName);
                hitGroupSubobject->SetIntersectionShaderImport(entryPointName);
            }

            qre::CharToWideChar(shaderGroupNames[numShaderGroupNames], shaderGroup.name);
            hitGroupSubobject->SetHitGroupExport(shaderGroupNames[numShaderGroupNames++]);
        }
        else
        {
            QHWI_ASSERT(shaderGroup.generalShader != ShaderUnused);
            qre::CharToWideChar(shaderGroupNames[numShaderGroupNames++], desc.base.shaders[shaderGroup.generalShader]->desc()->entryPointName);
            continue;
        }
    }

    // Create shader config subobject
    CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shaderConfig = rayTracingDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfig->Config(desc.base.maxPayloadSize, desc.base.maxAttributeSize);

    // Set global root signature
    CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* rootSignature = rayTracingDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSignature->SetRootSignature(static_cast<D3dPipelineLayout*>(desc.base.layout.Get())->rootSignature()->get());

    m_device->get()->CreateStateObject(rayTracingDesc, IID_PPV_ARGS(&m_rayTracingPipelineStateObject));
    setName(m_rayTracingPipelineStateObject.Get(), desc.base.name);

    // Cache shader identifiers
    ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
    m_rayTracingPipelineStateObject.As(&stateObjectProperties);

    for (uint32_t shaderGroupIndex = 0; shaderGroupIndex < numShaderGroupNames; ++shaderGroupIndex)
    {
        m_shaderIdentifiers.push_back(stateObjectProperties->GetShaderIdentifier(shaderGroupNames[shaderGroupIndex]));
    }
}

qhwi::ResultCode qhwi::D3dRayTracingPipeline::getRayTracingShaderGroupHandles(uint32_t startGroup, uint32_t groupCount, size_t dstSize, void* dst) const
{
    QHWI_ASSERT(m_device->getRayTracingPipelineProperties().shaderGroupHandleSize == D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    bool isGroupHandleAvailable =
        (groupCount > 0) && (startGroup + groupCount <= m_shaderIdentifiers.size()) && (D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES * groupCount <= dstSize) && (dst != nullptr);

    if (isGroupHandleAvailable)
    {
        uint8_t* dstPtr = static_cast<uint8_t*>(dst);

        for (uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
        {
            memcpy(dstPtr + groupIndex * D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, m_shaderIdentifiers[groupIndex + startGroup], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        }

        return ResultCode::eSuccess;
    }
    else
    {
        return ResultCode::eErrorOther;
    }
}

qhwi::D3dBuffer::Description::Description(const Buffer::Description& desc)
    : base(desc)
{
    d3d.heapFlags = qhwi::d3dHeapFlags(desc.heapFlags);
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    if (base.usage & BufferUsage_UnorderedAccess)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (base.usage & BufferUsage_AccelerationStructureBuildSource || base.usage & BufferUsage_AccelerationStructure)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
    }
    D3D12_HEAP_TYPE heapType = qhwi::d3d(base.heapType);
    d3d.heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
    d3d.resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(base.size, resourceFlags);

    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
    {
        d3d.initialResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
    else if (heapType == D3D12_HEAP_TYPE_READBACK)
    {
        d3d.initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    else
    {
        d3d.initialResourceState = D3D12_RESOURCE_STATE_COMMON;
    }

    if (base.usage & BufferUsage_AccelerationStructure)
    {
        d3d.initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    if (base.usage & (BufferUsage_IndirectBuffer | BufferUsage_AccelerationStructure))
    {
        d3d.resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
}

qhwi::D3dBuffer::~D3dBuffer()
{
    m_device->onDestroyResource(m_resource.Get());
    if (m_allocation)
    {
        m_allocation->Release();
    }
}

qhwi::D3dBuffer::D3dBuffer(D3dDevice* device, const Description& desc)
    : Buffer(desc.base)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dBuffer::D3dBuffer", QHWI_COLOR_RESOURCE_CREATE);
    auto d3dDevice = m_device->get();

    D3D12MA::ALLOCATION_DESC allocationDesc = {};

    allocationDesc.HeapType = desc.d3d.heapProperties.Type;

    if (desc.d3d.heapFlags & D3D12_HEAP_FLAG_SHARED)
    {
        allocationDesc.CustomPool = m_device->m_sharedAllocationsPool;
    }
    V(m_device->m_dmaAllocator->CreateResource(&allocationDesc, &desc.d3d.resourceDesc, desc.d3d.initialResourceState, nullptr, &m_allocation, IID_PPV_ARGS(&m_resource)));

    setName(m_resource.Get(), desc.base.name);

    m_deviceAddress = m_resource->GetGPUVirtualAddress();

    if (desc.d3d.heapFlags & D3D12_HEAP_FLAG_SHARED)
    {
        d3dDevice->CreateSharedHandle(m_resource.Get(), nullptr, GENERIC_ALL, nullptr, &m_sharedHandle);
    }
}

void* qhwi::D3dBuffer::map(uint64_t offset, uint64_t size)
{
    //TODO: pass read/write flags
    UINT8* ptr;
    V(m_resource->Map(0, nullptr, reinterpret_cast<void**>(&ptr)));
    m_mapAddr = ptr + offset;
    return m_mapAddr;
}

void qhwi::D3dBuffer::unmap()
{
    m_resource->Unmap(0, nullptr);
}

qhwi::D3dDescriptorSet::Description::Description(const DescriptorSet::Description& desc)
    : base(desc)
{
    QHWI_ASSERT(base.layout);
    base.reorderBindings();
}

qhwi::D3dDescriptorSet::D3dDescriptorSet(D3dDevice* device, const DescriptorSet::Description& desc)
    : m_device(device)
    , m_desc(desc)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dDescriptorSet::D3dDescriptorSet", QHWI_COLOR_RESOURCE_CREATE);
    if (desc.variableDescriptorCount)
    {
        return;
    }
}

qhwi::D3dDescriptorSetLayout::Description::Description(const qhwi::DescriptorSetLayout::Description& desc)
    : base(desc)
{
    auto& bindings = base.bindings;
    std::sort(bindings.begin(), bindings.end());
}

qhwi::D3dDescriptorSetLayout::D3dDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
    : m_desc(desc)
{
}

qhwi::D3dAccelerationStructure::Description::Description(const AccelerationStructure::Description& desc)
    : base(desc)
{
}

qhwi::D3dAccelerationStructure::D3dAccelerationStructure(D3dDevice* device, const Description& desc)
    : m_desc(desc)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dAccelerationStructure::D3dAccelerationStructure", QHWI_COLOR_RESOURCE_CREATE);
    Buffer::Description bufferDesc;
    bufferDesc.heapType = HeapType::eDefault;
    bufferDesc.usage = BufferUsage_AccelerationStructure, bufferDesc.size = desc.base.allocationSize, bufferDesc.name = "AccelerationStructure";

    D3dBuffer::Description d3dBufferDesc(bufferDesc);
    m_buffer = new D3dBuffer(m_device, d3dBufferDesc);

    setName(m_buffer->get(), desc.base.name);
}

void qhwi::D3dCommandList::begin()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::begin", QHWI_COLOR_COMMAND_LIST);
    ResetEvent(m_event);

    onBegin();
    V(m_commandAllocator->Reset());
    V(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
    uint32_t numDescriptorHeaps = 2;
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_device->getCbvSrvUavShaderVisibleDescriptorHeap()->get(), m_device->getSamplerShaderVisibleDescriptorHeap()->get() };
    m_commandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}

void qhwi::D3dCommandList::end()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::end", QHWI_COLOR_COMMAND_LIST);
    if (!m_device->supportsEnhancedBarriers())
    {
        for (const auto& b : m_bufferState)
        {
            setState(b.first, BufferState_NoAccess);
        }
        for (const auto& b : m_textureState)
        {
            setState(b.first, TextureState_NoAccess);
        }
    }
    V(m_commandList->Close());
}

void qhwi::D3dCommandList::beginRenderPass(uint32_t numColorAttachments,
                                           const ColorAttachmentDescription* colorAttachmentDescs,
                                           const DepthStencilAttachmentDescription* depthStencilAttachmentDesc,
                                           const MultiViewState* multiViewState,
                                           const Rect2D* multiViewPerViewRenderAreasQCOM,
                                           const uint32_t* colorAttachmentRemapping,
                                           const TileShadingInfoQCOM*,
                                           bool /*customResolve*/)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::beginRenderPass", QHWI_COLOR_COMMAND_LIST);
    QHWI_ASSERT(m_deferState == DeferState::eOff);
    ColorAttachmentDescription colorAttachmentDescsRemap[MaxColorAttachments];
    if (colorAttachmentRemapping)
    {
        uint32_t numUsedColorAttachments = 0;
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            const auto& colorAttachmentDesc = colorAttachmentDescs[j];
            uint32_t location;
            if (colorAttachmentDesc.isInputAttachment())
            {
                // in D3d, the input attachments are specified as render targets
                location = j;
            }
            else
            {
                location = colorAttachmentRemapping[j];
                if (location == AttachmentUnused)
                {
                    continue;
                }
            }
            colorAttachmentDescsRemap[location] = colorAttachmentDesc;
            numUsedColorAttachments++;
        }
        numColorAttachments = numUsedColorAttachments;
        colorAttachmentDescs = colorAttachmentDescsRemap;
    }
    if (numColorAttachments == 0)
    {
        colorAttachmentDescs = nullptr;
    }
    QHWI_ASSERT(colorAttachmentDescs != nullptr || (numColorAttachments == 0 && depthStencilAttachmentDesc != nullptr));

    for (uint32_t j = 0; j < numColorAttachments; j++)
    {
        const auto& colorAttachment = colorAttachmentDescs[j];
        auto colorTexture = colorAttachment.textureData.texture;
        auto resolveTexture = colorAttachment.resolveTarget.texture;

        markResourceAsUsed(colorTexture);
        if (colorTexture->automaticBarriersEnabled())
        {
            setState(colorTexture, TextureState_ColorAttachment, colorAttachment.textureData.getSubresourceRange());
        }
        if (resolveTexture)
        {
            markResourceAsUsed(resolveTexture);
            if (resolveTexture->automaticBarriersEnabled())
            {
                setState(resolveTexture, TextureState_ColorResolveDest, colorAttachment.textureData.getSubresourceRange());
            }
        }
    }
    if (depthStencilAttachmentDesc)
    {
        auto depthTexture = depthStencilAttachmentDesc->textureData.texture;
        markResourceAsUsed(depthTexture);
        if (depthTexture->automaticBarriersEnabled())
        {
            setState(depthTexture, TextureState_DepthStencilAttachment, depthStencilAttachmentDesc->textureData.getSubresourceRange());
        }
    }

    if (m_desc.base.supportsAutomaticBarriers)
    {
        m_deferState = DeferState::eRecord;
        m_rp.beginRecording(this);
        m_rp.beginRenderPass(numColorAttachments, colorAttachmentDescs, depthStencilAttachmentDesc);
        return;
    }
    beginRenderPass1(numColorAttachments, colorAttachmentDescs, depthStencilAttachmentDesc);
}

void qhwi::D3dCommandList::beginProfilingPass()
{
    if (m_scheduledTimerQuery)
    {
        D3dTimerQueryHeap* d3dTimerQueryHeap = m_device->getTimerQueryHeap();
        m_commandList->EndQuery(d3dTimerQueryHeap->get(), D3D12_QUERY_TYPE_TIMESTAMP, m_scheduledTimerQuery->beginQueryIndex);
    }
}

void qhwi::D3dCommandList::endProfilingPass()
{
    if (m_scheduledTimerQuery)
    {
        D3dTimerQueryHeap* d3dTimerQueryHeap = m_device->getTimerQueryHeap();
        m_commandList->EndQuery(d3dTimerQueryHeap->get(), D3D12_QUERY_TYPE_TIMESTAMP, m_scheduledTimerQuery->endQueryIndex);
        m_scheduledTimerQuery = nullptr;
    }
}

void qhwi::D3dCommandList::beginRenderPass1(uint32_t numColorAttachments,
                                            const ColorAttachmentDescription* colorAttachmentDescs,
                                            const DepthStencilAttachmentDescription* depthStencilAttachmentDesc)
{
    D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDescs[MaxColorAttachments]{};
    uint32_t numResolveAttachments = 0;

    for (uint32_t j = 0; j < numColorAttachments; j++)
    {
        D3D12_RENDER_PASS_RENDER_TARGET_DESC& r = rtDescs[j];
        const ColorAttachmentDescription& desc = colorAttachmentDescs[j];
        QHWI_ASSERT(desc.textureData.texture);

        auto d3dTexture = static_cast<D3dTexture*>(desc.textureData.texture);
        DXGI_FORMAT colorAttachmentFormat = d3dTexture->resourceDesc().Format;
        SubresourceRange subresourceRange = desc.textureData.getSubresourceRange();
        subresourceRange.clamp(d3dTexture);
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = qhwi::d3dRenderTargetViewDesc(d3dTexture, subresourceRange);

        r.cpuDescriptor = m_device->getRtvDescriptor(d3dTexture->get(), &rtvDesc).cpuHandle;
        r.BeginningAccess.Type = qhwi::d3d(desc.loadOp);
        if (r.BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
        {
            r.BeginningAccess.Clear.ClearValue = CD3DX12_CLEAR_VALUE(colorAttachmentFormat, desc.clearValue.rgba.data());
        }
        if (desc.resolveTarget.texture != nullptr)
        {

            auto resolveTexture = static_cast<D3dTexture*>(desc.resolveTarget.texture);
            r.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
            r.EndingAccess.Resolve.pSrcResource = d3dTexture->get();
            r.EndingAccess.Resolve.pDstResource = resolveTexture->get();
            r.EndingAccess.Resolve.Format = colorAttachmentFormat;
            r.EndingAccess.Resolve.ResolveMode = D3D12_RESOLVE_MODE_AVERAGE;
            r.EndingAccess.Resolve.PreserveResolveSource = (desc.storeOp == AttachmentStoreOp::eStore);

            D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS* subresourceParams = &m_resolveParameters[numResolveAttachments++];
            subresourceParams->SrcSubresource = qhwi::d3d(d3dTexture, &desc.textureData.subresource);
            subresourceParams->DstSubresource = qhwi::d3d(resolveTexture, &desc.resolveTarget.subresource);
            subresourceParams->DstX = 0;
            subresourceParams->DstY = 0;
            subresourceParams->SrcRect.left = 0;
            subresourceParams->SrcRect.top = 0;
            subresourceParams->SrcRect.right = d3dTexture->desc()->w;
            subresourceParams->SrcRect.bottom = d3dTexture->desc()->h;

            r.EndingAccess.Resolve.SubresourceCount = 1;
            r.EndingAccess.Resolve.pSubresourceParameters = subresourceParams;
        }
        else
        {
            r.EndingAccess.Type = qhwi::d3d(desc.storeOp);
        }
    }

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsDesc;
    if (depthStencilAttachmentDesc)
    {
        const DepthStencilAttachmentDescription& desc = *depthStencilAttachmentDesc;
        QHWI_ASSERT(desc.textureData.texture);

        auto d3dTexture = static_cast<qhwi::D3dTexture*>(desc.textureData.texture);
        DXGI_FORMAT depthAttachmentFormat = d3dTexture->resourceDesc().Format;
        SubresourceRange subresourceRange = desc.textureData.getSubresourceRange();
        subresourceRange.clamp(d3dTexture);
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = qhwi::d3dDepthStencilViewDesc(d3dTexture, subresourceRange);

        dsDesc.DepthBeginningAccess.Type = qhwi::d3d(desc.depthLoadOp);
        dsDesc.DepthEndingAccess.Type = qhwi::d3d(desc.depthStoreOp);
        dsDesc.StencilBeginningAccess.Type = qhwi::d3d(desc.stencilLoadOp);
        dsDesc.StencilEndingAccess.Type = qhwi::d3d(desc.stencilStoreOp);
        dsDesc.cpuDescriptor = m_device->getDsvDescriptor(d3dTexture->get(), &dsvDesc).cpuHandle;

        D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(depthAttachmentFormat, desc.clearValue.depth, UINT8(desc.clearValue.stencil));
        dsDesc.DepthBeginningAccess.Clear = { clearValue };
        dsDesc.StencilBeginningAccess.Clear = { clearValue };

        if (desc.depthResolveMode != ResolveMode::eNone || desc.stencilResolveMode != ResolveMode::eNone)
        {
            QHWI_ASSERT(desc.resolveTarget.texture != nullptr);
            auto resolveTexture = static_cast<D3dTexture*>(desc.resolveTarget.texture);

            D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS* subresourceParams = &m_resolveParameters[numResolveAttachments++];
            subresourceParams->SrcSubresource = qhwi::d3d(d3dTexture, &desc.textureData.subresource);
            subresourceParams->DstSubresource = qhwi::d3d(resolveTexture, &desc.resolveTarget.subresource);
            subresourceParams->DstX = 0;
            subresourceParams->DstY = 0;
            subresourceParams->SrcRect.left = 0;
            subresourceParams->SrcRect.top = 0;
            subresourceParams->SrcRect.right = d3dTexture->desc()->w;
            subresourceParams->SrcRect.bottom = d3dTexture->desc()->h;

            if (desc.depthResolveMode != ResolveMode::eNone)
            {
                dsDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
                dsDesc.DepthEndingAccess.Resolve.pSrcResource = d3dTexture->get();
                dsDesc.DepthEndingAccess.Resolve.pDstResource = resolveTexture->get();
                dsDesc.DepthEndingAccess.Resolve.Format = depthAttachmentFormat;
                dsDesc.DepthEndingAccess.Resolve.ResolveMode = qhwi::d3d(desc.depthResolveMode);
                dsDesc.DepthEndingAccess.Resolve.PreserveResolveSource = (desc.depthStoreOp == AttachmentStoreOp::eStore);
                dsDesc.DepthEndingAccess.Resolve.SubresourceCount = 1;
                dsDesc.DepthEndingAccess.Resolve.pSubresourceParameters = subresourceParams;
            }
            if (desc.stencilResolveMode != ResolveMode::eNone)
            {
                dsDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
                dsDesc.StencilEndingAccess.Resolve.pSrcResource = d3dTexture->get();
                dsDesc.StencilEndingAccess.Resolve.pDstResource = resolveTexture->get();
                dsDesc.StencilEndingAccess.Resolve.Format = depthAttachmentFormat;
                dsDesc.StencilEndingAccess.Resolve.ResolveMode = qhwi::d3d(desc.stencilResolveMode);
                dsDesc.StencilEndingAccess.Resolve.PreserveResolveSource = (desc.stencilStoreOp == AttachmentStoreOp::eStore);
                dsDesc.StencilEndingAccess.Resolve.SubresourceCount = 1;
                dsDesc.StencilEndingAccess.Resolve.pSubresourceParameters = subresourceParams;
            }
        }
    }

    beginD3dRenderPass(numColorAttachments, rtDescs, depthStencilAttachmentDesc ? &dsDesc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
}

void qhwi::D3dCommandList::beginD3dRenderPass(UINT numRenderTargets,
                                              const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets,
                                              const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil,
                                              D3D12_RENDER_PASS_FLAGS flags)
{
    onBeginRenderPass();
    beginProfilingPass();
    m_commandList->BeginRenderPass(numRenderTargets, pRenderTargets, pDepthStencil, flags);
}

void qhwi::D3dCommandList::endD3dRenderPass()
{
    onEndRenderPass();
    endProfilingPass();
    m_commandList->EndRenderPass();
}

void qhwi::D3dCommandList::endRenderPass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::endRenderPass", QHWI_COLOR_COMMAND_LIST);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.endRenderPass();
        m_inRenderPass = false;
        m_deferState = DeferState::ePlayback;
        m_rp.playback();
        m_rp.reset();
        m_deferState = DeferState::eOff;
        return;
    }
    endRenderPass1();
}

void qhwi::D3dCommandList::endRenderPass1()
{
    endD3dRenderPass();
}

void qhwi::D3dCommandList::beginComputePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::beginComputePass", QHWI_COLOR_COMMAND_LIST);
    onBeginComputePass();
    beginProfilingPass();
}

void qhwi::D3dCommandList::endComputePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::endComputePass", QHWI_COLOR_COMMAND_LIST);
    onEndComputePass();
    endProfilingPass();
}

void qhwi::D3dCommandList::beginTransferPass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::beginTransferPass", QHWI_COLOR_COMMAND_LIST);
    onBeginTransferPass();
    beginProfilingPass();
}

void qhwi::D3dCommandList::endTransferPass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::endTransferPass", QHWI_COLOR_COMMAND_LIST);
    onEndTransferPass();
    endProfilingPass();
}

void qhwi::D3dCommandList::beginAccelerationStructurePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::beginAccelerationStructurePass", QHWI_COLOR_COMMAND_LIST);
    onBeginAccelerationStructurePass();
    beginProfilingPass();
}

void qhwi::D3dCommandList::endAccelerationStructurePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::endAccelerationStructurePass", QHWI_COLOR_COMMAND_LIST);
    onEndAccelerationStructurePass();
    endProfilingPass();
}

void qhwi::D3dCommandList::dispatch(uint32_t numThreadGroupsX,
                                    uint32_t numThreadGroupsY,
                                    uint32_t numThreadGroupsZ,
                                    uint32_t numThreadsPerThreadgroupX,
                                    uint32_t numThreadsPerThreadgroupY,
                                    uint32_t numThreadsPerThreadgroupZ)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatch", QHWI_COLOR_COMPUTE);
    QHWI_ASSERT(m_inComputePass);
    setComputeState();
    m_commandList->Dispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
}

void qhwi::D3dCommandList::dispatchMesh(const DispatchMeshParams& p)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchMesh", QHWI_COLOR_COMPUTE);
    QHWI_ASSERT(m_inRenderPass);
    const uint32_t* numThreadGroups = p.numAmplificationShaderThreadGroups.data();
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.dispatchMesh(numThreadGroups[0], numThreadGroups[1], numThreadGroups[2]);
        return;
    }
    m_commandList->DispatchMesh(numThreadGroups[0], numThreadGroups[1], numThreadGroups[2]);
}

void qhwi::D3dCommandList::draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::draw", QHWI_COLOR_GRAPHICS);

    QHWI_ASSERT(m_inRenderPass);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.draw(vertexCount, instanceCount, firstVertex, firstInstance);
        return;
    }
    m_commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void qhwi::D3dCommandList::drawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::drawIndexed", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        return;
    }
    m_commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void qhwi::D3dCommandList::dispatchIndirect(uint32_t numThreadsPerThreadgroupX,
                                            uint32_t numThreadsPerThreadgroupY,
                                            uint32_t numThreadsPerThreadgroupZ,
                                            Buffer* buffer,
                                            size_t offset,
                                            uint32_t dispatchCount)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchIndirect", QHWI_COLOR_COMPUTE);
    QHWI_ASSERT(m_inComputePass);
    if (buffer->automaticBarriersEnabled())
    {
        m_nextComputeState.setIndirectBuffer(buffer);
    }
    setComputeState();
    m_commandList->ExecuteIndirect(m_device->getDispatchCommandSignature(), dispatchCount, static_cast<D3dBuffer*>(buffer)->get(), offset, nullptr, 0);
    markResourceAsUsed(buffer);
}

void qhwi::D3dCommandList::drawIndirect(Buffer* buffer, size_t offset, uint32_t drawCount)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::drawIndirect", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    markResourceAsUsed(buffer);
    auto d3dBuffer = static_cast<D3dBuffer*>(buffer);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawIndirect(m_device->getDrawCommandSignature(), d3dBuffer, offset, drawCount);
        return;
    }
    m_commandList->ExecuteIndirect(m_device->getDrawCommandSignature(), drawCount, d3dBuffer->get(), offset, nullptr, 0);
}

void qhwi::D3dCommandList::drawIndexedIndirect(Buffer* buffer, size_t offset, uint32_t drawCount)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::drawIndexedIndirect", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    markResourceAsUsed(buffer);
    auto d3dBuffer = static_cast<D3dBuffer*>(buffer);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawIndexedIndirect(m_device->getDrawIndexedCommandSignature(), d3dBuffer, offset, drawCount);
        return;
    }
    m_commandList->ExecuteIndirect(m_device->getDrawIndexedCommandSignature(), drawCount, d3dBuffer->get(), offset, nullptr, 0);
}

void qhwi::D3dCommandList::dispatchRays(const StridedDeviceAddressRegion& raygenShaderBindingTable,
                                        const StridedDeviceAddressRegion& missShaderBindingTable,
                                        const StridedDeviceAddressRegion& hitShaderBindingTable,
                                        const StridedDeviceAddressRegion& callableShaderBindingTable,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t depth)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchRays", QHWI_COLOR_RAYTRACING);
    auto D3dVirtualAddressRangeAndStride = [](const StridedDeviceAddressRegion& region)
    { return D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE({ region.startAddress, region.size, region.stride }); };

    auto D3dVirtualAddressRange = [](const StridedDeviceAddressRegion& region) { return D3D12_GPU_VIRTUAL_ADDRESS_RANGE({ region.startAddress, region.size }); };

    D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = { .RayGenerationShaderRecord = D3dVirtualAddressRange(raygenShaderBindingTable),
                                                  .MissShaderTable = D3dVirtualAddressRangeAndStride(missShaderBindingTable),
                                                  .HitGroupTable = D3dVirtualAddressRangeAndStride(hitShaderBindingTable),
                                                  .CallableShaderTable = D3dVirtualAddressRangeAndStride(callableShaderBindingTable),
                                                  .Width = width,
                                                  .Height = height,
                                                  .Depth = depth };

    CommandList::setRayTracingState();
    m_commandList->DispatchRays(&dispatchRaysDesc);
}

void qhwi::D3dCommandList::dispatchRaysIndirect(Buffer* buffer)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchRaysIndirect", QHWI_COLOR_RAYTRACING);
    QHWI_ASSERT_MSG(!m_device->supportsDispatchRaysIndirect(), "Feature D3D12_RAYTRACING_TIER_1_1 is not supported\n");
    QHWI_ASSERT(buffer);
    markResourceAsUsed(buffer);
    auto d3dBuffer = static_cast<D3dBuffer*>(buffer);
    CommandList::setRayTracingState();
    m_commandList->ExecuteIndirect(m_device->getDispatchRaysCommandSignature(), 1, d3dBuffer->get(), 0, nullptr, 0);
}

void qhwi::D3dCommandList::pushConstants(PipelineLayout* pipelineLayout, PipelineBindPoint pipelineBindPoint, const void* data, uint32_t index)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::pushConstants", QHWI_COLOR_COMMAND_LIST)
    QHWI_ASSERT(pipelineLayout);

    if (pipelineBindPoint == PipelineBindPoint::eGraphics)
    {
        QHWI_ASSERT(m_inRenderPass);
    }
    else
    {
        QHWI_ASSERT(m_inComputePass);
    }
    markResourceAsUsed(pipelineLayout);

    auto d3dPipelineLayout = static_cast<D3dPipelineLayout*>(pipelineLayout);
    auto pushConstantInfo = d3dPipelineLayout->pushConstantInfo(index);
    uint32_t rootParamIndex = pushConstantInfo.offset;
    uint32_t num32BitValues = pushConstantInfo.num32BitValues;

    uint32_t offsetIn32BitValues = 0;
    if (defer())
    {
        m_rp.pushConstants(rootParamIndex, num32BitValues, data, offsetIn32BitValues);
        return;
    }
    if (pipelineBindPoint == PipelineBindPoint::eGraphics)
    {
        m_commandList->SetGraphicsRoot32BitConstants(rootParamIndex, num32BitValues, data, offsetIn32BitValues);
    }
    else
    {
        m_commandList->SetComputeRoot32BitConstants(rootParamIndex, num32BitValues, data, offsetIn32BitValues);
    }
}

void qhwi::D3dCommandList::bindDescriptorSet(DescriptorSet* descriptorSet,
                                             PipelineBindPoint pipelineBindPoint,
                                             PipelineLayout* pipelineLayout,
                                             uint32_t numDynamicOffsets,
                                             const uint32_t* dynamicOffsets)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::bindDecriptorSet", QHWI_COLOR_PIPELINE);
    QHWI_ASSERT(descriptorSet);
    QHWI_ASSERT(pipelineLayout);

    m_currentPipelineLayout = pipelineLayout;
    markResourceAsUsed(descriptorSet);
    markResourceAsUsed(pipelineLayout);
    markDescSetResourcesAsUsed(descriptorSet);

    auto d3dDescriptorSet = static_cast<D3dDescriptorSet*>(descriptorSet);
    auto d3dPipelineLayout = static_cast<D3dPipelineLayout*>(pipelineLayout);
    uint32_t slot = descriptorSet->layoutDesc()->registerSpace;

    if (defer())
    {
        m_rp.bindDescriptorSet(d3dDescriptorSet, pipelineBindPoint, d3dPipelineLayout, numDynamicOffsets, dynamicOffsets);
        return;
    }
    if (m_desc.base.supportsAutomaticBarriers)
    {
        if (pipelineBindPoint == PipelineBindPoint::eGraphics)
        {
            // note: bindDescriptorSet might be called before beginRenderPass
            m_nextGraphicsState.setDescSet(slot, descriptorSet);
        }
        else if (pipelineBindPoint == PipelineBindPoint::eCompute)
        {
            m_nextComputeState.setDescSet(slot, descriptorSet);
        }
        else if (pipelineBindPoint == PipelineBindPoint::eRayTracing)
        {
            m_nextRayTracingState.setDescSet(slot, descriptorSet);
        }
    }
    bindDescriptorSet1(d3dDescriptorSet, pipelineBindPoint, d3dPipelineLayout, numDynamicOffsets, dynamicOffsets);
}

void qhwi::D3dCommandList::bindDescriptorSet1(D3dDescriptorSet* d3dDescriptorSet,
                                              PipelineBindPoint pipelineBindPoint,
                                              D3dPipelineLayout* d3dPipelineLayout,
                                              uint32_t numDynamicOffsets,
                                              const uint32_t* dynamicOffsets)
{
    auto dsDesc = d3dDescriptorSet->desc();
    auto descriptorSetLayout = d3dDescriptorSet->desc()->layout.Get();
    uint32_t dsLayoutRootParamOffset = d3dPipelineLayout->descriptorSetLayoutRootParamOffset(descriptorSetLayout);

    if (pipelineBindPoint == PipelineBindPoint::eGraphics)
    {
        m_commandList->SetGraphicsRootSignature(d3dPipelineLayout->rootSignature()->get());
    }
    else
    {
        m_commandList->SetComputeRootSignature(d3dPipelineLayout->rootSignature()->get());
    }

    uint32_t dynamicOffsetIndex = 0;

    for (uint32_t i = 0; i < dsDesc->numBindings(); i++)
    {
        DescriptorSetBinding* binding = dsDesc->bindings[i];
        if (!binding)
        {
            continue;
        }
        uint32_t rootParameterIndex = dsLayoutRootParamOffset + i;
        uint32_t count = binding->count();
        const auto bindingType = dsDesc->bindingType(i);
        if (bindingType == DescriptorType::eConstantBuffer || bindingType == DescriptorType::eDynamicConstantBuffer)
        {
            for (uint32_t j = 0; j < count; j++)
            {
                const auto& bufferInfo = binding->bufferInfo(j);
                auto d3dBuffer = static_cast<D3dBuffer*>(bufferInfo.buffer.Get());

                uint32_t dynamicOffset = 0;
                if (bindingType == DescriptorType::eDynamicConstantBuffer)
                {
                    QHWI_ASSERT(dynamicOffsetIndex < numDynamicOffsets);
                    dynamicOffset = dynamicOffsets[dynamicOffsetIndex++];
                }
                if (pipelineBindPoint == PipelineBindPoint::eGraphics)
                {
                    m_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, d3dBuffer->get()->GetGPUVirtualAddress() + bufferInfo.offset + dynamicOffset);
                }
                else
                {
                    m_commandList->SetComputeRootConstantBufferView(rootParameterIndex, d3dBuffer->get()->GetGPUVirtualAddress() + bufferInfo.offset + dynamicOffset);
                }
                rootParameterIndex++;
            }
        }
        else if (bindingType == DescriptorType::eTexture || bindingType == DescriptorType::eInputAttachment)
        {
            D3dDescriptorHandle baseDescriptor = d3dDescriptorSet->getBindingBaseDescriptorHandle(i);
            if (pipelineBindPoint == PipelineBindPoint::eGraphics)
            {
                m_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
            else
            {
                m_commandList->SetComputeRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
        }
        else if (bindingType == DescriptorType::eSampler)
        {
            D3dDescriptorHandle baseDescriptor = d3dDescriptorSet->getBindingBaseDescriptorHandle(i);

            if (pipelineBindPoint == PipelineBindPoint::eGraphics)
            {
                m_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
            else
            {
                m_commandList->SetComputeRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
        }
        else if (bindingType == DescriptorType::eStructuredBuffer || bindingType == DescriptorType::eByteAddressBuffer || bindingType == DescriptorType::eTypedBuffer)
        {
            D3dDescriptorHandle baseDescriptor = d3dDescriptorSet->getBindingBaseDescriptorHandle(i);

            if (pipelineBindPoint == PipelineBindPoint::eGraphics)
            {
                m_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
            else
            {
                m_commandList->SetComputeRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
        }
        else if (bindingType == DescriptorType::eRWStructuredBuffer || bindingType == DescriptorType::eRWByteAddressBuffer || bindingType == DescriptorType::eRWTypedBuffer)
        {
            D3dDescriptorHandle baseDescriptor = d3dDescriptorSet->getBindingBaseDescriptorHandle(i);

            if (pipelineBindPoint == PipelineBindPoint::eGraphics)
            {
                m_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
            else
            {
                m_commandList->SetComputeRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
        }
        else if (bindingType == DescriptorType::eRWTexture)
        {
            D3dDescriptorHandle baseDescriptor = d3dDescriptorSet->getBindingBaseDescriptorHandle(i);

            if (pipelineBindPoint == PipelineBindPoint::eGraphics)
            {
                m_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
            else
            {
                m_commandList->SetComputeRootDescriptorTable(rootParameterIndex, baseDescriptor.gpuHandle);
            }
        }
        else if (bindingType == DescriptorType::eAccelerationStructure)
        {
            for (uint32_t j = 0; j < count; j++)
            {
                const auto& accelStruct = binding->accelStructInfo(j);
                D3dAccelerationStructure* d3dAccelerationStructure = static_cast<D3dAccelerationStructure*>(accelStruct.accelerationStructure.Get());
                D3dBuffer* d3dBuffer = d3dAccelerationStructure->getBuffer();

                if (pipelineBindPoint == PipelineBindPoint::eGraphics)
                {
                    m_commandList->SetGraphicsRootShaderResourceView(rootParameterIndex, d3dBuffer->getDeviceAddress());
                }
                else
                {
                    m_commandList->SetComputeRootShaderResourceView(rootParameterIndex, d3dBuffer->getDeviceAddress());
                }
                rootParameterIndex++;
            }
        }
    }
}

void qhwi::D3dCommandList::bindVertexBuffer(Buffer* buffer, uint32_t slot, size_t offset, size_t stride)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(buffer);
    markResourceAsUsed(buffer);

    auto d3dBuffer = static_cast<D3dBuffer*>(buffer);

    D3D12_VERTEX_BUFFER_VIEW vb;
    vb.BufferLocation = d3dBuffer->get()->GetGPUVirtualAddress() + offset;
    QHWI_ASSERT(stride != 0);
    vb.StrideInBytes = UINT(stride);
    vb.SizeInBytes = UINT(d3dBuffer->size() - offset);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.bindVertexBuffer(d3dBuffer, slot, vb);
        return;
    }

    m_commandList->IASetVertexBuffers(slot, 1, &vb);
}

void qhwi::D3dCommandList::bindIndexBuffer(Buffer* buffer, IndexFormat indexFormat, size_t offset)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(buffer);
    markResourceAsUsed(buffer);
    auto d3dBuffer = static_cast<D3dBuffer*>(buffer);
    D3D12_INDEX_BUFFER_VIEW ib;
    ib.BufferLocation = d3dBuffer->get()->GetGPUVirtualAddress() + offset;
    ib.Format = d3d(indexFormat);
    ib.SizeInBytes = UINT(d3dBuffer->size() - offset);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.bindIndexBuffer(d3dBuffer, ib);
        return;
    }
    m_commandList->IASetIndexBuffer(&ib);
}

void qhwi::D3dCommandList::clear(Buffer* dst, uint32_t value)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }
    UINT values[] = { value, value, value, value };
    auto d3dDstBuffer = static_cast<D3dBuffer*>(dst);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc = { .Format = DXGI_FORMAT_R32_TYPELESS,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = { .FirstElement = 0, .NumElements = UINT(dst->size() / 4), .Flags = D3D12_BUFFER_UAV_FLAG_RAW } };
    auto d3dDescriptorHandle = m_device->getUavDescriptor(d3dDstBuffer->get(), &uavDesc);
    auto d3dShaderVisibleDescriptorHandle = m_device->getUavShaderVisibleDescriptor(d3dDstBuffer->get(), &uavDesc);

    //TODO: remove
    addBufferBarrier({ dst, BufferState_Transfer_Dst, { PipelineStage_Transfer, AccessMode_UnorderedAccess } });

    m_commandList->ClearUnorderedAccessViewUint(d3dShaderVisibleDescriptorHandle.gpuHandle, d3dDescriptorHandle.cpuHandle, d3dDstBuffer->get(), values, 0, nullptr);

    //TODO: remove
    addBufferBarrier({ dst, { PipelineStage_Transfer, AccessMode_UnorderedAccess }, BufferState_Transfer_Dst });

    markResourceAsUsed(dst);
}

void qhwi::D3dCommandList::clear(Texture* dst, Color v)
{
    QHWI_ASSERT(m_inTransferPass);

    //TODO: remove
    addTextureBarrier({ dst, TextureState_Transfer_Dst, { PipelineStage_Transfer, AccessMode_UnorderedAccess, qhwi::ImageLayout::eUnorderedAccess } });

    auto d3dDstTexture = static_cast<D3dTexture*>(dst);
    const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc = d3dDstTexture->defaultUavDesc();
    auto d3dDescriptorHandle = m_device->getUavDescriptor(d3dDstTexture->get(), &uavDesc);
    auto d3dShaderVisibleDescriptorHandle = m_device->getUavShaderVisibleDescriptor(d3dDstTexture->get(), &uavDesc);

    m_commandList->ClearUnorderedAccessViewFloat(d3dShaderVisibleDescriptorHandle.gpuHandle, d3dDescriptorHandle.cpuHandle, d3dDstTexture->get(), v.data(), 0, nullptr);

    markResourceAsUsed(dst);
    //TODO: remove
    addTextureBarrier({ dst, { PipelineStage_Transfer, AccessMode_UnorderedAccess, qhwi::ImageLayout::eUnorderedAccess }, TextureState_Transfer_Dst });
}

void qhwi::D3dCommandList::resetState()
{
    m_commandList->ClearState(nullptr);

    uint32_t numDescriptorHeaps = 2;
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_device->getCbvSrvUavShaderVisibleDescriptorHeap()->get(), m_device->getSamplerShaderVisibleDescriptorHeap()->get() };
    m_commandList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
}

void qhwi::D3dCommandList::copyBuffer(Buffer* dst, Buffer* src, std::span<const CopyBufferRegion> regions)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::copyBuffer", QHWI_COLOR_TRANSFER)
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    if (src->automaticBarriersEnabled())
    {
        setState(src, BufferState_Transfer_Src);
    }
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }

    auto d3dSrcBuffer = static_cast<D3dBuffer*>(src);
    auto d3dDstBuffer = static_cast<D3dBuffer*>(dst);
    for (const auto& r : regions)
    {
        size_t srcSize = r.srcSize;
        if (srcSize == 0)
        {
            srcSize = d3dSrcBuffer->size();
        }

        m_commandList->CopyBufferRegion(d3dDstBuffer->get(), r.dstOffset, d3dSrcBuffer->get(), r.srcOffset, srcSize);
    }

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::D3dCommandList::copyBufferToTexture(Texture* dst, Buffer* src, std::span<const CopyBufferToTextureRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    auto d3dSrcBuffer = static_cast<D3dBuffer*>(src);
    auto d3dDstTexture = static_cast<D3dTexture*>(dst);
    auto d3dDevice = m_device->get();

    if (src->automaticBarriersEnabled())
    {
        setState(src, BufferState_Transfer_Src);
    }
    for (const auto& region : regions)
    {
        if (dst->automaticBarriersEnabled())
        {
            SubresourceRange subresourceRange = EntireResource;

            if (region.dstSubresource)
            {
                subresourceRange.setStart(*region.dstSubresource);
                subresourceRange.setEnd(*region.dstSubresource);
            }

            setState(dst, TextureState_Transfer_Dst, subresourceRange);
        }

        uint32_t subresource = qhwi::d3d(d3dDstTexture, region.dstSubresource);

        auto dstResourceDesc = d3dDstTexture->get()->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows;
        uint64_t rowSizesInBytes;
        uint64_t requiredSize = 0;
        uint64_t baseOffset = region.srcOffset;

        d3dDevice->GetCopyableFootprints(&dstResourceDesc, subresource, 1, baseOffset, &layout, &numRows, &rowSizesInBytes, &requiredSize);

        CD3DX12_TEXTURE_COPY_LOCATION dstLocation(d3dDstTexture->get(), subresource);
        CD3DX12_TEXTURE_COPY_LOCATION srcLocation(d3dSrcBuffer->get(), layout);

        //TODO: set source box
        m_commandList->CopyTextureRegion(&dstLocation, region.dstOrigin.x, region.dstOrigin.y, region.dstOrigin.z, &srcLocation, nullptr);
    }

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::D3dCommandList::copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion> regions)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::copyTexture", QHWI_COLOR_TRANSFER);
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    auto d3dSrcTexture = static_cast<D3dTexture*>(src);
    auto d3dDstTexture = static_cast<D3dTexture*>(dst);
    const Texture::Description* srcDesc = d3dSrcTexture->desc();

    if (src->automaticBarriersEnabled())
    {
        setState(src, TextureState_Transfer_Src);
    }
    for (const auto& region : regions)
    {
        if (dst->automaticBarriersEnabled())
        {
            SubresourceRange subresourceRange = EntireResource;

            if (region.dstSubresource)
            {
                subresourceRange.setStart(*region.dstSubresource);
                subresourceRange.setEnd(*region.dstSubresource);
            }

            setState(dst, TextureState_Transfer_Dst, subresourceRange);
        }

        uint32_t srcSubresource = qhwi::d3d(d3dSrcTexture, region.srcSubresource);
        uint32_t dstSubresource = qhwi::d3d(d3dDstTexture, region.dstSubresource);

        uint32_t dstX = region.dstOrigin.x;
        uint32_t dstY = region.dstOrigin.y;
        uint32_t dstZ = region.dstOrigin.z;

        const uvec3& srcOrigin = region.srcOrigin;
        Extent3D srcExtent = region.srcExtent;
        uint32_t srcMipLevel = region.srcSubresource ? region.srcSubresource->mipSlice : 0;
        if (srcExtent.w == 0)
        {
            srcExtent.w = std::max(1u, srcDesc->w >> srcMipLevel);
        }
        if (srcExtent.h == 0)
        {
            srcExtent.h = std::max(1u, srcDesc->h >> srcMipLevel);
        }
        if (srcExtent.d == 0)
        {
            srcExtent.d = std::max(1u, srcDesc->d >> srcMipLevel);
        }

        D3D12_BOX srcBox;
        srcBox.left = srcOrigin.x;
        srcBox.right = srcBox.left + srcExtent.w;
        srcBox.top = srcOrigin.y;
        srcBox.bottom = srcBox.top + srcExtent.h;
        srcBox.front = srcOrigin.z;
        srcBox.back = srcBox.front + srcExtent.d;

        QRE_ASSERT(srcBox.right > srcBox.left && srcBox.bottom > srcBox.top && srcBox.back > srcBox.front);

        CD3DX12_TEXTURE_COPY_LOCATION dstLocation(d3dDstTexture->get(), dstSubresource);
        CD3DX12_TEXTURE_COPY_LOCATION srcLocation(d3dSrcTexture->get(), srcSubresource);
        m_commandList->CopyTextureRegion(&dstLocation, dstX, dstY, dstZ, &srcLocation, &srcBox);
    }

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::D3dCommandList::copyTextureToBuffer(Buffer* dst, Texture* src, std::span<const CopyTextureToBufferRegion> regions)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::copyTextureToBuffer", QHWI_COLOR_TRANSFER);
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    auto d3dSrcTexture = static_cast<D3dTexture*>(src);
    auto d3dDstBuffer = static_cast<D3dBuffer*>(dst);
    auto d3dDevice = m_device->get();

    if (src->automaticBarriersEnabled())
    {
        setState(src, TextureState_Transfer_Src);
    }
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }

    for (const auto& region : regions)
    {
        uint32_t subresource = qhwi::d3d(d3dSrcTexture, region.srcSubresource);

        auto srcResourceDesc = d3dSrcTexture->get()->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows;
        uint64_t rowSizesInBytes;
        uint64_t requiredSize = 0;

        d3dDevice->GetCopyableFootprints(&srcResourceDesc, subresource, 1, region.dstOffset, &layout, &numRows, &rowSizesInBytes, &requiredSize);

        CD3DX12_TEXTURE_COPY_LOCATION dstLocation(d3dDstBuffer->get(), layout);
        CD3DX12_TEXTURE_COPY_LOCATION srcLocation(d3dSrcTexture->get(), subresource);

        Extent3D srcExtent = region.srcExtent;
        uint32_t srcMipLevel = region.srcSubresource ? region.srcSubresource->mipSlice : 0;
        if (srcExtent.w == 0)
        {
            srcExtent.w = std::max(1u, src->desc()->w >> srcMipLevel);
        }
        if (srcExtent.h == 0)
        {
            srcExtent.h = std::max(1u, src->desc()->h >> srcMipLevel);
        }
        if (srcExtent.d == 0)
        {
            srcExtent.d = std::max(1u, src->desc()->d >> srcMipLevel);
        }

        D3D12_BOX srcBox;
        srcBox.left = region.srcOrigin.x;
        srcBox.right = srcBox.left + srcExtent.w;
        srcBox.top = region.srcOrigin.y;
        srcBox.bottom = srcBox.top + srcExtent.h;
        srcBox.front = region.srcOrigin.z;
        srcBox.back = srcBox.front + srcExtent.d;

        QRE_ASSERT(srcBox.right > srcBox.left && srcBox.bottom > srcBox.top && srcBox.back > srcBox.front);

        //TODO: dstX, dstY, dstZ
        m_commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, &srcBox);
    }

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::D3dCommandList::updateBuffer(Buffer* dst, std::span<const UpdateBufferRegion> regions)
{
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }
    CommandList::updateBuffer(dst, regions);
}

void qhwi::D3dCommandList::updateTexture(Texture* dst, std::span<const UpdateTextureRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    auto d3dDstTexture = static_cast<D3dTexture*>(dst);

    const D3D12_RESOURCE_DESC& resourceDesc = d3dDstTexture->resourceDesc();
    uint64_t totalBytes = 0;
    for (const auto& r : regions)
    {
        QHWI_ASSERT(r.srcData && r.srcRowPitch && r.srcDepthPitch);

        if (dst->automaticBarriersEnabled())
        {
            setState(dst, TextureState_Transfer_Dst, SubresourceRange(Subresource().setArraySlice(uint16_t(r.arraySlice)).setMipSlice(uint16_t(r.mipLevel))));
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows;
        uint64_t rowSizeInBytes;
        uint64_t subresourceTotalBytes;

        uint32_t subresource = D3D12CalcSubresource(r.mipLevel, r.arraySlice, 0, d3dDstTexture->desc()->mipLevels, d3dDstTexture->desc()->arrayLayers);
        m_device->get()->GetCopyableFootprints(&resourceDesc, subresource, 1, 0, &layout, &numRows, &rowSizeInBytes, &subresourceTotalBytes);
        subresourceTotalBytes = qre::AlignUp(subresourceTotalBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        totalBytes += subresourceTotalBytes;
    }

    // Upload to staging buffer
    Buffer* stagingBuffer = nullptr;
    size_t stagingBufferOffset = 0;
    if (!device()->uploadPool().get(device(), this, totalBytes, stagingBuffer, stagingBufferOffset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
    {
        QHWI_FATAL("no staging region found");
    }
    uint8_t* stagingBufferAddr = (uint8_t*)stagingBuffer->mapAddr();
    bool firstMapCall = (stagingBufferAddr == nullptr);
    if (firstMapCall)
    {
        stagingBufferAddr = (uint8_t*)stagingBuffer->map();
    }

    for (const auto& r : regions)
    {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows;
        uint64_t rowSizeInBytes;
        uint64_t subresourceTotalBytes;

        uint32_t subresource = D3D12CalcSubresource(r.mipLevel, r.arraySlice, 0, d3dDstTexture->desc()->mipLevels, d3dDstTexture->desc()->arrayLayers);
        m_device->get()->GetCopyableFootprints(&resourceDesc, subresource, 1, 0, &layout, &numRows, &rowSizeInBytes, &subresourceTotalBytes);

        layout.Offset = stagingBufferOffset;

        QHWI_ASSERT(numRows <= layout.Footprint.Height);

        uint32_t dstRowPitch = layout.Footprint.RowPitch;
        uint32_t depthSliceSize = dstRowPitch * numRows;
        uint32_t depth = layout.Footprint.Depth;
        const uint8_t* srcAddr = (const uint8_t*)r.srcData;
        uint8_t* dstAddr = stagingBufferAddr + stagingBufferOffset;

        if (r.srcRowPitch == dstRowPitch && rowSizeInBytes == dstRowPitch) // Fast path, we can copy all the regions texture data in one copy
        {
            size_t size = dstRowPitch * (numRows * depth);
            memcpy(dstAddr, srcAddr, size);
        }
        else
        {
            for (uint32_t depthSlice = 0; depthSlice < depth; ++depthSlice)
            {
                for (uint32_t row = 0; row < numRows; ++row)
                {
                    memcpy(dstAddr, srcAddr, rowSizeInBytes);
                    srcAddr += r.srcRowPitch;
                    dstAddr += dstRowPitch;
                }
            }
        }

        CD3DX12_TEXTURE_COPY_LOCATION dstLocation(d3dDstTexture->get(), subresource);
        CD3DX12_TEXTURE_COPY_LOCATION srcLocation(static_cast<D3dBuffer*>(stagingBuffer)->get(), layout);
        m_commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        stagingBufferOffset += (depthSliceSize * depth);
        stagingBufferOffset = qre::AlignUp(stagingBufferOffset, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    }
    markResourceAsUsed(dst);
    markResourceAsUsed(stagingBuffer);
}

void qhwi::D3dCommandList::bindComputePipeline(ComputePipeline* pipeline)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::bindComputePipeline", QHWI_COLOR_PIPELINE);
    QHWI_ASSERT(m_inComputePass);

    auto d3dPipeline = static_cast<D3dComputePipeline*>(pipeline);
    m_commandList->SetPipelineState(d3dPipeline->get());
    m_commandList->SetComputeRootSignature(d3dPipeline->getRootSignature()->get());
    m_currentPipelineLayout = pipeline->getLayout();
    m_currentPipeline = pipeline;

    markResourceAsUsed(pipeline);
}

void qhwi::D3dCommandList::bindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::bindGraphicsPipeline", QHWI_COLOR_PIPELINE);
    QHWI_ASSERT(m_inRenderPass);

    auto d3dPipeline = static_cast<D3dGraphicsPipeline*>(pipeline);
    m_currentPipelineLayout = pipeline->getLayout();
    m_currentPipeline = pipeline;

    markResourceAsUsed(pipeline);

    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.bindGraphicsPipeline(d3dPipeline);
        return;
    }
    bindGraphicsPipeline1(d3dPipeline);
}

void qhwi::D3dCommandList::bindRayTracingPipeline(RayTracingPipeline* pipeline)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::bindRayTracingPipeline", QHWI_COLOR_PIPELINE);
    m_currentPipelineLayout = pipeline->getLayout();
    m_currentPipeline = pipeline;

    markResourceAsUsed(pipeline);

    m_commandList->SetPipelineState1(static_cast<D3dRayTracingPipeline*>(pipeline)->get());
}

void qhwi::D3dCommandList::bindGraphicsPipeline1(D3dGraphicsPipeline* d3dPipeline)
{
    if (d3dPipeline->isStencilTestEnabled())
    {
        m_commandList->OMSetStencilRef(d3dPipeline->getStencilRef());
    }
    m_commandList->SetPipelineState(d3dPipeline->get());
    m_commandList->IASetPrimitiveTopology(d3dPipeline->getPrimitiveTopology());
    m_commandList->SetGraphicsRootSignature(d3dPipeline->getRootSignature()->get());
}

void qhwi::D3dCommandList::setViewports(uint32_t numViewports, const Viewport* viewports)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(viewports);
    QHWI_ASSERT(numViewports < MaxViewports);
    D3D12_VIEWPORT d3dViewports[MaxViewports];
    for (uint32_t j = 0; j < numViewports; j++)
    {
        d3dViewports[j] = d3d(viewports[j]);
    }
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.setViewports(numViewports, d3dViewports);
        return;
    }
    m_commandList->RSSetViewports(numViewports, d3dViewports);
}

void qhwi::D3dCommandList::setScissors(uint32_t numScissorRects, const ScissorRect* scissorRects)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(scissorRects);
    QHWI_ASSERT(numScissorRects < MaxScissors);
    D3D12_RECT d3dScissorRects[MaxScissors];
    for (uint32_t j = 0; j < numScissorRects; j++)
    {
        d3dScissorRects[j] = d3d(scissorRects[j]);
    }
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.setScissors(numScissorRects, d3dScissorRects);
        return;
    }
    m_commandList->RSSetScissorRects(numScissorRects, d3dScissorRects);
}

void qhwi::D3dCommandList::setStencilReference(uint32_t reference)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_TODO_MSG("Enable pipeline dynamic states");
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.setStencilReference(reference);
        return;
    }
    m_commandList->OMSetStencilRef(reference);
}

void qhwi::D3dCommandList::beginTimerQuery(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(timerQuery->timerState == TimerState::eReset, "Cannot start a timer that has not been reset");

    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot set a barrier inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot set a barrier inside a transfer pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot set a barrier inside a compute pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot set a barrier inside an acceleration structure pass");

    m_scheduledTimerQuery = timerQuery;

    timerQuery->timerState = TimerState::eStarted;
    markResourceAsUsed(timerQuery);
}

void qhwi::D3dCommandList::endTimerQuery(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(timerQuery->timerState == TimerState::eStarted, "Cannot end a timer that has not been started.");

    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot end a timer inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot end a timer inside a compute pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot end a timer inside a transfer pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot end a timer inside an acceleration structure pass");

    m_scheduledTimerQuery = nullptr;

    timerQuery->timerState = TimerState::eStopped;
    markResourceAsUsed(timerQuery);
}

void qhwi::D3dCommandList::resolveTimerQueryData(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(timerQuery->timerState == TimerState::eStopped, "Cannot resolve a timer that has not been stopped.");

    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot resolve a timer inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot resolve a timer inside a compute pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot resolve a timer inside a transfer pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot resolve a timer inside an acceleration structure pass");

    timerQuery->timerState = TimerState::eUnresolved;
    D3dTimerQueryHeap* d3dTimerQueryHeap = m_device->getTimerQueryHeap();

    m_commandList->ResolveQueryData(d3dTimerQueryHeap->get(),
                                    D3D12_QUERY_TYPE_TIMESTAMP,
                                    timerQuery->beginQueryIndex,
                                    2,
                                    d3dTimerQueryHeap->getQueryDataResolveBuffer(),
                                    timerQuery->beginQueryIndex * 8u);

    m_referencedTimerQueries[m_referencedTimerQueriesIndex++] = timerQuery;

    markResourceAsUsed(timerQuery);
}

void qhwi::D3dCommandList::resetTimerQueryPool(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot reset a timer inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot reset a timer inside a compute pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot reset a timer inside a transfer pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot reset a timer inside an acceleration structure pass");

    timerQuery->timerState = TimerState::eReset;
}

qhwi::D3dCommandQueue::D3dCommandQueue(D3dDevice* device, const Description& desc, const char* name)
    : m_desc(desc)
    , m_device(device)
{
    auto d3dDevice = device->get();
    V(d3dDevice->CreateCommandQueue(&m_desc.d3d.commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));

#ifdef QHWI_TRACY_GPU_BACKEND_D3D12
    m_tracyCtx = TracyD3D12Context(d3dDevice, m_commandQueue.Get());
#endif

    V(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_commandListSubmissionFence)));
    setName(m_commandQueue.Get(), name);
    char str[MaxStringSize];
    qre::PrintfSafe(str, "%s_commandListSubmissionFence", name);
    setName(m_commandListSubmissionFence.Get(), str);
}

qhwi::D3dCommandQueue::~D3dCommandQueue()

{
#ifdef QHWI_TRACY_GPU_BACKEND_D3D12
    if (m_tracyCtx)
    {
        TracyD3D12Destroy(m_tracyCtx);
    }
#endif
    waitForIdle();
}

uint64_t qhwi::D3dCommandQueue::submit(uint32_t numCommandLists, CommandList** commandLists, Fence* fence, ext::Swapchain*)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3DCommandQueue::submit", QHWI_COLOR_QUEUE_SUBMIT);
    updateLastCompletedID();

    if (numCommandLists > MaxCommandLists)
    {
        QHWI_FATAL("numCommandLists (%d) > MaxCommandLists (%d)", numCommandLists, MaxCommandLists);
    }

    if (numCommandLists != 0)
    {
        QHWI_ASSERT(commandLists);
    }

    D3dCommandList* qhwiD3dCommandLists[MaxCommandLists];
    ID3D12CommandList* d3d12CommandLists[MaxCommandLists];
    for (uint32_t j = 0; j < numCommandLists; j++)
    {
        CommandList* cmdList = commandLists[j];
        qhwiD3dCommandLists[j] = static_cast<D3dCommandList*>(cmdList);
        d3d12CommandLists[j] = qhwiD3dCommandLists[j]->get();
    }
    m_commandQueue->ExecuteCommandLists(numCommandLists, d3d12CommandLists);

#ifdef QHWI_TRACY_GPU_BACKEND_D3D12
    TracyD3D12Collect(m_tracyCtx);
#endif

    m_lastSubmittedID++;
    V(m_commandQueue->Signal(m_commandListSubmissionFence.Get(), m_lastSubmittedID));

    for (uint32_t j = 0; j < numCommandLists; ++j)
    {
        auto d3dCommandList = qhwiD3dCommandLists[j];
        d3dCommandList->submitted(this, m_commandListSubmissionFence.Get(), m_lastSubmittedID);
    }

    if (fence)
    {
        auto d3dFence = static_cast<D3dFence*>(fence);
        V(m_commandQueue->Signal(d3dFence->get(), FenceState_Signaled));
    }

    if (numCommandLists != 0)
    {
        trackCommandLists((uint32_t)numCommandLists, commandLists, m_lastSubmittedID);
    }

    return m_lastSubmittedID;
}

void qhwi::D3dCommandQueue::waitForIdle()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3DCommandQueue::waitForIdle", QHWI_COLOR_QUEUE_SUBMIT);
    D3dFence fence(m_device);
    V(m_commandQueue->Signal(fence.get(), FenceState_Signaled));
    fence.wait();

    updateLastCompletedID();
}

void qhwi::D3dCommandQueue::updateLastCompletedID()
{
    if (m_lastCompletedID < m_lastSubmittedID)
    {
        m_lastCompletedID = m_commandListSubmissionFence->GetCompletedValue();
    }
}

qhwi::D3dFence::D3dFence(D3dDevice* device, FenceState initialValue, const char* name, D3D12_HEAP_FLAGS heapFlags)
{
    auto d3dDevice = device->get();
    V(d3dDevice->CreateFence(UINT64(initialValue), D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    setName(m_fence.Get(), name);
    m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (heapFlags & D3D12_HEAP_FLAG_SHARED)
    {
        d3dDevice->CreateSharedHandle(m_fence.Get(), nullptr, GENERIC_ALL, nullptr, &m_sharedHandle);
    }
}

qhwi::D3dFence::~D3dFence()
{
    CloseHandle(m_event);
}

qhwi::Result qhwi::D3dFence::wait()
{
    if (m_fence->GetCompletedValue() == FenceState_Signaled)
    {
        return Result(ResultCode::eSuccess);
    }
    V(m_fence->SetEventOnCompletion(FenceState_Signaled, m_event));
    DWORD result = WaitForSingleObjectEx(m_event, INFINITE, FALSE);
    if (result != WAIT_OBJECT_0)
    {
        QHWI_ERR("wait for fence failed, result: %x", result);
    }
    return Result(ResultCode::eSuccess);
}

qhwi::FenceState qhwi::D3dFence::getCurrentState()
{
    return FenceState(m_fence->GetCompletedValue());
}

void qhwi::D3dFence::reset()
{
    V(m_fence->Signal(UINT64(FenceState_Unsignaled)));
}

void qhwi::D3dDescriptorHeap::init(D3dDevice* device, const Description& desc, const char* name)
{
    m_device = device;
    m_desc = desc;
    auto d3dDevice = device->get();
    V(d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap)));
    head.cpuHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        head.gpuHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    }
    m_descriptorHandleIncrementSize = d3dDevice->GetDescriptorHandleIncrementSize(m_desc.Type);
    m_heapAllocator.init(m_desc.NumDescriptors);
    setName(m_descriptorHeap.Get(), name);
}

std::optional<qhwi::D3dDescriptorHandle> qhwi::D3dDescriptorHeap::getHandle()
{
    int index = m_heapAllocator.alloc();
    if (index == -1)
    {
        return std::nullopt;
    }

    D3dDescriptorHandle handle = head;
    int offset = m_descriptorHandleIncrementSize * index;
    handle.cpuHandle.ptr += offset;
    if (m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        handle.gpuHandle.ptr += offset;
    }

    return handle;
}

std::optional<qhwi::D3dDescriptorHandle> qhwi::D3dDescriptorHeap::stage(D3dDescriptorHandle src)
{
    std::optional<qhwi::D3dDescriptorHandle> dst = getHandle();
    if (dst.has_value())
    {
        m_device->get()->CopyDescriptorsSimple(1, dst.value().cpuHandle, src.cpuHandle, m_desc.Type);
        return dst;
    }
    else
    {
        return std::nullopt;
    }
}

bool qhwi::D3dDescriptorHeap::releaseHandle(D3dDescriptorHandle handle)
{
    int index = int((handle.cpuHandle.ptr - head.cpuHandle.ptr) / m_descriptorHandleIncrementSize);
    return m_heapAllocator.release(index);
}

std::optional<qhwi::D3dDescriptorHandle> qhwi::D3dDescriptorHeap::stage(const D3dDescriptorTable& descriptorTable, const D3dDescriptorHandle& src, uint32_t descriptorIndex) const
{
    qhwi::D3dDescriptorHandle dst = descriptorTable.baseHandle;
    auto offset = descriptorIndex * m_descriptorHandleIncrementSize;
    dst.cpuHandle.ptr += offset;

    if (m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        dst.gpuHandle.ptr += offset;
    }

    m_device->get()->CopyDescriptorsSimple(1, dst.cpuHandle, src.cpuHandle, m_desc.Type);
    return dst;
}

bool qhwi::D3dDescriptorHeap::releaseDescriptorTable(const D3dDescriptorTable& descriptorTable)
{
    int index = int((descriptorTable.baseHandle.cpuHandle.ptr - head.cpuHandle.ptr) / m_descriptorHandleIncrementSize);
    return m_heapAllocator.release(index, descriptorTable.numDescriptors);
}

void qhwi::D3dHeapAllocator::init(uint32_t numDescriptors)
{
    m_numDescriptors = numDescriptors;
    m_isAllocated.reset();
}

std::optional<qhwi::D3dDescriptorTable> qhwi::D3dDescriptorHeap::getDescriptorTable(uint32_t numDescriptors)
{
    int index = m_heapAllocator.alloc(numDescriptors);
    if (index == -1)
    {
        return std::nullopt;
    }

    auto handle = head;
    int offset = m_descriptorHandleIncrementSize * index;
    handle.cpuHandle.ptr += offset;
    if (m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        handle.gpuHandle.ptr += offset;
    }

    return qhwi::D3dDescriptorTable{ handle, numDescriptors };
}

void qhwi::D3dDescriptorHeapManager::init(D3dDevice* device, const D3dDescriptorHeap::Description& desc, const char* name)
{
    m_device = device;
    m_desc = desc;
    m_descriptorHeapSearchIndex = 0;
    m_descriptorHandleIncrementSize = device->get()->GetDescriptorHandleIncrementSize(m_desc.Type);
    m_descriptorHeap.emplace_back().init(device, desc, name);
}

std::optional<qhwi::D3dDescriptorHandle> qhwi::D3dDescriptorHeapManager::getHandle()
{
    size_t numHeaps = m_descriptorHeap.size();

    while (m_descriptorHeapSearchIndex < numHeaps)
    {
        std::optional<D3dDescriptorHandle> handle = m_descriptorHeap[m_descriptorHeapSearchIndex].getHandle();

        if (handle.has_value())
        {
            return handle;
        }

        ++m_descriptorHeapSearchIndex;
    }

    // All heaps are full - Create a new heap
    m_descriptorHeap.emplace_back().init(m_device, m_desc);
    return m_descriptorHeap[m_descriptorHeapSearchIndex].getHandle();
}

std::optional<qhwi::D3dDescriptorTable> qhwi::D3dDescriptorHeapManager::getDescriptorTable(uint32_t numDescriptors)
{
    size_t numHeaps = m_descriptorHeap.size();

    while (m_descriptorHeapSearchIndex < numHeaps)
    {
        std::optional<qhwi::D3dDescriptorTable> descriptorTable = m_descriptorHeap[m_descriptorHeapSearchIndex].getDescriptorTable(numDescriptors);
        if (descriptorTable.has_value())
        {
            return descriptorTable;
        }

        ++m_descriptorHeapSearchIndex;
    }

    // All heaps are full - Create a new heap
    m_descriptorHeap.emplace_back().init(m_device, m_desc);
    return m_descriptorHeap[m_descriptorHeapSearchIndex].getDescriptorTable(numDescriptors);
}

std::optional<qhwi::D3dDescriptorHandle> qhwi::D3dDescriptorHeapManager::stage(D3dDescriptorHandle src)
{
    size_t numHeaps = m_descriptorHeap.size();

    while (m_descriptorHeapSearchIndex < numHeaps)
    {
        std::optional<qhwi::D3dDescriptorHandle> dst = m_descriptorHeap[m_descriptorHeapSearchIndex].stage(src);
        if (dst.has_value())
        {
            return dst;
        }

        ++m_descriptorHeapSearchIndex;
    }

    // All heaps are full - Create a new heap
    m_descriptorHeap.emplace_back().init(m_device, m_desc);
    return m_descriptorHeap[m_descriptorHeapSearchIndex].stage(src);
}

std::optional<qhwi::D3dDescriptorHandle> qhwi::D3dDescriptorHeapManager::stage(const D3dDescriptorTable& descriptorTable,
                                                                               const D3dDescriptorHandle& src,
                                                                               uint32_t descriptorIndex) const
{
    qhwi::D3dDescriptorHandle dst = descriptorTable.baseHandle;
    auto offset = descriptorIndex * m_descriptorHandleIncrementSize;
    dst.cpuHandle.ptr += offset;

    if (m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        dst.gpuHandle.ptr += offset;
    }

    m_device->get()->CopyDescriptorsSimple(1, dst.cpuHandle, src.cpuHandle, m_desc.Type);
    return dst;
}

void qhwi::D3dDescriptorHeapManager::releaseHandle(D3dDescriptorHandle handle)
{
    size_t numDescriptorHeaps = m_descriptorHeap.size();
    for (uint32_t index = 0; index < numDescriptorHeaps; ++index)
    {
        if (m_descriptorHeap[index].releaseHandle(handle))
        {
            m_descriptorHeapSearchIndex = index;
            break;
        }
    }
}

void qhwi::D3dDescriptorHeapManager::releaseDescriptorTable(const D3dDescriptorTable& descriptorTable)
{
    size_t numDescriptorHeaps = m_descriptorHeap.size();
    for (uint32_t index = 0; index < numDescriptorHeaps; ++index)
    {
        if (m_descriptorHeap[index].releaseDescriptorTable(descriptorTable))
        {
            m_descriptorHeapSearchIndex = index;
            break;
        }
    }
}

int qhwi::D3dHeapAllocator::alloc(uint32_t descriptorCount)
{
    uint32_t freeSlots = 0;
    int index = -1;

    for (uint32_t i = m_scanStartIndex; i < m_numDescriptors; ++i)
    {
        if (m_isAllocated[i])
        {
            freeSlots = 0;
        }
        else
        {
            ++freeSlots;
        }

        if (freeSlots == descriptorCount)
        {
            index = i - descriptorCount + 1;
            break;
        }
    }

    if (freeSlots == descriptorCount)
    {
        m_scanStartIndex = index + descriptorCount;
        m_currentAllocations += descriptorCount;

        for (uint32_t i = index; i < m_scanStartIndex; ++i)
        {
            m_isAllocated[i] = true;
        }
    }

    return index;
}

bool qhwi::D3dHeapAllocator::release(int index, uint32_t descriptorCount)
{
    if (index >= 0 && (index + descriptorCount) <= m_numDescriptors)
    {
        for (uint32_t i = uint32_t(index); i < uint32_t(index) + descriptorCount; ++i)
        {
            QHWI_ASSERT_MSG(m_isAllocated[i] == true, "Trying to free unallocated slot");
            m_isAllocated[i] = false;
        }

        if (m_scanStartIndex > uint32_t(index))
        {
            m_scanStartIndex = index;
        }

        m_currentAllocations -= descriptorCount;

        return true;
    }
    else
    {
        return false;
    }
}

qhwi::D3dUniqueDescriptorTable::~D3dUniqueDescriptorTable()
{
    if (m_heap && m_descriptorTable.numDescriptors)
    {
        m_heap->releaseDescriptorTable(m_descriptorTable);
    }

    if (m_heapManager && m_descriptorTable.numDescriptors)
    {
        m_heapManager->releaseDescriptorTable(m_descriptorTable);
    }
}

void qhwi::D3dUniqueDescriptorTable::reset(D3dDescriptorHeap* heap, const D3dDescriptorTable& descriptorTable, D3dDescriptorHeapManager* heapManager)
{
    m_heap = heap;
    m_heapManager = heapManager;
    m_descriptorTable = descriptorTable;
}

void qhwi::D3dFeaturesInfo::init(ID3D12Device* device)
{
    options = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS>(device, D3D12_FEATURE_D3D12_OPTIONS);
    options1 = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS1>(device, D3D12_FEATURE_D3D12_OPTIONS1);
    options3 = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS3>(device, D3D12_FEATURE_D3D12_OPTIONS3);
    options4 = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS4>(device, D3D12_FEATURE_D3D12_OPTIONS4);
    options5 = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS5>(device, D3D12_FEATURE_D3D12_OPTIONS5);
    options7 = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS7>(device, D3D12_FEATURE_D3D12_OPTIONS7);
    options12 = checkFeatureSupport<D3D12_FEATURE_DATA_D3D12_OPTIONS12>(device, D3D12_FEATURE_D3D12_OPTIONS12);
    architecture = checkFeatureSupport<D3D12_FEATURE_DATA_ARCHITECTURE>(device, D3D12_FEATURE_ARCHITECTURE);
    shaderModel = checkFeatureSupport<D3D12_FEATURE_DATA_SHADER_MODEL>(device, D3D12_FEATURE_SHADER_MODEL);
}

qhwi::D3dDevice::D3dDevice(D3dContext* ctx, ID3D12Device* device, IDXGIAdapter1* adapter, const Description& desc)
    : Device(ctx)
    , m_desc(desc)
{
    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice = device;
    allocatorDesc.pAdapter = adapter;
    allocatorDesc.Flags = (D3D12MA::ALLOCATOR_FLAGS)D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

    HRESULT hr = CreateAllocator(&allocatorDesc, &m_dmaAllocator);
    if (!SUCCEEDED(hr))
    {
        QHWI_FATAL("Warning: unable to create dma allocator, result: %x", hr);
    }

    // create custom memory pool for sharing
    D3D12MA::POOL_DESC poolDesc = {};
    poolDesc.HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    poolDesc.Flags = D3D12MA::POOL_FLAG_ALWAYS_COMMITTED;
    poolDesc.HeapFlags = D3D12MA_RECOMMENDED_HEAP_FLAGS | D3D12_HEAP_FLAG_SHARED;
    m_dmaAllocator->CreatePool(&poolDesc, &m_sharedAllocationsPool);

    V(device->QueryInterface(IID_PPV_ARGS(&m_device)));
    setName(m_device.Get(), m_desc.base.name);
    createDescriptorHeaps();
    createResourceViewCaches();
    initNullDescriptors();
    m_timerQueryHeap.init(this, "timerQueryHeap");

    m_hD3d12 = LoadLibraryA("d3d12.dll");
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    m_storageFactory = qre::MakeRefCountPtr<compressedstorage::Factory>(new compressedstorage::D3dFactory());

    if (static_cast<compressedstorage::D3dFactory*>(m_storageFactory.Get())->init() != ResultCode::eSuccess)
    {
        m_storageFactory = nullptr;
        QHWI_LOGW("Failed to get IDStorageFactory");
    }
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

    if (m_hD3d12)
    {
        if (!D3dRootSignature::initD3D12SerializeVersionedRootSignature(m_hD3d12))
        {
            QHWI_LOGW("Failed to get address of D3D12SerializeVersionedRootSignature");
        }
    }
    else
    {
        QHWI_LOGW("Failed to load d3d12.dll");
    }

    if (m_context && m_context->desc()->debug)
    {
        ComPtr<ID3D12InfoQueue> infoQueue;
        V(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)));
        V(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE));
        if (m_context->desc()->enableD3dDebugMessageCallback)
        {
            ComPtr<ID3D12InfoQueue1> infoQueue1;
            if SUCCEEDED (infoQueue->QueryInterface(IID_PPV_ARGS(&infoQueue1)))
            {
                DWORD callbackCookie = 0;
                hr = infoQueue1->RegisterMessageCallback(d3d12MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &callbackCookie);
                if (!SUCCEEDED(hr))
                {
                    QHWI_LOGW("Warning: register D3D12 message callback failed, result: %x", hr);
                }
            }
        }
    }

    // Create command signature for draw, drawIndexed and dispatch
    createCommandSignatures();

    // query features
    m_d3dFeaturesInfo.init(device);

    setFeature(Feature_ConservativeRasterization, m_d3dFeaturesInfo.supportsConservativeRasterization());
    setFeature(Feature_MeshShaders, m_d3dFeaturesInfo.supportsMeshShaders());
    setFeature(Feature_RayPipelines, m_d3dFeaturesInfo.supportsRayPipelines());
    setFeature(Feature_RayQuery, m_d3dFeaturesInfo.supportsRayQuery());
    setFeature(Feature_RtTraceRaysIndirect, m_d3dFeaturesInfo.supportsDispatchRaysIndirect());
    setFeature(Feature_RtInvocationReorder, m_d3dFeaturesInfo.supportsRayTracingInvocationReorder());
    setFeature(Feature_ShaderFloat16, m_d3dFeaturesInfo.supportsShaderFloat16());
    setFeature(Feature_TextureCompressionBc, true);
    setFeature(Feature_TextureCompressionEtc, false);
    setFeature(Feature_TextureCompressionAstc, false);
    setFeature(Feature_HdrMetadata, true);
    setFeature(Feature_Barycentric, m_d3dFeaturesInfo.supportsBarycentrics());
    m_supportsEnhancedBarriers = m_d3dFeaturesInfo.supportsEnhancedBarriers();
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    setFeature(Feature_StorageQueue, m_storageFactory != nullptr);
#else
    setFeature(Feature_StorageQueue, false);
#endif // QHWI_ENABLE_COMPRESSED_STORAGE
    setFeature(Feature_OpacityMicromaps, m_d3dFeaturesInfo.supportsOpacityMicromaps());

    m_rayTracingPipelineProperties = {};
    if (m_d3dFeaturesInfo.supportsRayPipelines())
    {
        m_rayTracingPipelineProperties = { .shaderGroupHandleSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                                           .shaderGroupHandleAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT,
                                           .shaderGroupBaseAlignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT };
    }

    // populate limits data structure
    m_limitsInfo.maxAnisotropy = D3D12_REQ_MAXANISOTROPY;
    m_limitsInfo.maxTextureDimension1D = D3D12_REQ_TEXTURE1D_U_DIMENSION;
    m_limitsInfo.maxTextureDimension2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    m_limitsInfo.maxTextureDimension3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    m_limitsInfo.maxTextureDimensionCube = D3D12_REQ_TEXTURECUBE_DIMENSION;
    m_limitsInfo.maxRenderTargets = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
    m_limitsInfo.minWaveSize = m_d3dFeaturesInfo.minWaveSize();
    m_limitsInfo.maxWaveSize = m_d3dFeaturesInfo.maxWaveSize();
}

bool qhwi::D3dDevice::isTimerQueryResultReady(TimerQuery* timerQuery)
{
    DWORD result = WaitForSingleObjectEx(static_cast<D3dTimerQuery*>(timerQuery)->cmdListCompletedEvent, 0, FALSE);

    if (result == WAIT_OBJECT_0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool qhwi::D3dDevice::getTimerQueryTimeInSec(TimerQuery* timerQuery, CommandQueue* commandQueue, bool blockUntilReady, float& timeInSec)
{
    QHWI_ASSERT(timerQuery->timerState == TimerState::eUnresolved);

    auto d3dTimerQuery = static_cast<D3dTimerQuery*>(timerQuery);

    bool isResultReady = d3dTimerQuery->lastSubmittedID <= commandQueue->getLastCompletedID();

    if (!isResultReady)
    {
        DWORD milliseconds = blockUntilReady ? INFINITE : 0;
        isResultReady = (WaitForSingleObjectEx(d3dTimerQuery->cmdListCompletedEvent, milliseconds, FALSE) == WAIT_OBJECT_0) ? true : false;
    }

    if (isResultReady)
    {
        constexpr uint32_t queryCount = 2; // { BeginQuery, EndQuery }
        uint64_t frequency;
        auto d3dCommandQueue = static_cast<D3dCommandQueue*>(commandQueue);
        d3dCommandQueue->get()->GetTimestampFrequency(&frequency);

        D3D12_RANGE bufferReadRange = { timerQuery->beginQueryIndex * sizeof(uint64_t), (timerQuery->beginQueryIndex + queryCount) * sizeof(uint64_t) };

        uint64_t* data;

        V(m_timerQueryHeap.getQueryDataResolveBuffer()->Map(0, &bufferReadRange, (void**)&data));
        timeInSec = float(double(data[timerQuery->endQueryIndex] - data[timerQuery->beginQueryIndex]) / double(frequency));
        m_timerQueryHeap.getQueryDataResolveBuffer()->Unmap(0, nullptr);

        timerQuery->timerState = TimerState::eResolved;

        return true;
    }
    else
    {
        return false;
    }
}

qre::RefCountPtr<qhwi::TimerQuery> qhwi::D3dDevice::createTimerQuery()
{
    int32_t startTimerIndex = m_timerQueryHeap.allocate() * 2;
    int32_t endTimerIndex = startTimerIndex + 1;

    return qre::MakeRefCountPtr<qhwi::TimerQuery>((startTimerIndex >= 0) ? new D3dTimerQuery(&m_timerQueryHeap, startTimerIndex, endTimerIndex) : nullptr);
}

size_t qhwi::D3dDevice::getConstantBufferOffsetAlignment() const
{
    return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
}

qhwi::FormatSupportMask qhwi::D3dDevice::queryFormatSupport(Format format) const
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupportData{};
    formatSupportData.Format = d3d(format);

    FormatSupportMask result = qhwi::FormatSupport_None;

    V(m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupportData, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

    if (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_BUFFER)
    {
        result |= FormatSupport_Buffer;
    }

    if ((formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE1D) || (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) ||
        (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE3D))
    {
        result |= FormatSupport_StorageImage;
    }

    if (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
    {
        result |= FormatSupport_DepthStencilAttachment;
    }

    if (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)
    {
        result |= FormatSupport_ColorAttachment;
    }

    if (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_IA_INDEX_BUFFER)
    {
        result |= FormatSupport_IndexBuffer;
    }

    if (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_IA_VERTEX_BUFFER)
    {
        result |= FormatSupport_VertexBuffer;
    }

    if (formatSupportData.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)
    {
        result |= FormatSupport_SampledImage;
    }

    return result;
}

qhwi::GraphicsBackend qhwi::D3dDevice::getGraphicsBackend() const
{
    return qhwi::GraphicsBackend::eDirectX12;
}

qhwi::D3dContext* qhwi::D3dDevice::getD3dContext()
{
    return static_cast<D3dContext*>(m_context);
}

void qhwi::D3dDevice::createDescriptorHeaps()
{
    uint32_t numCbvSrvUavDescriptors = MaxCbvDescriptors + MaxSrvDescriptors + MaxUavDescriptors;

    m_rtvDescriptorHeap.init(this, { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, MaxRtvDescriptors }, "rtvDescriptorHeap");
    m_cbvSrvUavShaderVisibleDescriptorHeap.init(this,
                                                { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, MaxDescriptorHeapDescriptors },
                                                "cbvSrvUavShaderVisibleDescriptorHeap");
    m_cbvSrvUavDescriptorHeapManager.init(this, { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, numCbvSrvUavDescriptors }, "cbvSrvUavDescriptorHeap");
    m_samplerDescriptorHeap.init(this, { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, MaxSamplerDescriptors }, "samplerDescriptorHeap");
    m_samplerShaderVisibleDescriptorHeap.init(this,
                                              { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, MaxSamplerDescriptors },
                                              "samplerShaderVisibleDescriptorHeap");
    m_dsvDescriptorHeap.init(this, { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, MaxDsvDescriptors }, "dsvDescriptorHeap");
}

void qhwi::D3dDevice::createResourceViewCaches()
{
    m_srvCache.init(this);
    m_rtvCache.init(this);
    m_dsvCache.init(this);
    m_uavCache.init(this);
}

void qhwi::D3dDevice::initNullDescriptors()
{
    auto descriptorHeap = getCbvSrvUavDescriptorHeap();
    auto d3dDevice = get();

    // Create null buffer SRV descriptor
    {
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_UINT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = 1;
        srvDesc.Buffer.StructureByteStride = 0;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        d3dDevice->CreateShaderResourceView(nullptr, &srvDesc, descriptorHandle->cpuHandle);
        m_nullBufferSrvDescriptor = *descriptorHandle;
    }

    // Create null Texture2D SRV descriptor
    {
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        d3dDevice->CreateShaderResourceView(nullptr, &srvDesc, descriptorHandle->cpuHandle);
        m_nullTexture2DSrvDescriptor = *descriptorHandle;
    }

    // Create null buffer UAV descriptor
    {
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_R32_UINT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = 1;
        uavDesc.Buffer.StructureByteStride = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        d3dDevice->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, descriptorHandle->cpuHandle);
        m_nullBufferUavDescriptor = *descriptorHandle;
    }

    // Create null Texture2D UAV descriptor
    {
        std::optional<D3dDescriptorHandle> descriptorHandle = descriptorHeap->getHandle();
        QHWI_ASSERT_MSG(descriptorHandle.has_value(), "Invalid descriptor handle");

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;

        d3dDevice->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, descriptorHandle->cpuHandle);
        m_nullTexture2DUavDescriptor = *descriptorHandle;
    }
}

qre::RefCountPtr<qhwi::PipelineLayout> qhwi::D3dDevice::createPipelineLayout(const PipelineLayout::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::PipelineLayout>(new D3dPipelineLayout(this, desc));
}

void qhwi::D3dDevice::createCommandSignatures()
{
    D3D12_COMMAND_SIGNATURE_DESC desc{};
    D3D12_INDIRECT_ARGUMENT_DESC argDesc{};

    desc.NodeMask = 0;
    desc.NumArgumentDescs = 1;
    desc.pArgumentDescs = &argDesc;

    desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    V(m_device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_commandSignatureDraw)));
    setName(m_commandSignatureDraw.Get(), "commandSignatureDraw");

    desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    V(m_device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_commandSignatureDrawIndexed)));
    setName(m_commandSignatureDrawIndexed.Get(), "commandSignatureDrawIndexed");

    desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
    argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    V(m_device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_commandSignatureDispatch)));
    setName(m_commandSignatureDispatch.Get(), "commandSignatureDispatch");

    if (m_d3dFeaturesInfo.supportsDispatchRaysIndirect())
    {
        desc.ByteStride = sizeof(D3D12_DISPATCH_RAYS_DESC);
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;
        V(m_device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_commandSignatureDispatchRays)));
        setName(m_commandSignatureDispatchRays.Get(), "commandSignatureDispatchRays");
    }
}

const qhwi::D3dDescriptorHandle& qhwi::D3dDescriptorSet::getBindingBaseDescriptorHandle(uint32_t index)
{
    if (!m_isStaged)
    {
        createDescriptorTables();
        m_isStaged = true;
    }

    return m_baseDescriptorHandles[index];
}

void qhwi::D3dDescriptorSet::resetShaderResources()
{
    if (m_isStaged)
    {
        auto srvUavshaderVisibleDescriptorHeap = m_device->getCbvSrvUavShaderVisibleDescriptorHeap();
        auto samplerDescriptorHeap = m_device->getSamplerShaderVisibleDescriptorHeap();

        srvUavshaderVisibleDescriptorHeap->releaseDescriptorTable(m_srvUavDescriptorTable.get());
        samplerDescriptorHeap->releaseDescriptorTable(m_samplerDescriptorTable.get());

        for (D3dDescriptorHandle& handle : m_baseDescriptorHandles)
        {
            handle = {};
        }

        m_isStaged = false;
    }
}

void qhwi::D3dDescriptorSet::createDescriptorTables()
{
    const auto& bindings = m_desc.base.bindings;
    const size_t numBindings = bindings.size();

    auto srvUavShaderVisibleDescriptorHeap = m_device->getCbvSrvUavShaderVisibleDescriptorHeap();
    auto samplerDescriptorHeap = m_device->getSamplerShaderVisibleDescriptorHeap();

    uint32_t cbvSrvUavDescriptorIdx = 0;
    uint32_t samplerDescriptorIdx = 0;

    m_baseDescriptorHandles.resize(numBindings);
    m_numSrvUav = 0;
    m_numSampler = 0;

    for (size_t i = 0; i < numBindings; ++i)
    {
        const auto& binding = bindings[i];
        if (!binding)
        {
            continue;
        }
        switch (binding->type())
        {
        case DescriptorType::eByteAddressBuffer:
        case DescriptorType::eRWByteAddressBuffer:
        case DescriptorType::eTexture:
        case DescriptorType::eRWTexture:
        case DescriptorType::eInputAttachment:
        case DescriptorType::eStructuredBuffer:
        case DescriptorType::eRWStructuredBuffer:
        case DescriptorType::eTypedBuffer:
        case DescriptorType::eRWTypedBuffer:
            m_numSrvUav += binding->count();
            break;

        case DescriptorType::eSampler:
            m_numSampler += binding->count();
            break;

        default:
            break;
        }
    }

    std::optional<D3dDescriptorTable> srvUavDescriptorTable = m_numSrvUav ? srvUavShaderVisibleDescriptorHeap->getDescriptorTable(m_numSrvUav) : std::nullopt;
    QHWI_ASSERT_MSG((m_numSrvUav == 0) || (m_numSrvUav > 0 && srvUavDescriptorTable.has_value()), "Invalid descriptor table");

    std::optional<D3dDescriptorTable> samplerDescriptorTable = m_numSampler ? samplerDescriptorHeap->getDescriptorTable(m_numSampler) : std::nullopt;
    QHWI_ASSERT_MSG((m_numSampler == 0) || (m_numSampler > 0 && samplerDescriptorTable.has_value()), "Invalid descriptor table");

    m_srvUavDescriptorTable.reset(srvUavShaderVisibleDescriptorHeap, srvUavDescriptorTable.value_or(D3dDescriptorTable()));
    m_samplerDescriptorTable.reset(samplerDescriptorHeap, samplerDescriptorTable.value_or(D3dDescriptorTable()));

    for (uint32_t i = 0; i < numBindings; ++i)
    {
        const auto& binding = bindings[i];
        if (!binding)
        {
            continue;
        }
        uint32_t count = binding->count();
        auto bindingType = binding->m_type;

        if (bindingType == DescriptorType::eTexture || bindingType == DescriptorType::eInputAttachment)
        {
            for (uint32_t k = 0; k < count; ++k)
            {
                const auto& textureInfo = binding->textureInfo(k);
                auto d3dTexture = static_cast<D3dTexture*>(textureInfo.texture.Get());

                D3dDescriptorHandle srvDescriptor;
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                // getSrvDescriptor will create the SRV descriptor on demand for non-null textures.
                if (d3dTexture)
                {
                    if (textureInfo.subresourceRange.isEntireResource())
                    {
                        srvDesc = d3dTexture->defaultSrvDesc();
                    }
                    else
                    {
                        srvDesc = qhwi::d3dShaderResourceViewDesc(d3dTexture, textureInfo.subresourceRange);
                    }
                    srvDescriptor = m_device->getSrvDescriptor(d3dTexture->get(), &srvDesc);
                }
                else
                {
                    // Use pre-created null Texture2D SRV descriptor
                    srvDescriptor = m_device->getNullTexture2DSrvDescriptor();
                }

                std::optional<D3dDescriptorHandle> srvShaderVisibleDescriptor =
                    srvUavShaderVisibleDescriptorHeap->stage(m_srvUavDescriptorTable.get(), srvDescriptor, cbvSrvUavDescriptorIdx++);
                QHWI_ASSERT_MSG(srvShaderVisibleDescriptor.has_value(), "Invalid descriptor handle");

                if (k == 0)
                {
                    m_baseDescriptorHandles[i] = *srvShaderVisibleDescriptor;
                }
            }
        }
        else if (bindingType == DescriptorType::eSampler)
        {
            for (uint32_t k = 0; k < count; ++k)
            {
                const auto& samplerInfo = binding->samplerInfo(k);
                auto d3dSampler = static_cast<D3dSampler*>(samplerInfo.sampler.Get());
                D3dDescriptorHandle samplerDescriptor = d3dSampler->getDescriptor();
                std::optional<D3dDescriptorHandle> samplerShaderVisibleDescriptor =
                    samplerDescriptorHeap->stage(m_samplerDescriptorTable.get(), samplerDescriptor, samplerDescriptorIdx++);
                QHWI_ASSERT_MSG(samplerShaderVisibleDescriptor.has_value(), "Invalid descriptor handle");

                if (k == 0)
                {
                    m_baseDescriptorHandles[i] = *samplerShaderVisibleDescriptor;
                }
            }
        }
        else if (bindingType == DescriptorType::eStructuredBuffer || bindingType == DescriptorType::eByteAddressBuffer || bindingType == DescriptorType::eTypedBuffer)
        {
            for (uint32_t k = 0; k < count; k++)
            {
                const auto& bufferInfo = binding->bufferInfo(k);
                auto d3dBuffer = static_cast<D3dBuffer*>(bufferInfo.buffer.Get());

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                D3dDescriptorHandle srvDescriptor;
                if (d3dBuffer)
                {
                    srvDesc = qhwi::d3dShaderResourceViewDesc(d3dBuffer, bufferInfo.offset, bufferInfo.range, bufferInfo.stride, bindingType);
                    srvDescriptor = m_device->getSrvDescriptor(d3dBuffer->get(), &srvDesc);
                }
                else
                {
                    // Use pre-created null Buffer SRV descriptor
                    srvDescriptor = m_device->getNullBufferSrvDescriptor();
                }
                std::optional<D3dDescriptorHandle> srvShaderVisibleDescriptor =
                    srvUavShaderVisibleDescriptorHeap->stage(m_srvUavDescriptorTable.get(), srvDescriptor, cbvSrvUavDescriptorIdx++);
                QHWI_ASSERT_MSG(srvShaderVisibleDescriptor.has_value(), "Invalid descriptor handle");

                if (k == 0)
                {
                    m_baseDescriptorHandles[i] = *srvShaderVisibleDescriptor;
                }
            }
        }
        else if (bindingType == DescriptorType::eRWStructuredBuffer || bindingType == DescriptorType::eRWByteAddressBuffer || bindingType == DescriptorType::eRWTypedBuffer)
        {
            for (uint32_t k = 0; k < count; k++)
            {
                const auto& bufferInfo = binding->bufferInfo(k);
                auto d3dBuffer = static_cast<D3dBuffer*>(bufferInfo.buffer.Get());
                D3dDescriptorHandle uavDescriptor;

                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
                if (d3dBuffer)
                {
                    uavDesc = qhwi::d3dUnorderedAccessViewDesc(d3dBuffer, bufferInfo.offset, bufferInfo.range, bufferInfo.stride, bindingType);
                    uavDescriptor = m_device->getUavDescriptor(d3dBuffer->get(), &uavDesc);
                }
                else
                {
                    // Use pre-created null Buffer UAV descriptor
                    uavDescriptor = m_device->getNullBufferUavDescriptor();
                }

                std::optional<D3dDescriptorHandle> uavShaderVisibleDescriptor =
                    srvUavShaderVisibleDescriptorHeap->stage(m_srvUavDescriptorTable.get(), uavDescriptor, cbvSrvUavDescriptorIdx++);
                QHWI_ASSERT_MSG(uavShaderVisibleDescriptor.has_value(), "Invalid descriptor handle");

                if (k == 0)
                {
                    m_baseDescriptorHandles[i] = *uavShaderVisibleDescriptor;
                }
            }
        }
        else if (bindingType == DescriptorType::eRWTexture)
        {
            for (uint32_t k = 0; k < count; k++)
            {
                const auto& textureInfo = binding->textureInfo(k);
                auto d3dTexture = static_cast<D3dTexture*>(textureInfo.texture.Get());

                D3dDescriptorHandle uavDescriptor;
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
                if (d3dTexture)
                {
                    if (textureInfo.subresourceRange.isEntireResource())
                    {
                        uavDesc = d3dTexture->defaultUavDesc();
                    }
                    else
                    {
                        uavDesc = qhwi::d3dUnorderedAccessViewDesc(d3dTexture, textureInfo.subresourceRange);
                    }
                    uavDescriptor = m_device->getUavDescriptor(d3dTexture->get(), &uavDesc);
                }
                else
                {
                    // Use pre-created null Texture2D UAV descriptor
                    uavDescriptor = m_device->getNullTexture2DUavDescriptor();
                }
                std::optional<D3dDescriptorHandle> uavShaderVisibleDescriptor =
                    srvUavShaderVisibleDescriptorHeap->stage(m_srvUavDescriptorTable.get(), uavDescriptor, cbvSrvUavDescriptorIdx++);
                QHWI_ASSERT_MSG(uavShaderVisibleDescriptor.has_value(), "Invalid descriptor handle");

                if (k == 0)
                {
                    m_baseDescriptorHandles[i] = *uavShaderVisibleDescriptor;
                }
            }
        }
    }
}

qre::RefCountPtr<qhwi::ComputePipeline> qhwi::D3dDevice::createComputePipeline(const ComputePipeline::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::ComputePipeline>(new D3dComputePipeline(this, desc));
}

qre::RefCountPtr<qhwi::GraphicsPipeline> qhwi::D3dDevice::createGraphicsPipeline(const GraphicsPipeline::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::GraphicsPipeline>(new D3dGraphicsPipeline(this, desc));
}

qre::RefCountPtr<qhwi::RayTracingPipeline> qhwi::D3dDevice::createRayTracingPipeline(const qhwi::RayTracingPipeline::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::RayTracingPipeline>(new qhwi::D3dRayTracingPipeline(this, desc));
}

qre::RefCountPtr<qhwi::Buffer> qhwi::D3dDevice::createBuffer(const Buffer::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Buffer>(new D3dBuffer(this, desc));
}

qre::RefCountPtr<qhwi::Texture> qhwi::D3dDevice::createTexture(const Texture::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Texture>(new D3dTexture(this, desc));
}

qre::RefCountPtr<qhwi::Texture> qhwi::D3dDevice::createTextureFromD3d12Resource(const Texture::Description& desc, ID3D12Resource* resource)
{
    return qre::MakeRefCountPtr<qhwi::Texture>(new D3dTexture(this, desc, resource));
}

qre::RefCountPtr<qhwi::Sampler> qhwi::D3dDevice::createSampler(const Sampler::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Sampler>(new D3dSampler(this, desc));
}

qre::RefCountPtr<qhwi::DescriptorSet> qhwi::D3dDevice::createDescriptorSet(const DescriptorSet::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::DescriptorSet>(new D3dDescriptorSet(this, desc));
}

qre::RefCountPtr<qhwi::DescriptorSetLayout> qhwi::D3dDevice::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
{
    return qre::MakeRefCountPtr<qhwi::DescriptorSetLayout>(new D3dDescriptorSetLayout(desc));
}

void qhwi::D3dDevice::updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSetWrite* writes, uint32_t numWrites)
{
    static_cast<D3dDescriptorSet*>(descriptorSet)->resetShaderResources();
    Device::updateDescriptorSet(descriptorSet, writes, numWrites);
}

qre::RefCountPtr<qhwi::Shader> qhwi::D3dDevice::createShader(const Shader::Description& desc, const void* byteCode, size_t length)
{
    return qre::MakeRefCountPtr<qhwi::Shader>(new D3dShader(this, desc, byteCode, length));
}

qre::RefCountPtr<qhwi::CommandList> qhwi::D3dDevice::createCommandList(CommandQueue* commandQueue, const CommandList::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::CommandList>(new D3dCommandList(this, desc));
}

qre::RefCountPtr<qhwi::CommandQueue> qhwi::D3dDevice::createCommandQueue(const CommandQueue::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::CommandQueue>(new D3dCommandQueue(this, desc));
}

qhwi::ext::Swapchain* qhwi::D3dDevice::createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc)
{
    auto d3dCommandQueue = static_cast<D3dCommandQueue*>(commandQueue);
    return new ext::DxgiSwapchain(this, d3dCommandQueue, desc);
}

qre::RefCountPtr<qhwi::Fence> qhwi::D3dDevice::createFence(FenceState initialState, const char* name)
{
    qre::RefCountPtr<qhwi::Fence> p;
    p.Attach(new D3dFence(this, initialState, name));

    return p;
}

qre::RefCountPtr<qhwi::AccelerationStructure> qhwi::D3dDevice::createAccelerationStructure(const AccelerationStructure::Description& desc)
{
    qre::RefCountPtr<qhwi::AccelerationStructure> p;
    p.Attach(new D3dAccelerationStructure(this, desc));

    return p;
}

size_t qhwi::D3dDevice::getBottomLevelScratchSize(size_t descriptorCount, AccelerationStructureBuildMode, AccelerationStructureBuildFlags)
{
    return sizeof(D3D12_RAYTRACING_GEOMETRY_DESC) * descriptorCount;
}

qhwi::AccelerationStructureBuildInfo qhwi::D3dDevice::getAccelerationStructureBuildInfo(const BottomLevelDescriptor* bottomLevelDescriptors,
                                                                                        size_t count,
                                                                                        void* scratchMemory,
                                                                                        AccelerationStructureBuildMode buildMode,
                                                                                        AccelerationStructureBuildFlags flags)
{
    if (!supportsRayTracing())
    {
        QHWI_FATAL("The device does not support ray tracing.");
    }

    if (count == 0)
    {
        return AccelerationStructureBuildInfo();
    }

    // Array of geometry descriptors, one per BLAS
    D3D12_RAYTRACING_GEOMETRY_DESC* descriptors = reinterpret_cast<D3D12_RAYTRACING_GEOMETRY_DESC*>(scratchMemory);

    for (size_t i = 0; i < count; i++)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC& d3dDescriptor(descriptors[i]);
        const BottomLevelDescriptor& inputDescriptor(bottomLevelDescriptors[i]);

        // TODO: Support bounding boxes
        // TODO: Support BLAS transforms
        d3dDescriptor = D3D12_RAYTRACING_GEOMETRY_DESC();
        d3dDescriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        d3dDescriptor.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        d3dDescriptor.Triangles.Transform3x4 = NULL;
        d3dDescriptor.Triangles.VertexCount = inputDescriptor.triangles.vertexCount;
        d3dDescriptor.Triangles.VertexFormat = d3d(inputDescriptor.triangles.vertexFormat);
        d3dDescriptor.Triangles.VertexBuffer.StartAddress =
            static_cast<D3dBuffer*>(inputDescriptor.triangles.vertexBuffer)->getDeviceAddress() + inputDescriptor.triangles.vertexOffset;
        d3dDescriptor.Triangles.VertexBuffer.StrideInBytes = inputDescriptor.triangles.vertexStride;

        if (inputDescriptor.triangles.indexBuffer)
        {
            d3dDescriptor.Triangles.IndexCount = inputDescriptor.triangles.indexCount;
            d3dDescriptor.Triangles.IndexFormat = d3d(inputDescriptor.triangles.indexFormat);
            d3dDescriptor.Triangles.IndexBuffer = static_cast<D3dBuffer*>(inputDescriptor.triangles.indexBuffer)->getDeviceAddress() + inputDescriptor.triangles.indexOffset;
        }
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs{};

    buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildInputs.NumDescs = UINT(count);
    buildInputs.Flags = qhwi::d3dRaytracingAccelerationStructureBuildFlags(buildMode, flags);
    buildInputs.pGeometryDescs = descriptors;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

    qhwi::AccelerationStructureBuildInfo buildInfo;

    buildInfo.accelerationStructureSize = prebuildInfo.ResultDataMaxSizeInBytes;
    buildInfo.buildScratchSize = prebuildInfo.ScratchDataSizeInBytes;
    buildInfo.updateScratchSize = prebuildInfo.UpdateScratchDataSizeInBytes;

    return buildInfo;
}

qre::RefCountPtr<qhwi::Buffer> qhwi::D3dDevice::createTopLevelInstanceBuffer(const TopLevelDescriptor* topLevelDescriptors, AccelerationStructure** blasArray, size_t count)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dDevice::createTopLevelInstanceBuffer", QHWI_COLOR_RAYTRACING);
    if (count == 0)
    {
        return qre::RefCountPtr<qhwi::Buffer>();
    }

    Buffer::Description instanceBufferDesc;
    instanceBufferDesc.heapType = HeapType::eUpload;
    instanceBufferDesc.size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * count;
    instanceBufferDesc.name = "InstanceBuffer";

    qre::RefCountPtr<qhwi::Buffer> instanceBuffer = createBuffer(instanceBufferDesc);

    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescriptors = (D3D12_RAYTRACING_INSTANCE_DESC*)instanceBuffer->map();

    for (size_t i = 0; i < count; i++)
    {
        D3dAccelerationStructure* blas = static_cast<D3dAccelerationStructure*>(blasArray[topLevelDescriptors[i].blasIndex]);

        std::memcpy(instanceDescriptors[i].Transform, topLevelDescriptors[i].transform, sizeof(topLevelDescriptors[i].transform));
        instanceDescriptors[i].InstanceID = topLevelDescriptors[i].instanceID;
        instanceDescriptors[i].InstanceMask = topLevelDescriptors[i].instanceMask;
        instanceDescriptors[i].InstanceContributionToHitGroupIndex = topLevelDescriptors[i].instanceContributionToHitGroupIndex;
        instanceDescriptors[i].Flags = topLevelDescriptors[i].flags;

        instanceDescriptors[i].AccelerationStructure = blas->getDeviceAddress();
    }

    instanceBuffer->unmap();

    return instanceBuffer;
}

qhwi::AccelerationStructureBuildInfo qhwi::D3dDevice::getAccelerationStructureBuildInfo(size_t instanceCount,
                                                                                        AccelerationStructureBuildMode buildMode,
                                                                                        AccelerationStructureBuildFlags flags)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("D3dDevice::getAccelerationStructureBuildInfo", QHWI_COLOR_RAYTRACING);

    if (!supportsRayTracing())
    {
        QHWI_FATAL("The device does not support ray tracing.");
    }

    if (instanceCount == 0)
    {
        return AccelerationStructureBuildInfo();
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs{};

    buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    buildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildInputs.NumDescs = UINT(instanceCount);
    buildInputs.Flags = qhwi::d3dRaytracingAccelerationStructureBuildFlags(buildMode, flags);
    buildInputs.InstanceDescs = 0;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

    qhwi::AccelerationStructureBuildInfo buildInfo;

    buildInfo.accelerationStructureSize = prebuildInfo.ResultDataMaxSizeInBytes;
    buildInfo.buildScratchSize = prebuildInfo.ScratchDataSizeInBytes;
    buildInfo.updateScratchSize = prebuildInfo.UpdateScratchDataSizeInBytes;

    return buildInfo;
}

qhwi::ResultCode qhwi::D3dDevice::getRayTracingShaderGroupHandles(const qhwi::RayTracingPipeline* pipeline,
                                                                  uint32_t startGroup,
                                                                  uint32_t groupCount,
                                                                  size_t dstSize,
                                                                  void* dst) const
{
    const D3dRayTracingPipeline* d3dRayTracingPipeline = static_cast<const D3dRayTracingPipeline*>(pipeline);

    return d3dRayTracingPipeline->getRayTracingShaderGroupHandles(startGroup, groupCount, dstSize, dst);
}

void qhwi::D3dTimerQueryHeap::init(Device* device, const char* name)
{
    auto d3dDevice = static_cast<D3dDevice*>(device)->get();

    D3D12_QUERY_HEAP_DESC queryHeapDesc = { .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = MaxTimerQueryHeapSize };

    V(d3dDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap)));

    freeList.Init(MaxTimerQueries);

    queryDataResolveBuffer = device->createBuffer({ .heapType = HeapType::eDownload, .size = MaxTimerQueryHeapSize * sizeof(uint64_t), .name = "queryDataResolveBuffer" });
    setName(queryHeap.Get(), name);
}

void qhwi::D3dContext::enumerateDevices(const EnumerateDeviceParams& params, EnumerateDeviceResults& enumerateDeviceResults)
{
    UINT adapterIndex = 0;
    ComPtr<IDXGIAdapter1> dxgiAdapter;
    D3D_FEATURE_LEVEL featureLevel = params.featureLevel;
    DXGI_GPU_PREFERENCE gpuPreference = params.gpuPreference;
    auto* results = enumerateDeviceResults.results;
    int& numResults = enumerateDeviceResults.numResults;

    while (true)
    {
        HRESULT hResult;
        hResult = m_dxgiFactory->EnumAdapterByGpuPreference(adapterIndex, gpuPreference, IID_PPV_ARGS(&dxgiAdapter));
        if (hResult == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc;
        dxgiAdapter->GetDesc1(&desc);
        const char* vendorName = getVendorName(desc.VendorId);
        QHWI_LOGI("Available D3D12 Adapters[%d]. Vendor: %s, Device ID: 0x%04x, Device Name: %ls.", adapterIndex, vendorName, desc.DeviceId, desc.Description);
        ++adapterIndex;
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }
        hResult = D3D12CreateDevice(dxgiAdapter.Get(), featureLevel, _uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hResult))
        {
            EnumerateDeviceResult deviceResult;
            deviceResult.adapter = dxgiAdapter;
            deviceResult.desc = desc;
            *results++ = deviceResult;
            numResults++;
            if (numResults > MaxDevices)
            {
                QHWI_FATAL("number of devices found (%d) > MaxDevices (%d)", numResults, MaxDevices);
            }
        }
    }
    if (numResults == 0)
    {
        QHWI_FATAL("no adapter found that supports feature level: %d", params.featureLevel);
    }
}

qre::RefCountPtr<qhwi::Device> qhwi::D3dContext::createDevice(const Device::Description& deviceDesc)
{
    EnumerateDeviceParams p;
    EnumerateDeviceResults devices;

    enumerateDevices(p, devices);
    const auto& selectedDevice = devices.results[0];
    const auto& desc = selectedDevice.desc;

    LARGE_INTEGER driverVersion;
    selectedDevice.adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driverVersion);

    QHWI_LOGI("Selected D3D12 Device: Vendor: %s, Device ID: 0x%04x, Device Name: %ls, Driver Version %llu.",
              getVendorName(desc.VendorId),
              desc.DeviceId,
              desc.Description,
              driverVersion.QuadPart);
    return createDevice(selectedDevice.adapter.Get(), p.featureLevel, deviceDesc);
}

qre::RefCountPtr<qhwi::Device> qhwi::D3dContext::createDevice(IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL featureLevel, const DeviceDesc& deviceDesc)
{
    D3dDevice::Description d3dDeviceDesc(adapter, deviceDesc);
    d3dDeviceDesc.d3d.featureLevel = featureLevel;

    ComPtr<ID3D12Device> device;
    V(D3D12CreateDevice(adapter, d3dDeviceDesc.d3d.featureLevel, IID_PPV_ARGS(&device)));

    return qre::MakeRefCountPtr<qhwi::Device>(new qhwi::D3dDevice(this, device.Get(), adapter, d3dDeviceDesc));
}

qhwi::D3dContext::D3dContext(const Description& desc)
    : m_desc(desc)
{
}

qhwi::ResultCode qhwi::D3dContext::init()
{
    UINT dxgiFactoryFlags = 0;
    ResultCode result = ResultCode::eSuccess;

    m_hD3d12 = LoadLibraryA("d3d12.dll");
    if (!m_hD3d12)
    {
        result = ResultCode::eErrorOther;
        QHWI_LOGW("Failed to load d3d12.dll");
    }
    else
    {
        D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(m_hD3d12, "D3D12CreateDevice");
        D3D12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(m_hD3d12, "D3D12GetDebugInterface");
        auto pfnD3d12SerializeVerionedRootSignature = D3dRootSignature::initD3D12SerializeVersionedRootSignature(m_hD3d12);

#ifdef QHWI_USE_AGILITY_SDK_717_PREVIEW

        D3D12EnableExperimentalFeatures = (PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES)GetProcAddress(m_hD3d12, "D3D12EnableExperimentalFeatures");
        if (D3D12EnableExperimentalFeatures != NULL)
        {
            // Enable Experimental Shader Models (no config structs needed)
            const IID experimentalFeatures[] = { D3D12ExperimentalShaderModels };
            HRESULT hRes = D3D12EnableExperimentalFeatures(1u,
                                                           experimentalFeatures,
                                                           nullptr, // pConfigurationStructs
                                                           nullptr  // pConfigurationStructSizes
            );

            if (FAILED(hRes))
            {
                QHWI_LOGW("Failed to enable D3D12ExperimentalShaderModels");
                return ResultCode::eErrorOther;
            }
        }

#endif // QHWI_USE_AGILITY_SDK_717_PREVIEW

        bool initSuccess = D3D12CreateDevice && D3D12GetDebugInterface && pfnD3d12SerializeVerionedRootSignature;
        if (!initSuccess)
        {
            result = ResultCode::eErrorOther;
            QHWI_LOGW("Failed to get address of DX12 functions");
        }
    }

    m_hDxgi = LoadLibraryA("dxgi.dll");
    if (!m_hDxgi)
    {
        result = ResultCode::eErrorOther;
        QHWI_LOGW("Failed to load dxgi.dll");
    }
    else
    {
        CreateDXGIFactory2 = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(m_hDxgi, "CreateDXGIFactory2");
        GetDebugInterface1 = (PFN_GET_DEBUG_INTERFACE1)GetProcAddress(m_hDxgi, "DXGIGetDebugInterface1");

        bool initSuccess = CreateDXGIFactory2 && GetDebugInterface1;
        if (!initSuccess)
        {
            result = ResultCode::eErrorOther;
            QHWI_LOGW("Failed to get address of DXGI functions");
        }
    }

    if (result == ResultCode::eSuccess)
    {
        if (m_desc.base.debug)
        {
            V(D3D12GetDebugInterface(IID_PPV_ARGS(&m_d3d12Debug)));
            m_d3d12Debug->EnableDebugLayer();
            if (m_desc.base.enableGPUValidation)
            {
                m_d3d12Debug->SetEnableGPUBasedValidation(true);
            }
        }
        V(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory)));
    }

    return result;
}

void qhwi::D3dContext::reportLiveObjects()
{
    if (m_desc.base.debug)
    {
        ComPtr<IDXGIDebug1> dxgiDebug;
        V(GetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));
        V(dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_ALL)));
    }
}

qhwi::D3dContext::~D3dContext()
{
    if (m_hDxgi)
    {
        FreeLibrary(m_hDxgi);
    }

    if (m_hD3d12)
    {
        FreeLibrary(m_hD3d12);
    }
    reportLiveObjects();
}

uint32_t qhwi::d3d(Texture* texture, const Subresource* s)
{
    QHWI_ASSERT(texture);
    return D3D12CalcSubresource(s ? s->mipSlice : 0, s ? s->arraySlice : 0, s ? s->planeSlice : 0, texture->desc()->mipLevels, texture->desc()->arrayLayers);
}

D3D12_RENDER_TARGET_VIEW_DESC qhwi::d3dRenderTargetViewDesc(Texture* texture, const SubresourceRange& s)
{
    QHWI_ASSERT(texture);
    auto d3dTexture = static_cast<D3dTexture*>(texture);
    D3D12_RENDER_TARGET_VIEW_DESC desc = d3dTexture->defaultRtvDesc();

    SampleCount numSamples = d3dTexture->desc()->numSamples;

    switch (d3dTexture->desc()->type)
    {
    case TextureType::e1D:
    {
        auto& p0 = desc.Texture1D;
        p0.MipSlice = s.start.mipSlice;
        break;
    }

    case TextureType::e1DArray:
    {
        auto& p0 = desc.Texture1DArray;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.MipSlice = s.start.mipSlice;
        break;
    }

    case TextureType::e2D:
    {
        if (numSamples == SampleCount::e1)
        {
            auto& p0 = desc.Texture2D;
            p0.MipSlice = s.start.mipSlice;
            p0.PlaneSlice = s.start.planeSlice;
        }
        break;
    }

    case TextureType::e2DArray:
    {
        if (numSamples == SampleCount::e1)
        {
            auto& p0 = desc.Texture2DArray;
            p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
            p0.FirstArraySlice = s.start.arraySlice;
            p0.MipSlice = s.start.mipSlice;
        }
        else
        {
            auto& p0 = desc.Texture2DMSArray;
            p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
            p0.FirstArraySlice = s.start.arraySlice;
        }
        break;
    }

    case TextureType::eCube:
    {
        auto& p0 = desc.Texture2DArray;
        p0.ArraySize = 1;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.MipSlice = s.start.mipSlice;
        p0.PlaneSlice = s.start.planeSlice;
        break;
    }

    case TextureType::e3D:
    {
        auto& p0 = desc.Texture3D;
        p0.FirstWSlice = s.start.arraySlice;
        p0.MipSlice = s.start.mipSlice;
        p0.WSize = 1;
        break;
    }

    default:
        QHWI_TODO();
    }
    return desc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC qhwi::d3dDepthStencilViewDesc(Texture* texture, const SubresourceRange& s)
{
    QHWI_ASSERT(texture);
    auto d3dTexture = static_cast<D3dTexture*>(texture);
    D3D12_DEPTH_STENCIL_VIEW_DESC desc = d3dTexture->defaultDsvDesc();
    SampleCount numSamples = d3dTexture->desc()->numSamples;
    switch (d3dTexture->desc()->type)
    {
    case TextureType::e1D:
    {
        auto& p0 = desc.Texture1D;
        p0.MipSlice = s.start.mipSlice;
        break;
    }

    case TextureType::e1DArray:
    {
        auto& p0 = desc.Texture1DArray;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.MipSlice = s.start.mipSlice;
        break;
    }

    case TextureType::e2D:
    {
        if (numSamples == SampleCount::e1)
        {
            auto& p0 = desc.Texture2D;
            p0.MipSlice = s.start.mipSlice;
        }
        break;
    }

    case TextureType::e2DArray:
    {
        if (numSamples == SampleCount::e1)
        {
            auto& p0 = desc.Texture2DArray;
            p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
            p0.FirstArraySlice = s.start.arraySlice;
            p0.MipSlice = s.start.mipSlice;
        }
        else
        {
            auto& p0 = desc.Texture2DMSArray;
            p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
            p0.FirstArraySlice = s.start.arraySlice;
        }
        break;
    }

    case TextureType::eCube:
    {
        auto& p0 = desc.Texture2DArray;
        p0.ArraySize = 1;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.MipSlice = s.start.mipSlice;
        break;
    }

    default:
        QHWI_TODO();
    }
    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC qhwi::d3dShaderResourceViewDesc(Texture* texture, const SubresourceRange& s)
{
    QHWI_ASSERT(texture);
    auto d3dTexture = static_cast<D3dTexture*>(texture);
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = d3dTexture->defaultSrvDesc();

    switch (desc.ViewDimension)
    {
    case D3D12_SRV_DIMENSION_TEXTURE1D:
    {
        auto& p0 = desc.Texture1D;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
    {
        auto& p0 = desc.Texture1DArray;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURE2D:
    {
        auto& p0 = desc.Texture2D;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.PlaneSlice = s.start.planeSlice;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
    {
        auto& p0 = desc.Texture2DArray;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.PlaneSlice = s.start.planeSlice;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURECUBE:
    {
        auto& p0 = desc.TextureCube;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
    {
        auto& p0 = desc.TextureCubeArray;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURE3D:
    {
        auto& p0 = desc.Texture3D;
        p0.MostDetailedMip = s.start.mipSlice;
        p0.MipLevels = s.end.mipSlice - s.start.mipSlice + 1;
        p0.ResourceMinLODClamp = 0.0;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
    {
        auto& p0 = desc.Texture2DMSArray;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        break;
    }

    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
        // Intentionally left blank - Nothing to define
        break;

    default:
    {
        QHWI_FATAL("Texture view dimension unsupported");
    }
    }

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC qhwi::d3dShaderResourceViewDesc(Buffer* buffer, uint64_t offset, uint64_t range, uint64_t stride, DescriptorType descriptorType)
{
    QHWI_ASSERT(buffer);
    QHWI_ASSERT(offset < buffer->size());

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};

    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    size_t sizeInBytes = (range == 0) ? (buffer->size() - offset) : range;

    switch (descriptorType)
    {
    case DescriptorType::eStructuredBuffer:
        QHWI_ASSERT(stride != 0);
        QHWI_ASSERT(offset % stride == 0);
        QHWI_ASSERT(range % stride == 0);
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.FirstElement = offset / stride;
        desc.Buffer.NumElements = UINT(sizeInBytes / stride);
        desc.Buffer.StructureByteStride = UINT(stride);
        break;

    case DescriptorType::eByteAddressBuffer:
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.Buffer.FirstElement = offset >> 2;
        desc.Buffer.NumElements = UINT(sizeInBytes >> 2);
        desc.Buffer.StructureByteStride = 0;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        break;
    case DescriptorType::eTypedBuffer:
    {
        Format fmt = buffer->format();
        auto fmtInfo = FormatUtil::getFormatInfo(fmt);
        uint8_t bytesPerElement = fmtInfo.bytesPerBlock;
        desc.Format = d3d(buffer->format());
        desc.Buffer.FirstElement = offset / bytesPerElement;
        desc.Buffer.NumElements = UINT(sizeInBytes / bytesPerElement);
        desc.Buffer.StructureByteStride = 0;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        break;
    }

    default:
        QHWI_FATAL("Invalid descriptor type");
    }

    return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC qhwi::d3dUnorderedAccessViewDesc(Buffer* buffer, uint64_t offset, uint64_t range, uint64_t stride, DescriptorType descriptorType)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

    if (buffer == nullptr)
    {
        desc.Format = DXGI_FORMAT_R32_UINT;
        return desc;
    }
    else
    {
        QHWI_ASSERT(offset < buffer->size());

        size_t sizeInBytes = sizeInBytes = (range == 0) ? (buffer->size() - offset) : range;

        switch (descriptorType)
        {
        case DescriptorType::eRWStructuredBuffer:
            QHWI_ASSERT(stride != 0);
            QHWI_ASSERT(offset % stride == 0);
            QHWI_ASSERT(range % stride == 0);
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Buffer.FirstElement = offset / stride;
            desc.Buffer.NumElements = UINT(sizeInBytes / stride);
            desc.Buffer.StructureByteStride = UINT(stride);
            break;

        case DescriptorType::eRWByteAddressBuffer:
            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.Buffer.FirstElement = offset >> 2;
            desc.Buffer.NumElements = UINT(sizeInBytes >> 2);
            desc.Buffer.StructureByteStride = 0;
            desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;

        case DescriptorType::eRWTypedBuffer:
        {
            Format fmt = buffer->format();
            auto fmtInfo = FormatUtil::getFormatInfo(fmt);
            uint8_t bytesPerElement = fmtInfo.bytesPerBlock;
            desc.Format = d3d(buffer->format());
            desc.Buffer.FirstElement = offset / bytesPerElement;
            desc.Buffer.NumElements = UINT(sizeInBytes / bytesPerElement);
            desc.Buffer.StructureByteStride = 0;
            break;
        }

        default:
            QHWI_FATAL("Invalid descriptor type");
        }

        return desc;
    }
}

D3D12_UNORDERED_ACCESS_VIEW_DESC qhwi::d3dUnorderedAccessViewDesc(Texture* texture, const SubresourceRange& s)
{
    QHWI_ASSERT(texture);
    auto d3dTexture = static_cast<D3dTexture*>(texture);
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = d3dTexture->defaultUavDesc();
    switch (texture->desc()->type)
    {
    case TextureType::e1D:
    {
        auto& p0 = desc.Texture1D;
        p0.MipSlice = s.start.mipSlice;
        break;
    }
    case TextureType::e1DArray:
    {
        auto& p0 = desc.Texture1DArray;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        p0.MipSlice = s.start.mipSlice;
        p0.FirstArraySlice = s.start.arraySlice;
        break;
    }
    case qhwi::TextureType::e2D:
    {
        auto& p0 = desc.Texture2D;
        p0.PlaneSlice = s.start.planeSlice;
        p0.MipSlice = s.start.mipSlice;
    }
    break;
    case qhwi::TextureType::e2DArray:
    case qhwi::TextureType::eCube:
    case TextureType::eCubeArray:
    {
        auto& p0 = desc.Texture2DArray;
        p0.FirstArraySlice = s.start.arraySlice;
        p0.ArraySize = s.end.arraySlice - s.start.arraySlice + 1;
        p0.PlaneSlice = s.start.planeSlice;
        p0.MipSlice = s.start.mipSlice;
    }
    break;
    case qhwi::TextureType::e3D:
    {
        auto& p0 = desc.Texture3D;
        p0.FirstWSlice = s.start.arraySlice;
        p0.MipSlice = s.start.mipSlice;
        p0.WSize = s.end.arraySlice - s.start.arraySlice + 1;
    }
    break;
    case TextureType::eUndefined:
        QHWI_ERR("Undefined texture type");
        break;
    }
    return desc;
}

#define EQ(n) (a.n == b.n)

constexpr bool qhwi::operator==(const D3D12_BUFFER_RTV& a, const D3D12_BUFFER_RTV& b)
{
    return (EQ(FirstElement) && EQ(NumElements));
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_RTV& a, const D3D12_TEX1D_RTV& b)
{
    return EQ(MipSlice);
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_ARRAY_RTV& a, const D3D12_TEX1D_ARRAY_RTV& b)
{
    return (EQ(ArraySize) && EQ(FirstArraySlice) && EQ(MipSlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_RTV& a, const D3D12_TEX2D_RTV& b)
{
    return (EQ(MipSlice) && EQ(PlaneSlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_ARRAY_RTV& a, const D3D12_TEX2D_ARRAY_RTV& b)
{
    return (EQ(MipSlice) && EQ(FirstArraySlice) && EQ(ArraySize) && EQ(PlaneSlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_RTV& a, const D3D12_TEX2DMS_RTV& b)
{
    return true;
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_ARRAY_RTV& a, const D3D12_TEX2DMS_ARRAY_RTV& b)
{
    return (EQ(ArraySize) && EQ(FirstArraySlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX3D_RTV& a, const D3D12_TEX3D_RTV& b)
{
    return (EQ(FirstWSlice) && EQ(MipSlice) && EQ(WSize));
}

bool qhwi::operator==(const D3D12_RENDER_TARGET_VIEW_DESC& a, const D3D12_RENDER_TARGET_VIEW_DESC& b)
{
    if (!(EQ(ViewDimension) && EQ(Format)))
    {
        return false;
    }
    switch (a.ViewDimension)
    {
    case D3D12_RTV_DIMENSION_UNKNOWN:
    default:
        return true;
    case D3D12_RTV_DIMENSION_BUFFER:
        return (a.Buffer == b.Buffer);
    case D3D12_RTV_DIMENSION_TEXTURE1D:
        return (a.Texture1D == b.Texture1D);
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
        return (a.Texture1DArray == b.Texture1DArray);
    case D3D12_RTV_DIMENSION_TEXTURE2D:
        return (a.Texture2D == b.Texture2D);
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
        return (a.Texture2DArray == b.Texture2DArray);
    case D3D12_RTV_DIMENSION_TEXTURE2DMS:
        return (a.Texture2DMS == b.Texture2DMS);
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
        return (a.Texture2DMSArray == b.Texture2DMSArray);
    case D3D12_RTV_DIMENSION_TEXTURE3D:
        return (a.Texture3D == b.Texture3D);
    }
}

constexpr bool qhwi::operator==(const D3D12_BUFFER_SRV& a, const D3D12_BUFFER_SRV& b)
{
    return (EQ(FirstElement) && EQ(NumElements) && EQ(StructureByteStride) && EQ(Flags));
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_SRV& a, const D3D12_TEX1D_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_ARRAY_SRV& a, const D3D12_TEX1D_ARRAY_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(FirstArraySlice) && EQ(ArraySize) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_SRV& a, const D3D12_TEX2D_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(PlaneSlice) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_ARRAY_SRV& a, const D3D12_TEX2D_ARRAY_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(FirstArraySlice) && EQ(ArraySize) && EQ(PlaneSlice) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_SRV& a, const D3D12_TEX2DMS_SRV& b)
{
    return true;
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_ARRAY_SRV& a, const D3D12_TEX2DMS_ARRAY_SRV& b)
{
    return (EQ(FirstArraySlice) && EQ(ArraySize));
}

constexpr bool qhwi::operator==(const D3D12_TEX3D_SRV& a, const D3D12_TEX3D_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_TEXCUBE_SRV& a, const D3D12_TEXCUBE_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_TEXCUBE_ARRAY_SRV& a, const D3D12_TEXCUBE_ARRAY_SRV& b)
{
    return (EQ(MostDetailedMip) && EQ(MipLevels) && EQ(First2DArrayFace) && EQ(NumCubes) && EQ(ResourceMinLODClamp));
}

constexpr bool qhwi::operator==(const D3D12_BUFFER_UAV& a, const D3D12_BUFFER_UAV& b)
{
    return (EQ(FirstElement) && EQ(NumElements) && EQ(StructureByteStride) && EQ(CounterOffsetInBytes) && EQ(Flags));
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_UAV& a, const D3D12_TEX1D_UAV& b)
{
    return EQ(MipSlice);
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_ARRAY_UAV& a, const D3D12_TEX1D_ARRAY_UAV& b)
{
    return (EQ(MipSlice) && EQ(FirstArraySlice) && EQ(ArraySize));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_UAV& a, const D3D12_TEX2D_UAV& b)
{
    return (EQ(MipSlice) && EQ(PlaneSlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_ARRAY_UAV& a, const D3D12_TEX2D_ARRAY_UAV& b)
{
    return (EQ(MipSlice) && EQ(FirstArraySlice) && EQ(ArraySize) && EQ(PlaneSlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_UAV& a, const D3D12_TEX2DMS_UAV& b)
{
    return true;
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_ARRAY_UAV& a, const D3D12_TEX2DMS_ARRAY_UAV& b)
{
    return (EQ(FirstArraySlice) && EQ(ArraySize));
}

constexpr bool qhwi::operator==(const D3D12_TEX3D_UAV& a, const D3D12_TEX3D_UAV& b)
{
    return (EQ(MipSlice) && EQ(FirstWSlice) && EQ(WSize));
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_DSV& a, const D3D12_TEX1D_DSV& b)
{
    return EQ(MipSlice);
}

constexpr bool qhwi::operator==(const D3D12_TEX1D_ARRAY_DSV& a, const D3D12_TEX1D_ARRAY_DSV& b)
{
    return (EQ(MipSlice) && EQ(FirstArraySlice) && EQ(ArraySize));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_DSV& a, const D3D12_TEX2D_DSV& b)
{
    return (EQ(MipSlice));
}

constexpr bool qhwi::operator==(const D3D12_TEX2D_ARRAY_DSV& a, const D3D12_TEX2D_ARRAY_DSV& b)
{
    return (EQ(MipSlice) && EQ(FirstArraySlice) && EQ(ArraySize));
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_DSV& a, const D3D12_TEX2DMS_DSV& b)
{
    return true;
}

constexpr bool qhwi::operator==(const D3D12_TEX2DMS_ARRAY_DSV& a, const D3D12_TEX2DMS_ARRAY_DSV& b)
{
    return (EQ(FirstArraySlice) && EQ(ArraySize));
}

constexpr bool qhwi::operator==(const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV& a, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV& b)
{
    return (EQ(Location));
}

bool qhwi::operator==(const D3D12_SHADER_RESOURCE_VIEW_DESC& a, const D3D12_SHADER_RESOURCE_VIEW_DESC& b)
{
    if (!(EQ(Format) && EQ(ViewDimension) && EQ(Shader4ComponentMapping)))
    {
        return false;
    }
    switch (a.ViewDimension)
    {
    case D3D12_SRV_DIMENSION_UNKNOWN:
    default:
        return true;
    case D3D12_SRV_DIMENSION_BUFFER:
        return (a.Buffer == b.Buffer);
    case D3D12_SRV_DIMENSION_TEXTURE1D:
        return (a.Texture1D == b.Texture1D);
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
        return (a.Texture1DArray == b.Texture1DArray);
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        return (a.Texture2D == b.Texture2D);
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        return (a.Texture2DArray == b.Texture2DArray);
    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
        return (a.Texture2DMS == b.Texture2DMS);
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
        return (a.Texture2DMSArray == b.Texture2DMSArray);
    case D3D12_SRV_DIMENSION_TEXTURE3D:
        return (a.Texture3D == b.Texture3D);
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
        return (a.TextureCube == b.TextureCube);
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
        return (a.TextureCubeArray == b.TextureCubeArray);
    case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
        return (a.RaytracingAccelerationStructure == b.RaytracingAccelerationStructure);
    }
}

bool qhwi::operator==(const D3D12_UNORDERED_ACCESS_VIEW_DESC& a, const D3D12_UNORDERED_ACCESS_VIEW_DESC& b)
{
    if (!(EQ(Format) && EQ(ViewDimension)))
    {
        return false;
    }
    switch (a.ViewDimension)
    {
    case D3D12_UAV_DIMENSION_UNKNOWN:
    default:
        return true;
    case D3D12_UAV_DIMENSION_BUFFER:
        return (a.Buffer == b.Buffer);
    case D3D12_UAV_DIMENSION_TEXTURE1D:
        return (a.Texture1D == b.Texture1D);
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
        return (a.Texture1DArray == b.Texture1DArray);
    case D3D12_UAV_DIMENSION_TEXTURE2D:
        return (a.Texture2D == b.Texture2D);
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
        return (a.Texture2DArray == b.Texture2DArray);
    case D3D12_UAV_DIMENSION_TEXTURE3D:
        return (a.Texture3D == b.Texture3D);
    }
}

bool qhwi::operator==(const D3D12_DEPTH_STENCIL_VIEW_DESC& a, const D3D12_DEPTH_STENCIL_VIEW_DESC& b)
{
    if (!(EQ(Format) && EQ(ViewDimension) && EQ(Flags)))
    {
        return false;
    }
    switch (a.ViewDimension)
    {
    case D3D12_DSV_DIMENSION_UNKNOWN:
    default:
        return true;
    case D3D12_DSV_DIMENSION_TEXTURE1D:
        return (a.Texture1D == b.Texture1D);
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
        return (a.Texture1DArray == b.Texture1DArray);
    case D3D12_DSV_DIMENSION_TEXTURE2D:
        return (a.Texture2D == b.Texture2D);
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
        return (a.Texture2DArray == b.Texture2DArray);
    case D3D12_DSV_DIMENSION_TEXTURE2DMS:
        return (a.Texture2DMS == b.Texture2DMS);
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
        return (a.Texture2DMSArray == b.Texture2DMSArray);
    }
}

uint64_t qhwi::hash_util::Fnv1a64Continue(uint64_t& hash, const D3D12_UNORDERED_ACCESS_VIEW_DESC& v)
{
    qre::Fnv1a64Continue(hash, v.Format);
    qre::Fnv1a64Continue(hash, v.ViewDimension);

    switch (v.ViewDimension)
    {
    case D3D12_UAV_DIMENSION_UNKNOWN:
    default:
        return hash;

    case D3D12_UAV_DIMENSION_BUFFER:
        qre::Fnv1a64Continue(hash, v.Buffer.CounterOffsetInBytes);
        qre::Fnv1a64Continue(hash, v.Buffer.FirstElement);
        qre::Fnv1a64Continue(hash, v.Buffer.Flags);
        qre::Fnv1a64Continue(hash, v.Buffer.NumElements);
        qre::Fnv1a64Continue(hash, v.Buffer.StructureByteStride);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE1D:
        qre::Fnv1a64Continue(hash, v.Texture1D.MipSlice);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture1DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.MipSlice);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE2D:
        qre::Fnv1a64Continue(hash, v.Texture2D.MipSlice);
        qre::Fnv1a64Continue(hash, v.Texture2D.PlaneSlice);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.MipSlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.PlaneSlice);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE2DMS:
        qre::Fnv1a64Continue(hash, v.Texture2DMS.UnusedField_NothingToDefine);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.FirstArraySlice);
        return hash;

    case D3D12_UAV_DIMENSION_TEXTURE3D:
        qre::Fnv1a64Continue(hash, v.Texture3D.FirstWSlice);
        qre::Fnv1a64Continue(hash, v.Texture3D.MipSlice);
        qre::Fnv1a64Continue(hash, v.Texture3D.WSize);
        return hash;
    }
}

uint64_t qhwi::hash_util::Fnv1a64Continue(uint64_t& hash, const D3D12_RENDER_TARGET_VIEW_DESC& v)
{
    qre::Fnv1a64Continue(hash, v.Format);
    qre::Fnv1a64Continue(hash, v.ViewDimension);

    switch (v.ViewDimension)
    {
    case D3D12_RTV_DIMENSION_UNKNOWN:
    default:
        return hash;

    case D3D12_RTV_DIMENSION_BUFFER:
        qre::Fnv1a64Continue(hash, v.Buffer.FirstElement);
        qre::Fnv1a64Continue(hash, v.Buffer.NumElements);
        return hash;

    case D3D12_RTV_DIMENSION_TEXTURE1D:
        qre::Fnv1a64Continue(hash, v.Texture1D.MipSlice);
        return hash;

    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture1DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.MipSlice);
        return hash;

    case D3D12_RTV_DIMENSION_TEXTURE2D:
        qre::Fnv1a64Continue(hash, v.Texture2D.MipSlice);
        qre::Fnv1a64Continue(hash, v.Texture2D.PlaneSlice);
        return hash;

    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.MipSlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.PlaneSlice);
        return hash;

    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.FirstArraySlice);
        return hash;

    case D3D12_RTV_DIMENSION_TEXTURE3D:
        qre::Fnv1a64Continue(hash, v.Texture3D.FirstWSlice);
        qre::Fnv1a64Continue(hash, v.Texture3D.MipSlice);
        qre::Fnv1a64Continue(hash, v.Texture3D.WSize);
        return hash;
    }
}

uint64_t qhwi::hash_util::Fnv1a64Continue(uint64_t& hash, const D3D12_SHADER_RESOURCE_VIEW_DESC& v)
{
    qre::Fnv1a64Continue(hash, v.Format);
    qre::Fnv1a64Continue(hash, v.ViewDimension);
    qre::Fnv1a64Continue(hash, v.Shader4ComponentMapping);

    switch (v.ViewDimension)
    {
    case D3D12_SRV_DIMENSION_UNKNOWN:
    default:
        return hash;

    case D3D12_SRV_DIMENSION_BUFFER:
        qre::Fnv1a64Continue(hash, v.Buffer.FirstElement);
        qre::Fnv1a64Continue(hash, v.Buffer.Flags);
        qre::Fnv1a64Continue(hash, v.Buffer.NumElements);
        qre::Fnv1a64Continue(hash, v.Buffer.StructureByteStride);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURE1D:
        qre::Fnv1a64Continue(hash, v.Texture1D.MipLevels);
        qre::Fnv1a64Continue(hash, v.Texture1D.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.Texture1D.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture1DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.MipLevels);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURE2D:
        qre::Fnv1a64Continue(hash, v.Texture2D.MipLevels);
        qre::Fnv1a64Continue(hash, v.Texture2D.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.Texture2D.PlaneSlice);
        qre::Fnv1a64Continue(hash, v.Texture2D.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.MipLevels);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.PlaneSlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.FirstArraySlice);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURE3D:
        qre::Fnv1a64Continue(hash, v.Texture3D.MipLevels);
        qre::Fnv1a64Continue(hash, v.Texture3D.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.Texture3D.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURECUBE:
        qre::Fnv1a64Continue(hash, v.TextureCube.MipLevels);
        qre::Fnv1a64Continue(hash, v.TextureCube.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.TextureCube.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
        qre::Fnv1a64Continue(hash, v.TextureCubeArray.First2DArrayFace);
        qre::Fnv1a64Continue(hash, v.TextureCubeArray.MipLevels);
        qre::Fnv1a64Continue(hash, v.TextureCubeArray.MostDetailedMip);
        qre::Fnv1a64Continue(hash, v.TextureCubeArray.NumCubes);
        qre::Fnv1a64Continue(hash, v.TextureCubeArray.ResourceMinLODClamp);
        return hash;

    case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
        qre::Fnv1a64Continue(hash, v.RaytracingAccelerationStructure.Location);
        return hash;
    }
}

uint64_t qhwi::hash_util::Fnv1a64Continue(uint64_t& hash, const D3D12_DEPTH_STENCIL_VIEW_DESC& v)
{
    qre::Fnv1a64Continue(hash, v.ViewDimension);
    qre::Fnv1a64Continue(hash, v.Format);

    switch (v.ViewDimension)
    {
    case D3D12_DSV_DIMENSION_UNKNOWN:
    default:
        return hash;

    case D3D12_DSV_DIMENSION_TEXTURE1D:
        qre::Fnv1a64Continue(hash, v.Texture1D.MipSlice);
        return hash;

    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture1DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture1DArray.MipSlice);
        return hash;

    case D3D12_DSV_DIMENSION_TEXTURE2D:
        qre::Fnv1a64Continue(hash, v.Texture2D.MipSlice);
        return hash;

    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.FirstArraySlice);
        qre::Fnv1a64Continue(hash, v.Texture2DArray.MipSlice);
        return hash;

    case D3D12_DSV_DIMENSION_TEXTURE2DMS:
        qre::Fnv1a64Continue(hash, v.Texture2DMS.UnusedField_NothingToDefine);
        return hash;

    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.ArraySize);
        qre::Fnv1a64Continue(hash, v.Texture2DMSArray.FirstArraySlice);
        return hash;
    }
}

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
qre::RefCountPtr<qhwi::compressedstorage::File> qhwi::D3dDevice::createStorageFile()
{
    return qre::MakeRefCountPtr<qhwi::compressedstorage::File>(new qhwi::compressedstorage::D3dFile());
}

qhwi::ResultCode qhwi::compressedstorage::D3dFactory::init()
{
    return (DStorageGetFactory(IID_PPV_ARGS(&m_factory)) == S_OK) ? ResultCode::eSuccess : ResultCode::eUnsupported;
}

qhwi::ResultCode qhwi::compressedstorage::D3dFactory::openFile(const char* fileToLoad, File* file) const
{
    wchar_t wFileToLoad[qhwi::MaxPathSize];

    qre::CharToWideChar(wFileToLoad, fileToLoad);

    QHWI_ASSERT_MSG(m_factory, "Storage factory is nullptr");
    return (m_factory->OpenFile(wFileToLoad, IID_PPV_ARGS(static_cast<D3dFile*>(file)->getAddressOf())) == S_OK) ? ResultCode::eSuccess : ResultCode::eErrorOther;
}

void qhwi::compressedstorage::D3dFile::getFileInformation(FileInfo& fileInfo) const
{
    BY_HANDLE_FILE_INFORMATION info = {};
    m_file->GetFileInformation(&info);

    fileInfo.sizeInBytes = (static_cast<size_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
}

qre::RefCountPtr<qhwi::compressedstorage::StorageQueue> qhwi::compressedstorage::D3dFactory::createQueue(const qhwi::compressedstorage::StorageQueueDesc& desc) const
{
    D3dStorageQueue* d3dStorageQueue = new qhwi::compressedstorage::D3dStorageQueue();

    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Device = desc.device ? static_cast<D3dDevice*>(desc.device)->get() : nullptr;
    queueDesc.Priority = qhwi::compressedstorage::dstoragePriority(desc.priority);
    queueDesc.SourceType = qhwi::compressedstorage::dstorageRequestSourceType(desc.sourceType);
    queueDesc.Name = desc.name;

    m_factory->CreateQueue(&queueDesc, IID_PPV_ARGS(d3dStorageQueue->getAddressOf()));

    return qre::MakeRefCountPtr<qhwi::compressedstorage::StorageQueue>(d3dStorageQueue);
}

qre::RefCountPtr<qhwi::compressedstorage::StorageStatusArray> qhwi::compressedstorage::D3dFactory::createStatusArray(uint32_t capacity, const char* name) const
{
    D3dStorageStatusArray* storageStatusArray = new D3dStorageStatusArray();

    if (m_factory->CreateStatusArray(capacity, name, IID_PPV_ARGS(storageStatusArray->getAddressOf())) != S_OK)
    {
        delete storageStatusArray;
        storageStatusArray = nullptr;
    }

    return qre::MakeRefCountPtr<qhwi::compressedstorage::StorageStatusArray>(storageStatusArray);
}

void qhwi::compressedstorage::D3dStorageQueue::enqueueRequest(const qhwi::compressedstorage::Request& request)
{
    DSTORAGE_REQUEST r{};

    r.Options.SourceType = dstorageRequestSourceType(request.sourceType);
    r.Options.DestinationType = dstorageRequestDestinationType(request.destinationType);
    r.Options.CompressionFormat = dstorageCompressionFormat(request.compressionFormat);
    r.UncompressedSize = static_cast<UINT32>(request.uncompressedSize);
    r.CancellationTag = request.cancellationTag;

    switch (request.sourceType)
    {
    case qhwi::compressedstorage::StorageRequestSourceType::eFile:
    {
        const qhwi::compressedstorage::SourceFile& sourceFile = std::get<qhwi::compressedstorage::SourceFile>(request.source);

        r.Source.File = {
            .Source = static_cast<const D3dFile*>(sourceFile.file)->get(),
            .Offset = sourceFile.offset,
            .Size = static_cast<UINT32>(sourceFile.size),
        };
    }
    break;

    case qhwi::compressedstorage::StorageRequestSourceType::eMemory:
    {
        const qhwi::compressedstorage::SourceMemory& sourceMemory = std::get<qhwi::compressedstorage::SourceMemory>(request.source);

        r.Source.Memory = { .Source = sourceMemory.sourceMemoryPtr, .Size = static_cast<UINT32>(sourceMemory.size) };
    }
    break;

    default:
        QHWI_FATAL("Invalid storage request source type");
    }

    switch (request.destinationType)
    {
    case qhwi::compressedstorage::StorageRequestDestinationType::eBuffer:
    {
        const qhwi::compressedstorage::DestinationBuffer& destinationBuffer = std::get<qhwi::compressedstorage::DestinationBuffer>(request.destination);

        r.Destination.Buffer = { .Resource = static_cast<D3dBuffer*>(destinationBuffer.buffer)->get(),
                                 .Offset = destinationBuffer.offset,
                                 .Size = static_cast<UINT32>(destinationBuffer.size) };
    }
    break;
    case qhwi::compressedstorage::StorageRequestDestinationType::eMemory:
    {
        const qhwi::compressedstorage::DestinationMemory& destinationMemory = std::get<qhwi::compressedstorage::DestinationMemory>(request.destination);

        r.Destination.Memory = { .Buffer = destinationMemory.destinationMemoryPtr, .Size = static_cast<UINT32>(destinationMemory.size) };
    }
    break;
    case qhwi::compressedstorage::StorageRequestDestinationType::eMutipleSubresources:
    {
        const qhwi::compressedstorage::DestinationMultipleSubresources& destinationSubresources =
            std::get<qhwi::compressedstorage::DestinationMultipleSubresources>(request.destination);

        r.Destination.MultipleSubresources = { .Resource = static_cast<D3dTexture*>(destinationSubresources.texture)->get(),
                                               .FirstSubresource = destinationSubresources.firstSubresource };
    }
    break;

    case qhwi::compressedstorage::StorageRequestDestinationType::eTextureRegion:
    {
        const qhwi::compressedstorage::DestinationTextureRegion& destinationTextureRegion = std::get<qhwi::compressedstorage::DestinationTextureRegion>(request.destination);

        D3D12_BOX region{};
        region.left = destinationTextureRegion.origin.x;
        region.top = destinationTextureRegion.origin.y;
        region.front = destinationTextureRegion.origin.z;
        region.right = region.left + destinationTextureRegion.extent.w;
        region.bottom = destinationTextureRegion.extent.h;
        region.back = destinationTextureRegion.extent.d;

        r.Destination.Texture = { .Resource = static_cast<D3dTexture*>(destinationTextureRegion.texture)->get(),
                                  .SubresourceIndex = destinationTextureRegion.subresourceIndex,
                                  .Region = region };
    }
    break;
    case qhwi::compressedstorage::StorageRequestDestinationType::eTiles:
        QHWI_TODO();
        QHWI_FATAL("Unsupported destination type");
        break;
    default:
        QHWI_FATAL("Invalid storage request destination type");
    }

    ++m_numEnqueuedRequests;
    m_storageQueue->EnqueueRequest(&r);
}

void qhwi::compressedstorage::D3dStorageQueue::enqueueStatus(StorageStatusArray* array, uint32_t index)
{
    m_storageQueue->EnqueueStatus(static_cast<D3dStorageStatusArray*>(array)->get(), index);
}

void qhwi::compressedstorage::D3dStorageQueue::enqueueSignal(Fence* fence, uint64_t value)
{
    m_storageQueue->EnqueueSignal(static_cast<D3dFence*>(fence)->get(), value);
}

void qhwi::compressedstorage::D3dStorageQueue::submit()
{
    m_numEnqueuedRequests = 0;
    m_storageQueue->Submit();
}

qhwi::ResultCode qhwi::compressedstorage::D3dStorageStatusArray::getResult(uint32_t index) const
{
    HRESULT result = m_storageStatusArray->GetHResult(index);

    if (result == S_OK)
    {
        return ResultCode::eSuccess;
    }
    else
    {
        QHWI_LOGW("IDStorageStatusArray->GetHResult returned: %x", result);
        return ResultCode::eErrorOther;
    }
}
#endif // QHWI_ENABLE_COMPRESSED_STORAGE
