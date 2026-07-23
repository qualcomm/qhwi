//
//
//  qhwi_d3d12.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_D3D12_H__
#define __QHWI_D3D12_H__

#include <qhwi/qhwi.h>
#include <qhwi/qhwi_d3d12_enums.h>
#include <qhwi/qhwi_d3d_render_pass_encoder.h>

#ifdef TRACY_ENABLE
#    include "Tracy/TracyD3D12.hpp"
#endif

// Silence warnings from DirectX12 SDK headers
#if defined(__clang__) || defined(__GNUC__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wswitch"
#endif
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dstorage.h>
#if defined(__clang__) || defined(__GNUC__)
#    pragma clang diagnostic pop
#endif

#include <wrl.h>
#include <memory>
#include <bitset>
#include <optional>

#include <D3D12MemAlloc.h>

#include "qhwi_d3d_internal_extensions.h"

namespace qhwi
{
using Microsoft::WRL::ComPtr;

#ifdef QHWI_USE_AGILITY_SDK_717_PREVIEW
// Typedef for D3D12EnableExperimentalFeatures
using PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES = HRESULT(WINAPI*)(UINT NumFeatures,
                                                                const IID* pIIDs,
                                                                void* pConfigurationStructs,    // usually nullptr for shader models
                                                                UINT* pConfigurationStructSizes // usually nullptr for shader models
);
#endif // QHWI_USE_AGILITY_SDK_717_PREVIEW

const D3D_FEATURE_LEVEL DefaultFeatureLevel = D3D_FEATURE_LEVEL_12_1;
class D3dContext;
class D3dDevice;
class D3dCommandQueue;
class D3dCommandList;
class D3dDescriptorHeap;
class D3dDescriptorHeapManager;
class D3dTexture;

struct D3dDescriptorHandle
{
    bool operator!() const { return (cpuHandle.ptr == 0 && gpuHandle.ptr == 0); }

    void clear()
    {
        cpuHandle = { 0 };
        gpuHandle = { 0 };
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = { 0 };
};

struct D3dDescriptorTable
{
    D3dDescriptorHandle baseHandle = {};
    uint32_t numDescriptors = 0;
};

class D3dUniqueDescriptorTable
{
public:
    void reset(D3dDescriptorHeap* heap, const D3dDescriptorTable& descriptorTable, D3dDescriptorHeapManager* heapManager = nullptr);

    const D3dDescriptorTable& get() const { return m_descriptorTable; };

    ~D3dUniqueDescriptorTable();

private:
    D3dDescriptorHeap* m_heap = nullptr;
    D3dDescriptorHeapManager* m_heapManager = nullptr;
    D3dDescriptorTable m_descriptorTable = {};
};

struct D3dUniqueDescriptorHandle
{
public:
    void reset(D3dDescriptorHeap* heap, D3dDescriptorHandle handle, D3dDescriptorHeapManager* heapManager = nullptr);
    ~D3dUniqueDescriptorHandle();
    void destroy();

    D3dDescriptorHandle get() const { return m_handle; }

private:
    D3dDescriptorHeap* m_heap = nullptr;
    D3dDescriptorHeapManager* m_heapManager = nullptr;
    D3dDescriptorHandle m_handle;
};

namespace ext
{
namespace DxgiSwapchainUtil
{
DXGI_HDR_METADATA_HDR10 hdrMetadata(DisplayGamut displayGamut);
std::optional<DXGI_OUTPUT_DESC1> getContainingOutputDesc(IDXGISwapChain* swapchain);
void initDxgiSwapChainDesc(D3dDevice* device, const Swapchain::Description& desc, DXGI_SWAP_CHAIN_DESC1& dxgiDesc);
} // namespace DxgiSwapchainUtil

class DxgiSwapchain : public Swapchain
{
public:
    DxgiSwapchain() {}

    DxgiSwapchain(D3dDevice* device, D3dCommandQueue* commandQueue, const Description& desc);
    DxgiSwapchain(D3dDevice* device, D3dCommandQueue* commandQueue, const Description& desc, const DXGI_SWAP_CHAIN_DESC1& dxgiDesc);
    ~DxgiSwapchain();
    void resize(const WindowSurface& surface) override;
    void resize(int width, int height) override;
    void acquireNextImage() override;
    void presentImage(CommandQueue*) override;
    void present(UINT syncInterval, UINT flags);
    void destroy() override;

    Texture* getCurrentImage() const override { return getColorImage(m_currentImageIndex); }

    Texture* getColorImage(uint32_t index) const;
    Format getFormat() const override;

    constexpr int getNumImages() const override { return m_numSwapchainImages; }

    D3D12_CPU_DESCRIPTOR_HANDLE getAttachmentDescriptor(int index) const;
    DXGI_SWAP_CHAIN_DESC1 getDesc1() const;

    HRESULT setHdrMetadata(const DXGI_HDR_METADATA_HDR10& hdrMetadata)
    {
        return m_swapchain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), (void*)&hdrMetadata);
    }

    bool supportsColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace, UINT flags = DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) const
    {
        UINT colorSpaceSupport = 0;
        if (!SUCCEEDED(m_swapchain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)))
        {
            return false;
        }
        return (colorSpaceSupport & flags);
    }

    void setColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace);

    std::optional<DXGI_OUTPUT_DESC1> getContainingOutputDesc() const { return qhwi::ext::DxgiSwapchainUtil::getContainingOutputDesc(m_swapchain.Get()); }

    IDXGISwapChain4* get() { return m_swapchain.Get(); }

    const IDXGISwapChain4* get() const { return m_swapchain.Get(); }

protected:
    void init(D3dDevice* device, D3dCommandQueue* commandQueue, const Description& desc, const DXGI_SWAP_CHAIN_DESC1& dsDesc);
    void chooseSurfaceFormat(DXGI_SWAP_CHAIN_DESC1& dxgiSwapchainDesc);
    void getBuffers(const char* name, const DXGI_SWAP_CHAIN_DESC1& dxgiSwapchainDesc, bool automaticBarriers);
    void createRenderTargetViews(const DXGI_SWAP_CHAIN_DESC1& dxgiSwapchainDesc);
    D3dDevice* m_device = nullptr;
    ComPtr<IDXGISwapChain4> m_swapchain;
    qre::RefCountPtr<D3dTexture> m_colorSwapchainTextures[MaxSwapchainImages];
    D3dUniqueDescriptorHandle m_rtvDescriptorHandles[MaxSwapchainImages];
    uint32_t m_numSwapchainImages = 0;
    DXGI_FORMAT m_colorFormat = DXGI_FORMAT_UNKNOWN;
    bool m_autoBarrier = false;
    CommandQueue* m_commandQueue = nullptr;
};
} // namespace ext

void setName(ID3D12Object* object, const char* name);

constexpr bool operator==(const D3D12_BUFFER_RTV& a, const D3D12_BUFFER_RTV& b);
constexpr bool operator==(const D3D12_TEX1D_RTV& a, const D3D12_TEX1D_RTV& b);
constexpr bool operator==(const D3D12_TEX1D_ARRAY_RTV& a, const D3D12_TEX1D_ARRAY_RTV& b);
constexpr bool operator==(const D3D12_TEX2D_RTV& a, const D3D12_TEX2D_RTV& b);
constexpr bool operator==(const D3D12_TEX2D_ARRAY_RTV& a, const D3D12_TEX2D_ARRAY_RTV& b);
constexpr bool operator==(const D3D12_TEX2DMS_RTV& a, const D3D12_TEX2DMS_RTV& b);
constexpr bool operator==(const D3D12_TEX2DMS_ARRAY_RTV& a, const D3D12_TEX2DMS_ARRAY_RTV& b);
constexpr bool operator==(const D3D12_TEX3D_RTV& a, const D3D12_TEX3D_RTV& b);

constexpr bool operator==(const D3D12_BUFFER_SRV& a, const D3D12_BUFFER_SRV& b);
constexpr bool operator==(const D3D12_TEX1D_SRV& a, const D3D12_TEX1D_SRV& b);
constexpr bool operator==(const D3D12_TEX1D_ARRAY_SRV& a, const D3D12_TEX1D_ARRAY_SRV& b);
constexpr bool operator==(const D3D12_TEX2D_SRV& a, const D3D12_TEX2D_SRV& b);
constexpr bool operator==(const D3D12_TEX2D_ARRAY_SRV& a, const D3D12_TEX2D_ARRAY_SRV& b);
constexpr bool operator==(const D3D12_TEX2DMS_SRV& a, const D3D12_TEX2DMS_SRV& b);
constexpr bool operator==(const D3D12_TEX2DMS_ARRAY_SRV& a, const D3D12_TEX2DMS_ARRAY_SRV& b);
constexpr bool operator==(const D3D12_TEX3D_SRV& a, const D3D12_TEX3D_SRV& b);
constexpr bool operator==(const D3D12_TEXCUBE_SRV& a, const D3D12_TEXCUBE_SRV& b);
constexpr bool operator==(const D3D12_TEXCUBE_ARRAY_SRV& a, const D3D12_TEXCUBE_ARRAY_SRV& b);
constexpr bool operator==(const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV& a, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV& b);

constexpr bool operator==(const D3D12_BUFFER_UAV& a, const D3D12_BUFFER_UAV& b);
constexpr bool operator==(const D3D12_TEX1D_UAV& a, const D3D12_TEX1D_UAV& b);
constexpr bool operator==(const D3D12_TEX1D_ARRAY_UAV& a, const D3D12_TEX1D_ARRAY_UAV& b);
constexpr bool operator==(const D3D12_TEX2D_UAV& a, const D3D12_TEX2D_UAV& b);
constexpr bool operator==(const D3D12_TEX2D_ARRAY_UAV& a, const D3D12_TEX2D_ARRAY_UAV& b);
constexpr bool operator==(const D3D12_TEX2DMS_UAV& a, const D3D12_TEX2DMS_UAV& b);
constexpr bool operator==(const D3D12_TEX2DMS_ARRAY_UAV& a, const D3D12_TEX2DMS_ARRAY_UAV& b);
constexpr bool operator==(const D3D12_TEX3D_UAV& a, const D3D12_TEX3D_UAV& b);

constexpr bool operator==(const D3D12_TEX1D_DSV& a, const D3D12_TEX1D_DSV& b);
constexpr bool operator==(const D3D12_TEX1D_ARRAY_DSV& a, const D3D12_TEX1D_ARRAY_DSV& b);
constexpr bool operator==(const D3D12_TEX2D_DSV& a, const D3D12_TEX2D_DSV& b);
constexpr bool operator==(const D3D12_TEX2D_ARRAY_DSV& a, const D3D12_TEX2D_ARRAY_DSV& b);
constexpr bool operator==(const D3D12_TEX2DMS_DSV& a, const D3D12_TEX2DMS_DSV& b);
constexpr bool operator==(const D3D12_TEX2DMS_ARRAY_DSV& a, const D3D12_TEX2DMS_ARRAY_DSV& b);

bool operator==(const D3D12_RENDER_TARGET_VIEW_DESC& a, const D3D12_RENDER_TARGET_VIEW_DESC& b);
bool operator==(const D3D12_SHADER_RESOURCE_VIEW_DESC& a, const D3D12_SHADER_RESOURCE_VIEW_DESC& b);
bool operator==(const D3D12_UNORDERED_ACCESS_VIEW_DESC& a, const D3D12_UNORDERED_ACCESS_VIEW_DESC& b);
bool operator==(const D3D12_DEPTH_STENCIL_VIEW_DESC& a, const D3D12_DEPTH_STENCIL_VIEW_DESC& b);

class D3dShaderResourceViewCache final
{
public:
    void init(D3dDevice* device);

    struct SrvDescHash
    {
        size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& v) const;
    };

    struct SrvDescCompare
    {
        bool operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& lhs, const D3D12_SHADER_RESOURCE_VIEW_DESC& rhs) const { return lhs == rhs; };
    };

    struct Entry
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc;
        D3dUniqueDescriptorHandle descriptorHandle;
        D3dUniqueDescriptorHandle shaderVisibleDescriptorHandle;
    };

    const Entry& get(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc);

    void onDestroyResource(ID3D12Resource* resource) { m_cache.erase(resource); }

private:
    std::unordered_map<ID3D12Resource*, std::unordered_map<D3D12_SHADER_RESOURCE_VIEW_DESC, Entry, SrvDescHash, SrvDescCompare>> m_cache;
    D3dDevice* m_device = nullptr;
};

class D3dRenderTargetViewCache final
{
public:
    void init(D3dDevice* device);

    struct RtvDescHash
    {
        size_t operator()(const D3D12_RENDER_TARGET_VIEW_DESC& v) const;
    };

    struct RtvDescCompare
    {
        bool operator()(const D3D12_RENDER_TARGET_VIEW_DESC& lhs, const D3D12_RENDER_TARGET_VIEW_DESC& rhs) const { return lhs == rhs; };
    };

    struct Entry
    {
        D3D12_RENDER_TARGET_VIEW_DESC desc;
        D3dUniqueDescriptorHandle descriptorHandle;
    };

    const Entry& get(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc);

    void onDestroyResource(ID3D12Resource* resource) { m_cache.erase(resource); }

private:
    std::unordered_map<ID3D12Resource*, std::unordered_map<D3D12_RENDER_TARGET_VIEW_DESC, Entry, RtvDescHash, RtvDescCompare>> m_cache;
    D3dDevice* m_device = nullptr;
};

class D3dDepthStencilViewCache final
{
public:
    void init(D3dDevice* device);

    struct DsvDescHash
    {
        size_t operator()(const D3D12_DEPTH_STENCIL_VIEW_DESC& v) const;
    };

    struct DsvDescCompare
    {
        bool operator()(const D3D12_DEPTH_STENCIL_VIEW_DESC& lhs, const D3D12_DEPTH_STENCIL_VIEW_DESC& rhs) const { return lhs == rhs; };
    };

    struct Entry
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC desc;
        D3dUniqueDescriptorHandle descriptorHandle;
    };

    const Entry& get(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc);

    void onDestroyResource(ID3D12Resource* resource) { m_cache.erase(resource); }

private:
    std::unordered_map<ID3D12Resource*, std::unordered_map<D3D12_DEPTH_STENCIL_VIEW_DESC, Entry, DsvDescHash, DsvDescCompare>> m_cache;
    D3dDevice* m_device = nullptr;
};

class D3dUnorderedAccessViewCache final
{
public:
    void init(D3dDevice* device);

    struct UavDescHash
    {
        size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& v) const;
    };

    struct UavDescCompare
    {
        bool operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& lhs, const D3D12_UNORDERED_ACCESS_VIEW_DESC& rhs) const { return lhs == rhs; };
    };

    struct Entry
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
        D3dUniqueDescriptorHandle descriptorHandle;
        D3dUniqueDescriptorHandle shaderVisibleDescriptorHandle;
    };

    const Entry& get(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc);

    void onDestroyResource(ID3D12Resource* resource) { m_cache.erase(resource); }

private:
    std::unordered_map<ID3D12Resource*, std::unordered_map<D3D12_UNORDERED_ACCESS_VIEW_DESC, Entry, UavDescHash, UavDescCompare>> m_cache;
    D3dDevice* m_device = nullptr;
};

class D3dTexture final : public Texture
{
public:
    struct Description
    {
        Description(const Texture::Description& desc);
        Texture::Description base;

        struct
        {
            D3D12_HEAP_FLAGS heapFlags;
        } d3d;
    };

    D3dTexture(D3dDevice* device, const Description& desc, ID3D12Resource* resource = nullptr);
    ~D3dTexture();
    void updateFromHandle(Handle handle) override;

    ID3D12Resource* get() { return m_resource.Get(); }

    const D3D12_RENDER_TARGET_VIEW_DESC& defaultRtvDesc() const { return m_defaultRtvDesc; }

    const D3D12_SHADER_RESOURCE_VIEW_DESC& defaultSrvDesc() const { return m_defaultSrvDesc; }

    const D3D12_UNORDERED_ACCESS_VIEW_DESC& defaultUavDesc() const { return m_defaultUavDesc; }

    const D3D12_DEPTH_STENCIL_VIEW_DESC& defaultDsvDesc() const { return m_defaultDsvDesc; }

    const D3D12_RESOURCE_DESC& resourceDesc() const { return m_resourceDesc; }

    uint32_t pitch(uint16_t mipLevel) const override;

    HANDLE getSharedHandle() const { return m_sharedHandle; }

private:
    void init(const Description& desc);
    uint32_t calculatePitch(Format format, uint32_t w) const;
    D3dDevice* m_device = nullptr;
    ComPtr<ID3D12Resource> m_resource;
    D3D12_RENDER_TARGET_VIEW_DESC m_defaultRtvDesc{};
    D3D12_SHADER_RESOURCE_VIEW_DESC m_defaultSrvDesc{};
    D3D12_UNORDERED_ACCESS_VIEW_DESC m_defaultUavDesc{};
    D3D12_DEPTH_STENCIL_VIEW_DESC m_defaultDsvDesc{};
    D3D12_RESOURCE_DESC m_resourceDesc{};
    HANDLE m_sharedHandle = NULL;

    D3D12MA::Allocation* m_allocation = nullptr;
};

using D3dTextureDesc = D3dTexture::Description;

class D3dSampler final : public Sampler
{
public:
    D3dSampler(D3dDevice* device, const Description& desc);

    D3dDescriptorHandle getShaderVisibleDescriptor() const { return m_shaderVisibleDescriptorHandle.get(); }

    D3dDescriptorHandle getDescriptor() const { return m_descriptorHandle.get(); }

private:
    void init(const Sampler::Description& desc, D3D12_SAMPLER_DESC& d3dDesc);
    D3dDevice* m_device = nullptr;
    D3dUniqueDescriptorHandle m_descriptorHandle;
    D3dUniqueDescriptorHandle m_shaderVisibleDescriptorHandle;
};

class D3dShader final : public Shader
{
public:
    D3dShader(D3dDevice* device, const Description& desc, const void* byteCode, size_t length);

    ~D3dShader() { delete[] m_shaderByteCodeStorage; }

    constexpr D3D12_SHADER_BYTECODE byteCode() const { return m_shaderByteCode; }

private:
    D3D12_SHADER_BYTECODE m_shaderByteCode;
    uint8_t* m_shaderByteCodeStorage;
};

struct D3dAttachmentData
{
    DXGI_FORMAT format;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
    ID3D12Resource* resource = nullptr;
    UINT subresourceIndex = 0;
};

class D3dRootSignature final
{
public:
    struct Description
    {
        const D3D12_ROOT_PARAMETER1* getRootParam(D3D12_ROOT_PARAMETER_TYPE filterType, uint32_t filterIndex, uint32_t& index) const
        {
            uint32_t i = 0;
            for (uint32_t j = 0; j < numRootParams; j++)
            {
                auto p = &rootParams[j];
                if (p->ParameterType != filterType)
                {
                    continue;
                }
                if (i == filterIndex)
                {
                    index = j;
                    return p;
                }
                i++;
            }
            QHWI_FATAL("root parameter not found for type: %d index: %d", filterType, filterIndex);
            return nullptr;
        }

        static const int MaxRootParameters = 32;
        static const int MaxStaticSamplers = 16;
        static const int MaxDescriptorRanges = 32;
        uint32_t numRootParams = 0;
        D3D12_ROOT_PARAMETER1 rootParams[MaxRootParameters];
        uint32_t numStaticSamplers = 0;
        D3D12_STATIC_SAMPLER_DESC staticSamplers[MaxStaticSamplers];
        D3D12_DESCRIPTOR_RANGE1 descriptorRanges[MaxDescriptorRanges];
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        void* pD3DInternalExportHeader = nullptr;
    };

    void init(D3dDevice* device, const Description& desc);

    ID3D12RootSignature* get() { return m_rootSignature.Get(); }

    static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE initD3D12SerializeVersionedRootSignature(HMODULE hD3d12)
    {
        if (hD3d12)
        {
            D3D12SerializeVersionedRootSignature = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(hD3d12, "D3D12SerializeVersionedRootSignature");
        }

        return D3D12SerializeVersionedRootSignature;
    };

private:
    D3dDevice* m_device = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE D3D12SerializeVersionedRootSignature;
};

struct D3dInputLayoutDesc
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescriptions[MaxVertexShaderInputAttributes];
    D3D12_INPUT_LAYOUT_DESC desc;
};

class D3dPipelineLayout final : public PipelineLayout
{
public:
    struct DescriptorSetLayoutInfoEntry
    {
        const DescriptorSetLayout* p = nullptr;
        uint32_t offset = 0;
    };

    struct PushConstantInfoEntry
    {
        uint32_t offset = 0;
        uint32_t num32BitValues = 0;
    };

    D3dPipelineLayout(D3dDevice* device, const Description& desc);

    constexpr D3dRootSignature* rootSignature() { return &m_rootSignature; }

    uint32_t descriptorSetLayoutRootParamOffset(const DescriptorSetLayout* layout) const
    {
        for (const auto& e : m_descriptorSetLayoutInfos)
        {
            if (e.p == layout)
            {
                return e.offset;
            }
        }
        QHWI_FATAL("layout not found");
    }

    const PushConstantInfoEntry& pushConstantInfo(uint32_t index) const { return m_pushConstantInfos[index]; }

private:
    void init(const Description& desc);
    D3dDevice* m_device = nullptr;
    D3dRootSignature m_rootSignature;
    qre::StaticVector<DescriptorSetLayoutInfoEntry, MaxDescriptorSetLayoutsPerPipeline> m_descriptorSetLayoutInfos;
    qre::StaticVector<PushConstantInfoEntry, MaxPushConstantRanges> m_pushConstantInfos;
};

class D3dGraphicsPipeline final : public GraphicsPipeline
{
public:
    struct Description
    {
        Description(const GraphicsPipeline::Description& desc);
        GraphicsPipeline::Description base;

        struct
        {
            D3dInputLayoutDesc inputLayoutDesc;
            CD3DX12_PIPELINE_STATE_STREAM2 pipelineStateStream;
            D3D12_VIEW_INSTANCE_LOCATION viewInstanceLocations[MaxViews];
        } d3d;
    };

    D3dGraphicsPipeline(D3dDevice* device, const Description& desc);

    ID3D12PipelineState* get() { return m_pipelineState.Get(); }

    D3dRootSignature* getRootSignature() const
    {
        auto d3dLayout = static_cast<D3dPipelineLayout*>(m_layout.Get());
        return d3dLayout->rootSignature();
    }

    D3D12_PRIMITIVE_TOPOLOGY getPrimitiveTopology() const { return m_primitiveTopology; }

    bool isStencilTestEnabled() const { return m_enableStencilTest; }

    UINT getStencilRef() const { return m_stencilRef; }

private:
    D3dDevice* m_device = nullptr;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    D3D12_PRIMITIVE_TOPOLOGY m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    bool m_enableStencilTest = false;
    UINT m_stencilRef = 0;
};

using D3dGraphicsPipelineDesc = D3dGraphicsPipeline::Description;

class D3dComputePipeline final : public ComputePipeline
{
public:
    struct Description
    {
        Description(const ComputePipeline::Description& desc);
        ComputePipeline::Description base;

        struct
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC stateDesc = {};
        } d3d;
    };

    D3dComputePipeline(D3dDevice* device, const Description& desc);

    ID3D12PipelineState* get() { return m_pipelineState.Get(); }

    D3dRootSignature* getRootSignature()
    {
        auto d3dLayout = static_cast<D3dPipelineLayout*>(m_layout.Get());
        return d3dLayout->rootSignature();
    }

private:
    D3dDevice* m_device = nullptr;
    ComPtr<ID3D12PipelineState> m_pipelineState;
};

using D3dComputePipelineDesc = D3dComputePipeline::Description;

class D3dRayTracingPipeline final : public RayTracingPipeline
{
public:
    struct Description
    {
        Description(const RayTracingPipelineDesc& desc)
            : base(desc){};

        RayTracingPipelineDesc base;
    };

    D3dRayTracingPipeline(D3dDevice* device, const Description& desc);

    ResultCode getRayTracingShaderGroupHandles(uint32_t startGroup, uint32_t groupCount, size_t dstSize, void* dst) const;

    ID3D12StateObject* get() { return m_rayTracingPipelineStateObject.Get(); };

private:
    D3dDevice* m_device = nullptr;
    ComPtr<ID3D12StateObject> m_rayTracingPipelineStateObject;
    qre::StaticVector<void*, MaxRayPipelineShaderGroups> m_shaderIdentifiers;
};

using D3dRayTracingPipelineDesc = D3dRayTracingPipeline::Description;

class D3dBuffer final : public Buffer
{
public:
    struct Description
    {
        Description(const Buffer::Description& desc);
        Buffer::Description base;

        struct
        {
            D3D12_HEAP_PROPERTIES heapProperties;
            D3D12_HEAP_FLAGS heapFlags;
            D3D12_RESOURCE_DESC resourceDesc;
            D3D12_RESOURCE_STATES initialResourceState;
        } d3d;
    };

    D3dBuffer(D3dDevice* device, const Description& desc);
    ~D3dBuffer();

    void* map(uint64_t offset = 0, uint64_t size = 0) override;
    void unmap() override;

    ID3D12Resource* get() { return m_resource.Get(); }

    HANDLE getSharedHandle() const { return m_sharedHandle; }

    uint64_t getDeviceAddress() const override { return m_deviceAddress; }

private:
    D3dDevice* m_device = nullptr;
    ComPtr<ID3D12Resource> m_resource;
    D3D12_GPU_VIRTUAL_ADDRESS m_deviceAddress = NULL;
    HANDLE m_sharedHandle = NULL;

    D3D12MA::Allocation* m_allocation = nullptr;
};

using D3dBufferDesc = D3dBuffer::Description;

class D3dAccelerationStructure final : public AccelerationStructure
{
public:
    struct Description
    {
        Description(const AccelerationStructure::Description& desc);
        AccelerationStructure::Description base;
    };

    D3dAccelerationStructure(D3dDevice* device, const Description& desc);

    ~D3dAccelerationStructure() override { delete m_buffer; }

    AccelerationStructure::Description* desc() override { return &m_desc.base; }

    const AccelerationStructure::Description* desc() const override { return &m_desc.base; }

    qhwi::Buffer* buffer() const override { return m_buffer; }

    D3D12_GPU_VIRTUAL_ADDRESS getDeviceAddress() const { return m_buffer->getDeviceAddress(); }

    D3dBuffer* getBuffer() { return m_buffer; }

private:
    Description m_desc;
    D3dDevice* m_device;
    D3dBuffer* m_buffer;
};

class D3dDescriptorSet final : public DescriptorSet
{
public:
    struct Description
    {
        Description(const DescriptorSet::Description& desc);
        DescriptorSet::Description base;
    };

    D3dDescriptorSet(D3dDevice* device, const DescriptorSet::Description& desc);

    DescriptorSet::Description* desc() override { return &m_desc.base; }

    const DescriptorSet::Description* desc() const override { return &m_desc.base; }

    const D3dDescriptorHandle& getBindingBaseDescriptorHandle(uint32_t index);
    void resetShaderResources();

private:
    void createDescriptorTables();

    D3dDevice* m_device;
    Description m_desc;
    qre::StaticVector<D3dDescriptorHandle, MaxDescriptorSetLayoutBindings> m_baseDescriptorHandles;
    uint32_t m_numSrvUav = 0;
    uint32_t m_numSampler = 0;
    bool m_isStaged = false;
    D3dUniqueDescriptorTable m_srvUavDescriptorTable;
    D3dUniqueDescriptorTable m_samplerDescriptorTable;
};

using D3dDescriptorSetDesc = D3dDescriptorSet::Description;

class D3dDescriptorSetLayout final : public DescriptorSetLayout
{
public:
    struct Description
    {
        Description(const DescriptorSetLayout::Description& desc);
        DescriptorSetLayout::Description base;
    };

    D3dDescriptorSetLayout(const DescriptorSetLayoutDesc& desc);

    constexpr DescriptorSetLayout::Description* desc() override { return &m_desc.base; }

    constexpr const DescriptorSetLayout::Description* desc() const override { return &m_desc.base; }

private:
    Description m_desc;
};

using D3dDescriptorSetLayoutDesc = D3dDescriptorSetLayout::Description;

class D3dCommandList final : public CommandList
{
public:
    friend class D3dRenderPassEncoder;
    friend struct D3dBeginRenderPassOp;
    friend struct D3dEndRenderPassOp;
    friend struct D3dBindGraphicsPipelineOp;
    friend struct D3dBindDescriptorSetOp;

    struct Description
    {
        Description(const CommandList::Description& desc);
        CommandList::Description base;

        struct
        {
            D3D12_COMMAND_LIST_TYPE type;
        } d3d;
    };

    D3dCommandList(D3dDevice* device, const Description& desc);
    ~D3dCommandList();

    CommandList::Description* desc() override { return &m_desc.base; }

    const CommandList::Description* desc() const override { return &m_desc.base; }

    void beginMarker(const char* name) override;
    void endMarker() override;

    // Begin recording
    void begin() override;
    // End recording
    void end() override;
    void beginRenderPass(uint32_t numColorAttachments,
                         const ColorAttachmentDescription* colorAttachments,
                         const DepthStencilAttachmentDescription* depthStencilAttachment,
                         const MultiViewState* multiViewState,
                         const Rect2D* multiViewPerViewRenderAreasQCOM = nullptr,
                         const uint32_t* colorAttachmentRemapping = nullptr,
                         const TileShadingInfoQCOM* tileShadingInfoQCOM = nullptr,
                         bool customResolve = false) override;
    void endRenderPass() override;

    void beginD3dRenderPass(UINT numRenderTargets,
                            const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets,
                            const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil = nullptr,
                            D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE);
    void endD3dRenderPass();

    void beginComputePass() override;
    void endComputePass() override;

    void beginTransferPass() override;
    void endTransferPass() override;

    void beginAccelerationStructurePass() override;
    void endAccelerationStructurePass() override;

    void dispatch(uint32_t numThreadGroupsX,
                  uint32_t numThreadGroupsY,
                  uint32_t numThreadGroupsZ,
                  uint32_t numThreadsPerThreadgroupX,
                  uint32_t numThreadsPerThreadgroupY,
                  uint32_t numThreadsPerThreadgroupZ) override;
    void dispatchMesh(const DispatchMeshParams& p) override;
    void draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0) override;
    void drawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0) override;
    void dispatchRays(const StridedDeviceAddressRegion& raygenShaderBindingTable,
                      const StridedDeviceAddressRegion& missShaderBindingTable,
                      const StridedDeviceAddressRegion& hitShaderBindingTable,
                      const StridedDeviceAddressRegion& callableShaderBindingTable,
                      uint32_t width,
                      uint32_t height,
                      uint32_t depth) override;
    void dispatchRaysIndirect(Buffer* buffer) override;

    // Indirect dispatch and draw calls
    void dispatchIndirect(uint32_t numThreadsPerThreadgroupX,
                          uint32_t numThreadsPerThreadgroupY,
                          uint32_t numThreadsPerThreadgroupZ,
                          Buffer* buffer,
                          size_t offset = 0,
                          uint32_t dispatchCount = 1) override;
    void drawIndirect(Buffer* buffer, size_t offset = 0, uint32_t drawCount = 1) override;
    void drawIndexedIndirect(Buffer* buffer, size_t offset = 0, uint32_t drawCount = 1) override;

    void bindVertexBuffer(Buffer* buffer, uint32_t slot, size_t offset = 0, size_t stride = 0) override;
    void bindIndexBuffer(Buffer* buffer, IndexFormat indexFormat = IndexFormat::eUint32, size_t offset = 0) override;
    void pushConstants(PipelineLayout* pipelineLayout, PipelineBindPoint pipelineBindPoint, const void* data, uint32_t index = 0) override;
    void bindDescriptorSet(DescriptorSet* descriptorSet,
                           PipelineBindPoint pipelineBindPoint,
                           PipelineLayout* pipelineLayout,
                           uint32_t numDynamicOffsets = 0,
                           const uint32_t* dynamicOffsets = nullptr) override;

    void clear(Buffer* dst, uint32_t value = 0) override;
    void clear(Texture* dst, Color value = Color()) override;
    void resetState() override;

    void updateBuffer(Buffer* dst, std::span<const UpdateBufferRegion> regions) override;
    void updateTexture(Texture* dst, std::span<const UpdateTextureRegion> regions) override;

    void copyBuffer(Buffer* dst, Buffer* src, std::span<const CopyBufferRegion> regions) override;
    void copyBufferToTexture(Texture* dst, Buffer* src, std::span<const CopyBufferToTextureRegion> regions) override;
    void copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion> regions) override;
    void copyTextureToBuffer(Buffer* dst, Texture* src, std::span<const CopyTextureToBufferRegion> regions) override;

    void bindComputePipeline(ComputePipeline* pipeline) override;
    void bindGraphicsPipeline(GraphicsPipeline* pipeline) override;
    void bindRayTracingPipeline(RayTracingPipeline* pipeline) override;

    void setViewports(uint32_t numViewports, const Viewport* viewports) override;
    void setScissors(uint32_t numScissorRects, const ScissorRect* scissorRects) override;
    void setStencilReference(uint32_t reference) override;

    void buildBottomLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                               Buffer* scratchBuffer,
                                               BottomLevelDescriptor* bottomLevelDescriptors,
                                               size_t descriptorCount,
                                               void* scratchMemory,
                                               AccelerationStructureBuildMode buildMode,
                                               AccelerationStructureBuildFlags flags) override;
    void buildTopLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                            Buffer* instanceBuffer,
                                            Buffer* scratchBuffer,
                                            size_t instanceCount,
                                            AccelerationStructure** blasArray,
                                            size_t blasCount,
                                            AccelerationStructureBuildMode buildMode,
                                            AccelerationStructureBuildFlags flags) override;

    void beginTimerQuery(TimerQuery* timerQuery) override;
    void endTimerQuery(TimerQuery* timerQuery) override;
    void resolveTimerQueryData(TimerQuery* timerQuery) override;
    void resetTimerQueryPool(TimerQuery* timerQuery) override;

    void submitted(CommandQueue* commandQueue, ID3D12Fence* fence, uint64_t lastSubmittedID);

    ID3D12GraphicsCommandList7* get() { return m_commandList.Get(); }

    void addBarriers(uint32_t numBufferBarriers,
                     const BufferBarrier* bufferBarriers,
                     uint32_t numTextureBarriers,
                     const TextureBarrier* textureBarriers,
                     uint32_t numGlobalBarriers = 0,
                     const GlobalBarrier* globalBarriers = nullptr,
                     DependencyFlags flags = 0) override;

    Device* device() override { return (Device*)m_device; }

    void addBarriers(uint32_t numBarrierGroups, const CD3DX12_BARRIER_GROUP* barriers, const D3D12_RESOURCE_DESC** textureDescs);

private:
    void beginProfilingPass();
    void endProfilingPass();
    void beginRenderPass1(uint32_t numColorAttachments,
                          const ColorAttachmentDescription* colorAttachments,
                          const DepthStencilAttachmentDescription* depthStencilAttachment = nullptr);
    void endRenderPass1();
    void bindGraphicsPipeline1(D3dGraphicsPipeline* pipeline);
    void bindDescriptorSet1(D3dDescriptorSet* descriptorSet,
                            PipelineBindPoint pipelineBindPoint,
                            D3dPipelineLayout* pipelineLayout,
                            uint32_t numDynamicOffsets = 0,
                            const uint32_t* dynamicOffsets = nullptr);
    D3dDevice* m_device = nullptr;
    Description m_desc;
    HANDLE m_event;
    D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS m_resolveParameters[MaxColorAttachments + 1] = {};
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList7> m_commandList;

    // Defer render pass commands state
    bool defer() { return m_deferState == DeferState::eRecord; }
    enum class DeferState
    {
        eOff,
        eRecord,
        ePlayback
    };
    DeferState m_deferState = DeferState::eOff;
    D3dRenderPassEncoder m_rp;
};

class D3dCommandQueue final : public CommandQueue
{
public:
    struct Description
    {
        Description(const CommandQueue::Description& desc)
            : base(desc)
        {
            auto& cqDesc = d3d.commandQueueDesc;
            cqDesc.Type = qhwi::d3d(desc.capabilities);
            cqDesc.Priority = 0;
            cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            cqDesc.NodeMask = 0;
        }

        CommandQueue::Description base;

        struct
        {
            D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
        } d3d;
    };

    D3dCommandQueue(D3dDevice* device, const Description& desc, const char* name = "");

    ~D3dCommandQueue();

    // Return submission ID
    uint64_t submit(uint32_t numCommandLists, CommandList** commandLists, Fence* fence = nullptr, ext::Swapchain* swapchain = nullptr) override;
    void waitForIdle() override;

    ID3D12CommandQueue* get() const { return m_commandQueue.Get(); }

    CommandQueue::Description* desc() override { return &m_desc.base; }

    const CommandQueue::Description* desc() const override { return &m_desc.base; }

    void* getNativeQueue() override { return get(); }

    ID3D12Fence* getSubmissionFence() { return m_commandListSubmissionFence.Get(); };

#ifdef QHWI_TRACY_GPU_BACKEND_D3D12
    auto getTracyContext() { return m_tracyCtx; }
#endif

private:
    void updateLastCompletedID() override;

    Description m_desc;
    ComPtr<ID3D12Fence> m_commandListSubmissionFence;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    D3dDevice* m_device = nullptr;

#ifdef QHWI_TRACY_GPU_BACKEND_D3D12
    TracyD3D12Ctx m_tracyCtx = nullptr;
#endif
};

class D3dFence final : public Fence
{
public:
    D3dFence(D3dDevice* device, FenceState initialValue = FenceState_Unsignaled, const char* name = "", D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);
    ~D3dFence();
    Result wait() override;
    FenceState getCurrentState() override;
    void reset() override;

    ID3D12Fence* get() { return m_fence.Get(); }

    HANDLE getSharedHandle() const { return m_sharedHandle; }

private:
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_event = nullptr;
    HANDLE m_sharedHandle = NULL;
};

class D3dHeapAllocator final
{
public:
    void init(uint32_t numDescriptors);
    int alloc(uint32_t descriptorCount = 1);
    bool release(int index, uint32_t descriptorCount = 1);

private:
    uint32_t m_numDescriptors = 0;
    std::bitset<MaxDescriptorHeapDescriptors> m_isAllocated;
    uint32_t m_scanStartIndex = 0;
    uint32_t m_currentAllocations = 0;
};

class D3dDescriptorHeap final
{
public:
    struct Description : D3D12_DESCRIPTOR_HEAP_DESC
    {
        Description() {}

        Description(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE, uint32_t numDescriptors = MaxDescriptorHeapDescriptors)
        {
            Type = type;
            NumDescriptors = numDescriptors;
            Flags = flags;
            NodeMask = 0;
        }
    };

    D3dDescriptorHeap() {}

    void init(D3dDevice* device, const Description& desc, const char* name = "");

    std::optional<D3dDescriptorHandle> getHandle();
    std::optional<D3dDescriptorTable> getDescriptorTable(uint32_t numDescriptors = 1);

    std::optional<D3dDescriptorHandle> stage(D3dDescriptorHandle srcDescriptor);
    std::optional<D3dDescriptorHandle> stage(const D3dDescriptorTable& descriptorTable, const D3dDescriptorHandle& srcDescriptor, uint32_t descriptorIndex) const;

    bool releaseHandle(D3dDescriptorHandle descriptorHandle);
    bool releaseDescriptorTable(const D3dDescriptorTable& descriptorTable);

    constexpr const Description* desc() { return &m_desc; }

    ID3D12DescriptorHeap* get() const { return m_descriptorHeap.Get(); }

private:
    D3dDevice* m_device = nullptr;
    D3dDescriptorHandle head;
    Description m_desc;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    uint32_t m_descriptorHandleIncrementSize = 0;
    D3dHeapAllocator m_heapAllocator;
};

class D3dDescriptorHeapManager final
{
public:
    void init(D3dDevice* device, const D3dDescriptorHeap::Description& desc, const char* name = "");

    std::optional<D3dDescriptorHandle> getHandle();
    std::optional<D3dDescriptorTable> getDescriptorTable(uint32_t numDescriptors = 1);

    std::optional<D3dDescriptorHandle> stage(D3dDescriptorHandle srcDescriptor);
    std::optional<D3dDescriptorHandle> stage(const D3dDescriptorTable& descriptorTable, const D3dDescriptorHandle& srcDescriptor, uint32_t descriptorIndex) const;

    void releaseHandle(D3dDescriptorHandle descriptorHandle);
    void releaseDescriptorTable(const D3dDescriptorTable& descriptorTable);

    constexpr const D3dDescriptorHeap::Description* desc() { return &m_desc; }

private:
    D3dDevice* m_device = nullptr;
    D3dDescriptorHeap::Description m_desc;
    qre::StaticVector<D3dDescriptorHeap, MaxDescriptorHeaps> m_descriptorHeap;
    uint32_t m_descriptorHandleIncrementSize = 0;
    size_t m_descriptorHeapSearchIndex = 0;
};

class D3dTimerQueryHeap final : public QueryHeap
{
public:
    ~D3dTimerQueryHeap(){};

    void destroy() { queryDataResolveBuffer.Get()->Release(); };

    void init(Device* device, const char* name = "") override;

    ID3D12QueryHeap* get() { return queryHeap.Get(); };

    ID3D12Resource* getQueryDataResolveBuffer() { return static_cast<D3dBuffer*>(queryDataResolveBuffer.Get())->get(); };

private:
    ComPtr<ID3D12QueryHeap> queryHeap;
    qre::RefCountPtr<Buffer> queryDataResolveBuffer;
};

class D3dTimerQuery final : public TimerQuery
{
public:
    D3dTimerQuery(D3dTimerQueryHeap* pQueryHeap, uint32_t beginIndex, uint32_t endIndex)
        : TimerQuery(pQueryHeap, beginIndex, endIndex)
    {
    }

    HANDLE cmdListCompletedEvent = NULL;
    UINT64 lastSubmittedID = 0;
};

struct D3dFeaturesInfo
{
    void init(ID3D12Device* device);
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS> options;
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS1> options1;
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS3> options3;
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS4> options4;
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS5> options5;
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS7> options7;
    std::optional<D3D12_FEATURE_DATA_D3D12_OPTIONS12> options12;
    std::optional<D3D12_FEATURE_DATA_ARCHITECTURE> architecture;
    std::optional<D3D12_FEATURE_DATA_SHADER_MODEL> shaderModel; // Shader Execution Reordering

    bool supportsConservativeRasterization() const
    {
        return options.has_value() ? (options.value().ConservativeRasterizationTier != D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED) : false;
    }

    bool supportsShaderFloat16() const { return options4.has_value() ? options4.value().Native16BitShaderOpsSupported : false; }

    bool supportsRayPipelines() const { return options5.has_value() ? (options5.value().RaytracingTier >= D3D12_RAYTRACING_TIER_1_0) : false; }

    bool supportsRayQuery() const { return options5.has_value() ? (options5.value().RaytracingTier >= D3D12_RAYTRACING_TIER_1_1) : false; }

    bool supportsMeshShaders() const { return (options7.has_value()) ? (options7.value().MeshShaderTier >= D3D12_MESH_SHADER_TIER_1) : false; }

    bool supportsEnhancedBarriers() const { return (options12.has_value()) ? (options12.value().EnhancedBarriersSupported) : false; }

    bool supportsMultiView() const { return (options3.has_value()) ? (options3.value().ViewInstancingTier != D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED) : false; }

    bool supportsBarycentrics() const { return (options3.has_value()) ? (options3.value().BarycentricsSupported) : false; }

    bool supportsOpacityMicromaps() const { return options5.has_value() ? (options5.value().RaytracingTier >= D3D12_RAYTRACING_TIER_1_2) : false; }

    bool isUnifiedMemoryArchitecture() const { return (architecture.has_value()) ? architecture.value().UMA : false; }

    uint32_t minWaveSize() const { return (options1.has_value()) ? options1.value().WaveLaneCountMin : 0; }

    uint32_t maxWaveSize() const { return (options1.has_value()) ? options1.value().WaveLaneCountMax : 0; }

    bool supportsDispatchRaysIndirect() const { return options5.has_value() ? (options5.value().RaytracingTier >= D3D12_RAYTRACING_TIER_1_1) : false; }

    bool supportsRayTracingInvocationReorder() const
    {
        if (options5.has_value() && shaderModel.has_value())
        {
            return options5.value().RaytracingTier >= D3D12_RAYTRACING_TIER_1_2 && shaderModel.value().HighestShaderModel >= D3D_SHADER_MODEL_6_9;
        }

        return false;
    }

private:
    template <typename T>
    std::optional<T> checkFeatureSupport(ID3D12Device* device, D3D12_FEATURE feature)
    {
        T value = {};
        if constexpr (std::is_same_v<T, D3D12_FEATURE_DATA_SHADER_MODEL>)
        {
            value.HighestShaderModel = D3D_HIGHEST_SHADER_MODEL;
        }

        if (SUCCEEDED(device->CheckFeatureSupport(feature, &value, sizeof(value))))
        {
            return value;
        }
        return std::nullopt;
    }
};

class D3dDevice final : public Device
{
public:
    struct Description
    {
        Description(ComPtr<IDXGIAdapter1> adapter, const Device::Description& desc)
            : base(desc)
        {
            d3d.adapter = adapter;
            d3d.adapter->GetDesc1(&d3d.adapterDesc);
            d3d.featureLevel = DefaultFeatureLevel;
        }

        Device::Description base;

        struct
        {
            ComPtr<IDXGIAdapter1> adapter;
            DXGI_ADAPTER_DESC1 adapterDesc{};
            D3D_FEATURE_LEVEL featureLevel{};
        } d3d;
    };

    D3dDevice(D3dContext* ctx, ID3D12Device* device, IDXGIAdapter1* adapter, const Description& desc);

    ~D3dDevice()
    {
        // Release staged null descriptors to free heap slots
        if (!!m_nullBufferSrvDescriptor)
        {
            m_cbvSrvUavDescriptorHeapManager.releaseHandle(m_nullBufferSrvDescriptor);
            m_nullBufferSrvDescriptor.clear();
        }
        if (!!m_nullTexture2DSrvDescriptor)
        {
            m_cbvSrvUavDescriptorHeapManager.releaseHandle(m_nullTexture2DSrvDescriptor);
            m_nullTexture2DSrvDescriptor.clear();
        }
        if (!!m_nullBufferUavDescriptor)
        {
            m_cbvSrvUavDescriptorHeapManager.releaseHandle(m_nullBufferUavDescriptor);
            m_nullBufferUavDescriptor.clear();
        }
        if (!!m_nullTexture2DUavDescriptor)
        {
            m_cbvSrvUavDescriptorHeapManager.releaseHandle(m_nullTexture2DUavDescriptor);
            m_nullTexture2DUavDescriptor.clear();
        }

        m_timerQueryHeap.destroy();
        m_uploadPool.destroy();

        m_sharedAllocationsPool->Release();
        m_dmaAllocator->Release();

        if (m_hD3d12)
        {
            FreeLibrary(m_hD3d12);
        }
    }

    void* getNativeDevice() override { return m_device.Get(); };

    qre::RefCountPtr<PipelineLayout> createPipelineLayout(const PipelineLayout::Description& desc) override;
    qre::RefCountPtr<ComputePipeline> createComputePipeline(const ComputePipeline::Description& desc) override;
    qre::RefCountPtr<GraphicsPipeline> createGraphicsPipeline(const GraphicsPipeline::Description& desc) override;
    qre::RefCountPtr<RayTracingPipeline> createRayTracingPipeline(const RayTracingPipeline::Description& desc) override;

    qre::RefCountPtr<Buffer> createBuffer(const Buffer::Description& desc) override;
    qre::RefCountPtr<Texture> createTexture(const Texture::Description& desc) override;

    qre::RefCountPtr<Texture> createTextureFromHandle(const Texture::Description& desc, Texture::Handle handle, Texture::Handle handle2 = nullptr) override
    {
        return createTextureFromD3d12Resource(desc, (ID3D12Resource*)handle);
    }

    qre::RefCountPtr<Texture> createTextureFromD3d12Resource(const Texture::Description& desc, ID3D12Resource* resource);
    qre::RefCountPtr<Sampler> createSampler(const Sampler::Description& desc) override;

    qre::RefCountPtr<Shader> createShader(const Shader::Description& desc, const void* byteCode, size_t length) override;

    qre::RefCountPtr<DescriptorSet> createDescriptorSet(const DescriptorSet::Description& desc) override;
    qre::RefCountPtr<DescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
    void updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSetWrite* writes, uint32_t numWrites = 1) override;

    qre::RefCountPtr<CommandList> createCommandList(CommandQueue* commandQueue, const CommandList::Description& desc = {}) override;

    qre::RefCountPtr<CommandQueue> createCommandQueue(const CommandQueue::Description& desc) override;

    qre::RefCountPtr<directstorage::File> createStorageFile() override;

    ext::WindowSurface* getWindowSurface() { return &m_windowSurface; }

    void setWindowSurface(const ext::WindowSurface& windowSurface) override { m_windowSurface = windowSurface; }

    ext::Swapchain* createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc) override;

    qre::RefCountPtr<Fence> createFence(FenceState initialState = FenceState_Unsignaled, const char* name = "") override;
    qre::RefCountPtr<TimerQuery> createTimerQuery() override;
    qre::RefCountPtr<AccelerationStructure> createAccelerationStructure(const AccelerationStructure::Description& desc) override;
    size_t getBottomLevelScratchSize(size_t descriptorCount,
                                     AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                     AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) override;
    AccelerationStructureBuildInfo getAccelerationStructureBuildInfo(const BottomLevelDescriptor* bottomLevelDescriptors,
                                                                     size_t count,
                                                                     void* scratchMemory,
                                                                     AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                                     AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) override;
    qre::RefCountPtr<Buffer> createTopLevelInstanceBuffer(const TopLevelDescriptor* topLevelDescriptors, AccelerationStructure** blasArray, size_t count) override;
    AccelerationStructureBuildInfo getAccelerationStructureBuildInfo(size_t instanceCount,
                                                                     AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                                     AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) override;
    ResultCode getRayTracingShaderGroupHandles(const RayTracingPipeline* pipeline, uint32_t startGroup, uint32_t groupCount, size_t dstSize, void* dst) const override;
    [[deprecated]] bool isTimerQueryResultReady(TimerQuery* timerQuery) override;
    bool getTimerQueryTimeInSec(TimerQuery* timerQuery, CommandQueue* commandQueue, bool blockUntilReady, float& timerInSec) override;

    Device::Description* desc() override { return &m_desc.base; }

    const Device::Description* desc() const override { return &m_desc.base; }

    const D3dDevice::Description* d3dDesc() const { return &m_desc; }

    GraphicsBackend getGraphicsBackend() const override;

    D3dContext* getD3dContext();

    ID3D12Device5* get() const { return m_device.Get(); }

    constexpr D3dDescriptorHeap* getRtvDescriptorHeap() { return &m_rtvDescriptorHeap; };

    constexpr D3dDescriptorHeapManager* getCbvSrvUavDescriptorHeap() { return &m_cbvSrvUavDescriptorHeapManager; };

    constexpr D3dDescriptorHeap* getSamplerDescriptorHeap() { return &m_samplerDescriptorHeap; };

    constexpr D3dDescriptorHeap* getCbvSrvUavShaderVisibleDescriptorHeap() { return &m_cbvSrvUavShaderVisibleDescriptorHeap; };

    constexpr D3dDescriptorHeap* getSamplerShaderVisibleDescriptorHeap() { return &m_samplerShaderVisibleDescriptorHeap; };

    constexpr D3dDescriptorHeap* getDsvDescriptorHeap() { return &m_dsvDescriptorHeap; };

    D3dDescriptorHandle getSrvDescriptor(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr)
    {
        return m_srvCache.get(resource, desc).descriptorHandle.get();
    }

    D3dDescriptorHandle getSrvShaderVisibleDescriptor(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr)
    {
        return m_srvCache.get(resource, desc).shaderVisibleDescriptorHandle.get();
    }

    D3dDescriptorHandle getRtvDescriptor(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc = nullptr)
    {
        return m_rtvCache.get(resource, desc).descriptorHandle.get();
    }

    D3dDescriptorHandle getDsvDescriptor(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc = nullptr)
    {
        return m_dsvCache.get(resource, desc).descriptorHandle.get();
    }

    D3dDescriptorHandle getUavDescriptor(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr)
    {
        return m_uavCache.get(resource, desc).descriptorHandle.get();
    }

    D3dDescriptorHandle getUavShaderVisibleDescriptor(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr)
    {
        return m_uavCache.get(resource, desc).shaderVisibleDescriptorHandle.get();
    }

    D3dDescriptorHandle getSrvDescriptor(D3dTexture* texture, D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr)
    {
        return getSrvDescriptor(texture->get(), desc ? desc : &texture->defaultSrvDesc());
    }

    D3dDescriptorHandle getSrvShaderVisibleDescriptor(D3dTexture* texture, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr)
    {
        return getSrvShaderVisibleDescriptor(texture->get(), desc ? desc : &texture->defaultSrvDesc());
    }

    D3dDescriptorHandle getRtvDescriptor(D3dTexture* texture, const D3D12_RENDER_TARGET_VIEW_DESC* desc = nullptr)
    {
        return getRtvDescriptor(texture->get(), desc ? desc : &texture->defaultRtvDesc());
    }

    D3dDescriptorHandle getDsvDescriptor(D3dTexture* texture, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc = nullptr)
    {
        return getDsvDescriptor(texture->get(), desc ? desc : &texture->defaultDsvDesc());
    }

    D3dDescriptorHandle getUavDescriptor(D3dTexture* texture, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr)
    {
        return getUavDescriptor(texture->get(), desc ? desc : &texture->defaultUavDesc());
    }

    D3dDescriptorHandle getUavShaderVisibleDescriptor(D3dTexture* texture, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr)
    {
        return getUavShaderVisibleDescriptor(texture->get(), desc ? desc : &texture->defaultUavDesc());
    }

    void onDestroyResource(ID3D12Resource* resource)
    {
        m_srvCache.onDestroyResource(resource);
        m_rtvCache.onDestroyResource(resource);
        m_dsvCache.onDestroyResource(resource);
        m_uavCache.onDestroyResource(resource);
    }

    D3dDescriptorHandle getNullBufferSrvDescriptor() const { return m_nullBufferSrvDescriptor; }

    D3dDescriptorHandle getNullTexture2DSrvDescriptor() const { return m_nullTexture2DSrvDescriptor; }

    D3dDescriptorHandle getNullBufferUavDescriptor() const { return m_nullBufferUavDescriptor; }

    D3dDescriptorHandle getNullTexture2DUavDescriptor() const { return m_nullTexture2DUavDescriptor; }

    ID3D12CommandSignature* getDispatchCommandSignature() { return m_commandSignatureDispatch.Get(); };

    ID3D12CommandSignature* getDrawCommandSignature() { return m_commandSignatureDraw.Get(); };

    ID3D12CommandSignature* getDrawIndexedCommandSignature() { return m_commandSignatureDrawIndexed.Get(); };

    ID3D12CommandSignature* getDispatchRaysCommandSignature() { return m_commandSignatureDispatchRays.Get(); };

    constexpr D3dTimerQueryHeap* getTimerQueryHeap() { return &m_timerQueryHeap; };

    constexpr bool isUnifiedMemoryArchitecture() override
    {
        if (m_desc.base.emulateUma)
        {
            return true;
        }
        return m_d3dFeaturesInfo.isUnifiedMemoryArchitecture();
    }

    FormatSupportMask queryFormatSupport(Format format) const override;

    size_t getConstantBufferOffsetAlignment() const override;

    size_t getMinStorageBufferOffsetAlignment() const override
    {
        // No required minimum offset for Structured buffers
        return 1;
    }

    // Returns true if the device supports enhanced barriers AND if they are enabled
    constexpr bool supportsEnhancedBarriers()
    {
        if (!m_desc.base.enableD3dEnhancedBarriers)
        {
            return false;
        }
        return m_supportsEnhancedBarriers;
    }

    bool supportsImageProcessingQCOM() const override { return false; }

    bool supportsMultiView() const override { return m_d3dFeaturesInfo.supportsMultiView(); }

    bool setStablePowerState(bool enable) const { return SUCCEEDED(m_device->SetStablePowerState(enable)); }

    D3D12MA::Allocator* getDmaAllocator() { return m_dmaAllocator; }

    D3D12MA::Allocator* m_dmaAllocator = nullptr;
    D3D12MA::Pool* m_sharedAllocationsPool = nullptr;

private:
    void createDescriptorHeaps();
    void createResourceViewCaches();
    void createCommandSignatures();
    void initNullDescriptors();
    ComPtr<ID3D12Device5> m_device;
    ComPtr<ID3D12CommandSignature> m_commandSignatureDraw;
    ComPtr<ID3D12CommandSignature> m_commandSignatureDrawIndexed;
    ComPtr<ID3D12CommandSignature> m_commandSignatureDispatch;
    ComPtr<ID3D12CommandSignature> m_commandSignatureDispatchRays;
    Description m_desc;
    D3dDescriptorHeap m_rtvDescriptorHeap;
    D3dDescriptorHeapManager m_cbvSrvUavDescriptorHeapManager;
    D3dDescriptorHeap m_cbvSrvUavShaderVisibleDescriptorHeap;
    D3dDescriptorHeap m_samplerDescriptorHeap;
    D3dDescriptorHeap m_samplerShaderVisibleDescriptorHeap;
    D3dDescriptorHeap m_dsvDescriptorHeap;
    D3dShaderResourceViewCache m_srvCache;
    D3dRenderTargetViewCache m_rtvCache;
    D3dDepthStencilViewCache m_dsvCache;
    D3dUnorderedAccessViewCache m_uavCache;

    D3dFeaturesInfo m_d3dFeaturesInfo;

    ext::WindowSurface m_windowSurface;
    D3dTimerQueryHeap m_timerQueryHeap;
    bool m_supportsEnhancedBarriers = false;
    HMODULE m_hD3d12;

    // Null descriptors for optional shader resources
    D3dDescriptorHandle m_nullBufferSrvDescriptor;
    D3dDescriptorHandle m_nullTexture2DSrvDescriptor;
    D3dDescriptorHandle m_nullBufferUavDescriptor;
    D3dDescriptorHandle m_nullTexture2DUavDescriptor;
};

using D3dDeviceDesc = D3dDevice::Description;

class D3dContext final : public Context
{
public:
    struct Description
    {
        Description(const Context::Description& desc)
            : base(desc)
        {
        }

        Context::Description base;
    };

    D3dContext(const Description& desc);
    ~D3dContext();

    struct EnumerateDeviceParams
    {
        DXGI_GPU_PREFERENCE gpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
        D3D_FEATURE_LEVEL featureLevel = DefaultFeatureLevel;
    };

    struct EnumerateDeviceResult
    {
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc;
    };

    struct EnumerateDeviceResults
    {
        int numResults = 0;
        EnumerateDeviceResult results[MaxDevices];
    };

    ResultCode init() override;
    void enumerateDevices(const EnumerateDeviceParams& params, EnumerateDeviceResults& results);
    qre::RefCountPtr<Device> createDevice(const DeviceDesc& desc) override;
    qre::RefCountPtr<Device> createDevice(IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL featureLevel, const DeviceDesc& deviceDesc);
    void reportLiveObjects();

    IDXGIFactory6* getFactory() { return m_dxgiFactory.Get(); }

    void setFactory(IDXGIFactory6* factory) { m_dxgiFactory = factory; }

    Context::Description* desc() override { return &m_desc.base; }

    const Context::Description* desc() const override { return &m_desc.base; }

private:
    typedef HRESULT(WINAPI* PFN_CREATE_DXGI_FACTORY2)(UINT Flags, REFIID riid, _COM_Outptr_ void** ppFactory);
    typedef HRESULT(WINAPI* PFN_GET_DEBUG_INTERFACE1)(UINT Flags, REFIID riid, _COM_Outptr_ void** pDebug);

    Description m_desc;
    ComPtr<IDXGIFactory6> m_dxgiFactory;
    ComPtr<ID3D12Debug6> m_d3d12Debug;

    HMODULE m_hD3d12 = 0;
    HMODULE m_hDxgi = 0;
    PFN_D3D12_CREATE_DEVICE D3D12CreateDevice = 0;
    PFN_D3D12_GET_DEBUG_INTERFACE D3D12GetDebugInterface = 0;
    PFN_CREATE_DXGI_FACTORY2 CreateDXGIFactory2 = 0;
    PFN_GET_DEBUG_INTERFACE1 GetDebugInterface1 = 0;
#ifdef QHWI_USE_AGILITY_SDK_717_PREVIEW
    PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES D3D12EnableExperimentalFeatures = 0;
#endif //QHWI_USE_AGILITY_SDK_717_PREVIEW
};

D3D12_SHADER_BYTECODE d3d(Shader* shader);
D3D12_BLEND_DESC d3d(const BlendState& blendState, const MultisampleState& multisampleState);
D3D12_RENDER_TARGET_BLEND_DESC d3d(const ColorAttachmentBlendState& desc);
D3D12_DEPTH_STENCIL_DESC d3d(const DepthStencilState& depthStencilState);
D3D12_DEPTH_STENCILOP_DESC d3d(const StencilOpState& stencilOpState);
D3D12_RASTERIZER_DESC d3d(const RasterizerState& rasterizerState, const MultisampleState& multisampleState);
D3D12_INDEX_BUFFER_STRIP_CUT_VALUE d3d(PrimitiveRestartValue v);

D3D12_RENDER_TARGET_VIEW_DESC d3dRenderTargetViewDesc(Texture* texture, const SubresourceRange& subresourceRange);
D3D12_SHADER_RESOURCE_VIEW_DESC d3dShaderResourceViewDesc(Texture* texture, const SubresourceRange& subresourceRange);
D3D12_SHADER_RESOURCE_VIEW_DESC d3dShaderResourceViewDesc(Buffer* buffer, uint64_t offset, uint64_t range, uint64_t stride, DescriptorType descriptorType);
D3D12_DEPTH_STENCIL_VIEW_DESC d3dDepthStencilViewDesc(Texture* texture, const SubresourceRange& sRange);
D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUnorderedAccessViewDesc(Texture* texture, const SubresourceRange& s);
D3D12_UNORDERED_ACCESS_VIEW_DESC d3dUnorderedAccessViewDesc(Buffer* buffer, uint64_t offset, uint64_t range, uint64_t stride, DescriptorType descriptorType);

CD3DX12_TEXTURE_BARRIER d3dBarrier(const ColorAttachmentDescription& desc, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
CD3DX12_TEXTURE_BARRIER d3dBarrier(const DepthStencilAttachmentDescription& desc);

void d3d(const VertexInputState& vertexInputState, D3dInputLayoutDesc& d3dInputLayoutDesc);
uint32_t d3d(Texture* texture, const Subresource* s);

constexpr D3D12_VIEWPORT d3d(const Viewport& v)
{
    return { v.x, v.y, v.w, v.h, v.zNear, v.zFar };
}

constexpr D3D12_RECT d3d(const ScissorRect& r)
{
    return { LONG(r.x), LONG(r.y), LONG(r.x + r.w), LONG(r.y + r.h) };
}

namespace hash_util
{
uint64_t Fnv1a64Continue(uint64_t& hash, const D3D12_UNORDERED_ACCESS_VIEW_DESC& v);
uint64_t Fnv1a64Continue(uint64_t& hash, const D3D12_RENDER_TARGET_VIEW_DESC& v);
uint64_t Fnv1a64Continue(uint64_t& hash, const D3D12_SHADER_RESOURCE_VIEW_DESC& v);
uint64_t Fnv1a64Continue(uint64_t& hash, const D3D12_DEPTH_STENCIL_VIEW_DESC& v);
}; // namespace hash_util

namespace directstorage
{
class D3dFile final : public qhwi::directstorage::File
{
public:
    ~D3dFile() { m_file->Close(); }

    void close() override { m_file->Close(); }

    void getFileInformation(FileInfo& fileInfo) const override;

    IDStorageFile* get() const { return m_file.Get(); }

    IDStorageFile** getAddressOf() { return m_file.GetAddressOf(); }

private:
    ComPtr<IDStorageFile> m_file;
};

class D3dStorageQueue final : public qhwi::directstorage::StorageQueue
{
public:
    IDStorageQueue1** getAddressOf() { return m_storageQueue.GetAddressOf(); };

    void enqueueRequest(const Request&) override;
    void enqueueStatus(StorageStatusArray* array, uint32_t index = 0) override;
    void enqueueSignal(Fence* fence, uint64_t value) override;
    void submit() override;

private:
    ComPtr<IDStorageQueue1> m_storageQueue;
};

class D3dStorageStatusArray final : public StorageStatusArray
{
public:
    IDStorageStatusArray* get() { return m_storageStatusArray.Get(); };

    IDStorageStatusArray** getAddressOf() { return m_storageStatusArray.GetAddressOf(); };

    ResultCode getResult(uint32_t index = 0) const override;

    bool isComplete(uint32_t index = 0) const override { return m_storageStatusArray->IsComplete(index); }

private:
    ComPtr<IDStorageStatusArray> m_storageStatusArray;
};

class D3dFactory final : public Factory
{
public:
    D3dFactory() {}

    ~D3dFactory() {}

    ResultCode init() override;
    ResultCode openFile(const char* fileToLoad, File* file) const override;
    qre::RefCountPtr<StorageQueue> createQueue(const StorageQueueDesc& desc) const override;
    qre::RefCountPtr<StorageStatusArray> createStatusArray(uint32_t capacity, const char* name) const override;

private:
    ComPtr<IDStorageFactory> m_factory = NULL;
};
} // namespace directstorage

} // namespace qhwi

#endif
