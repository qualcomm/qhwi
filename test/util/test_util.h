//
//
//  test_util.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <qhwi/qhwi.h>
#include <qhwi/qhwi_util.h>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <qre/text/textutil.h>

#ifndef QHWI_DATA_DIR
#    define QHWI_DATA_DIR ""
#endif

namespace test_util
{
template <typename T>
inline bool contains(const std::vector<T>& v, const T& val)
{
    return (std::find(v.begin(), v.end(), val) != v.end());
}

struct int2
{
    int32_t x, y;
};

struct int3
{
    int32_t x, y, z;
};

struct int4
{
    int32_t x, y, z, w;
};

struct uint2
{
    uint32_t x, y;
};

struct uint3
{
    uint32_t x, y, z;
};

struct uint4
{
    uint32_t x, y, z, w;
};

struct half2
{
    uint16_t x, y;
};

struct half3
{
    uint16_t x, y, z;
};

struct half4
{
    uint16_t x, y, z, w;
};

struct float2
{
    float x, y;
};

struct float3
{
    float x, y, z;
};

struct float4
{
    float x, y, z, w;
};

struct float4x4
{
    float4x4() {}

    float4x4(float c) { v[0] = v[5] = v[10] = v[15] = c; }

    float4x4(const std::array<float, 16>& s)
    {
        for (int j = 0; j < 16; j++)
        {
            v[j] = s[j];
        }
    }

    float v[16] = { 0.0f };
};

static const char* ShaderExtensions[] = { "", "dxil", "spv", "metallib" };

class Shader1
{
public:
    void init(qhwi::Device* device, std::string file, std::string entryPointName, qhwi::ShaderType shaderType = qhwi::ShaderType::eUnknown);
    qre::RefCountPtr<qhwi::Shader> shader;
    qhwi::FileData byteCode;
    std::string entryPointName;
};

inline void initShader(qhwi::Device* device,
                       std::string name,
                       std::string entryPointName,
                       qhwi::ShaderType type,
                       Shader1& m,
                       std::string suffix = "",
                       std::string dataDir = QHWI_DATA_DIR)
{
    using str = std::string;
    qhwi::GraphicsBackend graphicsBackend = device->getGraphicsBackend();
    str shaderExtension = ShaderExtensions[int(graphicsBackend)];
    str path = str("shaders/") + name + "_" + suffix + entryPointName + str(".") + shaderExtension;
#ifndef QHWI_PLATFORM_ANDROID
    path = dataDir + str("/") + path;
#endif
    m.init(device, path, entryPointName, type);
}

inline void initVertexShader(qhwi::Device* device, std::string name, Shader1& m, std::string entryPointName = "VSMain", std::string suffix = "")
{
    initShader(device, name, entryPointName, qhwi::ShaderType::eVertex, m, suffix);
}

inline void initPixelShader(qhwi::Device* device, std::string name, Shader1& m, std::string entryPointName = "PSMain", std::string suffix = "")
{
    initShader(device, name, entryPointName, qhwi::ShaderType::ePixel, m, suffix);
}

inline void initVertexAndPixelShader(qhwi::Device* device,
                                     std::string name,
                                     Shader1& vs,
                                     Shader1& ps,
                                     std::string vsEntryPointName = "VSMain",
                                     std::string psEntryPointName = "PSMain",
                                     std::string suffix = "")
{
    initShader(device, name, vsEntryPointName, qhwi::ShaderType::eVertex, vs, suffix);
    initShader(device, name, psEntryPointName, qhwi::ShaderType::ePixel, ps, suffix);
}

inline void initComputeShader(qhwi::Device* device, std::string name, Shader1& m, std::string entryPointName = "CSMain", std::string suffix = "")
{
    initShader(device, name, entryPointName, qhwi::ShaderType::eCompute, m, suffix);
}

inline void initMeshShader(qhwi::Device* device, std::string name, Shader1& m, std::string entryPointName = "MSMain", std::string suffix = "")
{
    initShader(device, name, entryPointName, qhwi::ShaderType::eMesh, m, suffix);
}

inline void initAmplificationShader(qhwi::Device* device, std::string name, Shader1& m, std::string entryPointName = "ASMain", std::string suffix = "")
{
    initShader(device, name, entryPointName, qhwi::ShaderType::eAmplification, m, suffix);
}

class Buffer1
{
public:
    qhwi::Buffer* get() { return buffer.Get(); }

    qre::RefCountPtr<qhwi::Buffer> buffer;
    qre::RefCountPtr<qhwi::Buffer> stagingBuffer;
    qre::RefCountPtr<qhwi::Buffer> readbackBuffer;

    void initAsConstantBuffer(qhwi::Device* device, const char* name, void* data = nullptr, uint32_t size = 0)
    {
        qhwi::BufferDesc desc = { .heapType = qhwi::HeapType::eDefault,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsage_ConstantBuffer,
                                  .size = size,
                                  .format = {},
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc, data);
    }

    void initAsVertexBuffer(qhwi::Device* device, const char* name, void* data = nullptr, uint32_t size = 0)
    {
        qhwi::BufferDesc desc = { .heapType = qhwi::HeapType::eDefault,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsage_VertexBuffer,
                                  .size = size,
                                  .format = {},
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc, data);
    }

    void initAsIndexBuffer(qhwi::Device* device, const char* name, void* data = nullptr, uint32_t size = 0)
    {
        qhwi::BufferDesc desc = { .heapType = qhwi::HeapType::eDefault,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsage_IndexBuffer,
                                  .size = size,
                                  .format = {},
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc, data);
    }

    void initAsReadbackBuffer(qhwi::Device* device, const char* name, uint32_t size)
    {
        qhwi::BufferDesc desc = { .heapType = qhwi::HeapType::eDownload,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsageMask(0),
                                  .size = size,
                                  .format = {},
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc);
    }

    void initAsUnorderedAccessBuffer(qhwi::Device* device, const char* name, void* data = nullptr, uint32_t size = 0)
    {
        qhwi::BufferDesc desc = { .heapType = qhwi::HeapType::eDefault,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsage_UnorderedAccess,
                                  .size = size,
                                  .format = {},
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc, data);
    }

    void initAsTypedBuffer(qhwi::Device* device, const char* name, qhwi::Format format, void* data = nullptr, uint32_t size = 0)
    {
        qhwi::BufferDesc desc = { .heapType = qhwi::HeapType::eDefault,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsage_TypedBuffer,
                                  .size = size,
                                  .format = format,
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc, data);
    }

    void initAsIndirectBuffer(qhwi::Device* device, const char* name, uint32_t size = 0)
    {
        qhwi::HeapType heapType = qhwi::HeapType::eDefault;
        qhwi::BufferDesc desc = { .heapType = heapType,
                                  .heapFlags = {},
                                  .usage = qhwi::BufferUsage_IndirectBuffer | qhwi::BufferUsage_UnorderedAccess,
                                  .size = size,
                                  .format = {},
                                  .name = name,
                                  .automaticBarriers = true };
        init(device, desc, nullptr);
    }

    void init(qhwi::Device* device, const qhwi::BufferDesc& desc, void* data = nullptr);
    void update(void* data, uint32_t size);

    void initReadbackBuffer(qhwi::Device* device);

    void clear(qhwi::CommandList* cmdList, uint8_t value = 0);

    void uploadFromStagingBuffer(qhwi::CommandList* cmdList, uint32_t srcOffset = 0, uint32_t srcSize = 0, uint32_t dstOffset = 0);
    void downloadToReadbackBuffer(qhwi::CommandList* commandList, uint32_t srcOffset = 0, uint32_t srcSize = 0, uint32_t dstOffset = 0);

private:
    qhwi::Device* m_device = nullptr;
};

struct ScopedBufferMap
{
    ScopedBufferMap() {}

    ScopedBufferMap(qhwi::Buffer* buffer)
        : buffer(buffer)
    {
    }

    void* map(uint64_t offset = 0, uint64_t size = 0) { return buffer->map(offset, size); }

    ~ScopedBufferMap() { buffer->unmap(); }

    qhwi::Buffer* buffer = nullptr;
};

class Texture1
{
public:
    qre::RefCountPtr<qhwi::Texture> texture;
    qre::RefCountPtr<qhwi::Buffer> stagingBuffer;
    Buffer1 readbackBuffer;

    void init(qhwi::Device* device,
              const qhwi::TextureDesc& desc,
              const void* data = nullptr,
              bool supportsReadback = false,
              qhwi::Texture::Handle handle = nullptr,
              qhwi::Texture::Handle handle2 = nullptr);

    void initFromHandle(qhwi::Device* device,
                        const qhwi::TextureDesc& desc,
                        qhwi::Texture::Handle handle,
                        qhwi::Texture::Handle handle2 = nullptr,
                        const void* data = nullptr,
                        bool supportsReadback = false)
    {
        QRE_ASSERT(handle);
        init(device, desc, data, supportsReadback, handle, handle2);
    }

    void initReadbackBuffer(qhwi::Device* device);
    void initStagingBuffer(qhwi::Device* device);

    qhwi::Texture* get() { return texture.Get(); }

    void uploadFromStagingBuffer(qhwi::CommandList* commandList);
    void downloadToReadbackBuffer(qhwi::CommandList* commandList);
};

struct TextureUtil
{
    static void readPng(Texture1& textureObj, const char* file);
    static void writeToPng(Texture1& textureObj, const char* file);
};

class Task
{
public:
    virtual ~Task() {}

protected:
    void init(qhwi::Device* device)
    {
        m_device = device;
        m_isUma = m_device->isUnifiedMemoryArchitecture();
        m_graphicsBackend = m_device->getGraphicsBackend();
    }

    qhwi::GraphicsBackend m_graphicsBackend;
    qhwi::Device* m_device = nullptr;
    bool m_isUma = false;
};

class Geometry
{
public:
    virtual ~Geometry() {}
};

class Quad : public Geometry
{
public:
    void init(float2 posData[2] = nullptr, float2 uvData[2] = nullptr);
    static const int NumVerts = 4;
    float2 pos[NumVerts];
    float2 uv[NumVerts];
};

class DrawColorTask : public Task
{
public:
    struct InitParams
    {
        qhwi::Device* device;
        float4 color;
        float2* posData = nullptr;
        uint32_t numVerts = 0;
        uint16_t* indicesData = nullptr;
        uint32_t numIndices = 0;
        qhwi::PrimitiveTopology primitiveTopology = qhwi::PrimitiveTopology::eTriangleStrip;
        qhwi::Format colorAttachmentFormat;
        qhwi::Format depthStencilFormat = qhwi::Format::eUnknown;
    };

    void init(const InitParams& initParams);
    virtual void uploadStagingBuffers(qhwi::CommandList* cmdList);
    virtual void draw(qhwi::CommandList* cmdList);
    void updateVerts(float2* posData, uint32_t numVerts);
    void updateIndices(uint16_t* indicesData, uint32_t numIndices);
    void updateColor(float4 color);

    Buffer1& bPos() { return m_bPos; }

    Buffer1& bIndices() { return m_bIndices; }

    Buffer1& bColor() { return m_cb; }

    static const uint32_t B_POS = 0, B_CB = 0;

    uint32_t numVerts() const { return m_numVerts; }

    uint32_t numIndices() const { return m_numIndices; }

    qhwi::GraphicsPipeline* graphicsPipeline() { return m_graphicsPipeline.Get(); }

    qhwi::DescriptorSet* descriptorSet(uint32_t index) { return m_descriptorSets[index].Get(); }

protected:
    virtual void initDescriptorSetsAndLayouts();
    void initPipeline(qhwi::PrimitiveTopology primitiveTopology, qhwi::Format colorAttachmentFormat, qhwi::Format depthStencilFormat);

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<qhwi::DescriptorSet> m_descriptorSets[NumDescriptorSets];
    qhwi::DescriptorBufferInfo m_bufferInfos[1];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos;
    Buffer1 m_bIndices;
    Buffer1 m_cb;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
    qhwi::PrimitiveTopology m_primitiveTopology;
    float4 m_color;

    struct CbData
    {
        float4 color;
    };
};

class DrawColor1Task : public Task
{
public:
    struct VSInput
    {
        float4 pos;
        float4 color;
    };

    struct CBInput
    {
        float4x4 modelViewProj[2];
    };

    struct InitParams
    {
        qhwi::Device* device;
        VSInput* vertsData = nullptr;
        uint32_t numVerts = 0;
        uint16_t* indicesData = nullptr;
        uint32_t numIndices = 0;
        CBInput* cbData = nullptr;
        qhwi::PrimitiveTopology primitiveTopology = qhwi::PrimitiveTopology::eTriangleStrip;
        qhwi::Format colorAttachmentFormat;
        qhwi::Format depthStencilFormat = qhwi::Format::eUnknown;
        qhwi::RasterizerState rasterizerState{};
        qhwi::MultiViewState multiViewState{};
    };

    void init(const InitParams& initParams);
    virtual void uploadStagingBuffers(qhwi::CommandList* cmdList);
    virtual void draw(qhwi::CommandList* cmdList);
    void updateVerts(VSInput* vertsData, uint32_t numVerts);
    void updateIndices(uint16_t* indicesData, uint32_t numIndices);
    void updateConstants(const CBInput& cb);

    Buffer1& bVerts() { return m_bVerts; }

    Buffer1& bIndices() { return m_bIndices; }

    static const uint32_t B_VERTS = 0;

    uint32_t numVerts() const { return m_numVerts; }

    uint32_t numIndices() const { return m_numIndices; }

    qhwi::GraphicsPipeline* graphicsPipeline() { return m_graphicsPipeline.Get(); }

protected:
    void initPipeline(qhwi::PrimitiveTopology primitiveTopology,
                      qhwi::Format colorAttachmentFormat,
                      qhwi::Format depthStencilFormat,
                      qhwi::RasterizerState rasterizerState,
                      qhwi::MultiViewState multiViewState);
    qhwi::DescriptorBufferInfo m_bufferInfos[1];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bVerts;
    Buffer1 m_bIndices;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
    qhwi::PrimitiveTopology m_primitiveTopology;
    CBInput m_cb;
};

class DrawCubeTask : public DrawColor1Task
{
public:
    struct InitParams
    {
        qhwi::Device* device;
        CBInput* cbData = nullptr;
        qhwi::Format colorAttachmentFormat;
        qhwi::Format depthStencilFormat = qhwi::Format::eUnknown;
        qhwi::RasterizerState rasterizerState{};
        qhwi::MultiViewState multiViewState{};
        float4 colors[6] = { { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f },
                             { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } };
    };

    void init(const InitParams& initParams);
};

class DrawTextureTask2D : public Task
{
public:
    struct InitParams
    {
        qhwi::Device* device;
        Texture1* texture;
        float2* posData = nullptr;
        uint32_t numVerts = 0;
        uint16_t* indicesData = nullptr;
        uint32_t numIndices = 0;
        float2* uvData = nullptr;
        qhwi::SamplerDesc samplerDesc = {};
        qhwi::Format colorAttachmentFormat;
        qhwi::Format depthStencilFormat = qhwi::Format::eUnknown;
        qhwi::Subresource subresource;
    };

    void init(const InitParams& initParams);
    void draw(qhwi::CommandList* cmdList);
    void uploadStagingBuffers(qhwi::CommandList* cmdList);
    void samplePixelCenter(bool samplePixelCenter);
    void setFrameBufferSize(float frameBufferWidth, float frameBufferHeight);

    Buffer1& bPos() { return m_bPos; }

    Buffer1& bUv() { return m_bUv; }

    Buffer1& bIndices() { return m_bIndices; }

    Texture1* texture() { return m_texture; }

private:
    void initDescriptorSetLayouts();
    void initDescriptorSets();
    void initPipeline(qhwi::Format colorAttachmentFormat, qhwi::Format depthStencilFormat);

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<qhwi::DescriptorSet> m_descriptorSets[NumDescriptorSets];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos, m_bUv;
    Buffer1 m_bIndices;
    Texture1* m_texture = nullptr;
    qre::RefCountPtr<qhwi::Sampler> m_sampler;
    static const uint32_t B_POS = 0, B_UV = 1;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;

    bool m_samplePixelCenter = true;
    float m_frameBufferWidth = 0;
    float m_frameBufferHeight = 0;

    qhwi::Subresource m_subresource;
};

class DrawTextureTask3D : public Task
{
public:
    struct InitParams
    {
        qhwi::Device* device;
        Texture1* texture;
        float3* posData = nullptr;
        uint32_t numVerts = 0;
        uint16_t* indicesData = nullptr;
        uint32_t numIndices = 0;
        float3* uvwData = nullptr;
        qhwi::SamplerDesc samplerDesc = {};
        float4x4* transform = nullptr;
        const char* vertexShader = nullptr;
        const char* pixelShader = nullptr;
        qhwi::Format colorAttachmentFormat;
        qhwi::Format depthStencilFormat = qhwi::Format::eUnknown;
    };

    void init(const InitParams& initParams);
    void draw(qhwi::CommandList* cmdList);
    void uploadStagingBuffers(qhwi::CommandList* cmdList);

private:
    void initDescriptorSetLayouts();
    void initDescriptorSets();
    void initPipeline(qhwi::Format colorAttachmentFormat, qhwi::Format depthStencilFormat);

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    const char* m_vertexShader;
    const char* m_pixelShader;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<qhwi::DescriptorSet> m_descriptorSets[NumDescriptorSets];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos, m_bUv;
    Buffer1 m_bIndices;
    Buffer1 m_cbTransform;
    Texture1* m_texture = nullptr;
    qre::RefCountPtr<qhwi::Sampler> m_sampler;
    static const uint32_t B_POS = 0, B_UV = 1;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
};

class SampleTextureCubeTask : public Task
{
public:
    struct InitParams
    {
        qhwi::Device* device;
        Texture1* texture;
        test_util::float2* posData = nullptr;
        uint32_t numVerts = 0;
        uint16_t* indicesData = nullptr;
        uint32_t numIndices = 0;
        float3* uvwData = nullptr;
        qhwi::SamplerDesc samplerDesc = {};
        qhwi::Format colorAttachmentFormat;
        qhwi::Format depthStencilFormat = qhwi::Format::eUnknown;
    };

    void init(const InitParams& initParams);
    void draw(qhwi::CommandList* cmdList);
    void uploadStagingBuffers(qhwi::CommandList* cmdList);

private:
    void initDescriptorSetsAndLayouts();
    void initPipeline(qhwi::Format colorAttachmentFormat, qhwi::Format depthStencilFormat);

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<qhwi::DescriptorSet> m_descriptorSets[NumDescriptorSets];
    qhwi::DescriptorTextureInfo m_textureInfos[1];
    qhwi::DescriptorSamplerInfo m_samplerInfos[1];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos, m_bUv;
    Buffer1 m_bIndices;
    Texture1* m_texture = nullptr;
    qre::RefCountPtr<qhwi::Sampler> m_sampler;
    static const uint32_t B_POS = 0, B_UV = 1;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
};

template <typename T>
bool imageCompare(void* src,
                  const int width,
                  const int height,
                  void* ref,
                  const int srcPitch,
                  const int refPitch,
                  const int filter,
                  const bool verbose,
                  const qhwi::Format format,
                  const std::optional<T> threshold);

bool imageCompare(void* src, const int width, const int height, void* ref, const int srcPitch, const int refPitch, const int filter, const bool verbose, const qhwi::Format format);

template <typename T>
bool imageCompare(void* src,
                  const int width,
                  const int height,
                  void* ref,
                  const int srcPitch,
                  const int refPitch,
                  const int filter,
                  const bool verbose,
                  const qhwi::Format format,
                  const T threshold)
{
    return imageCompare(src, width, height, ref, srcPitch, refPitch, filter, verbose, format, std::optional<T>{ threshold });
}
} // namespace test_util
