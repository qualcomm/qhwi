//
//
//  test_util.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include <qre/memory/align.h>

using namespace qhwi;
using namespace test_util;

void Shader1::init(Device* device, std::string path, std::string entryPointName1, ShaderType shaderType)
{
#ifdef QHWI_PLATFORM_ANDROID
    FileUtil::readAndroidAsset(path.c_str(), byteCode);
#else
    FileUtil::readFile(path.c_str(), byteCode);
#endif
    entryPointName = entryPointName1;
    shader = device->createShader({ .name = nullptr, .entryPointName = entryPointName.c_str(), .shaderType = shaderType }, byteCode.data, byteCode.size);
}

void Buffer1::init(Device* device, const BufferDesc& desc, void* data)
{
    m_device = device;
    size_t size = desc.size;
    buffer = device->createBuffer(desc);
    update(data, uint32_t(size));
}

void Buffer1::update(void* data, uint32_t size)
{
    if (!data)
    {
        return;
    }
    if (buffer->heapType() == HeapType::eUpload)
    {
        void* dstData = buffer->map();
        memcpy(dstData, data, size);
        buffer->unmap();
    }
    else if (buffer->heapType() == HeapType::eDefault)
    {
        stagingBuffer =
            m_device->createBuffer({ .heapType = HeapType::eUpload, .heapFlags = {}, .usage = {}, .size = size, .format = {}, .name = "stagingBuffer", .automaticBarriers = {} });
        void* dstData = stagingBuffer->map();
        memcpy(dstData, data, size);
        stagingBuffer->unmap();
    }
}

void Buffer1::initReadbackBuffer(Device* device)
{
    readbackBuffer = device->createBuffer(
        { .heapType = HeapType::eDownload, .heapFlags = {}, .usage = BufferUsageMask(0), .size = buffer->size(), .format = {}, .name = "rbBuffer", .automaticBarriers = {} });
}

void Buffer1::clear(CommandList* cmdList, uint8_t value)
{
    cmdList->clear(buffer.Get(), value);
}

void Buffer1::uploadFromStagingBuffer(CommandList* cmdList, uint32_t srcOffset, uint32_t srcSize, uint32_t dstOffset)
{
    cmdList->copyBuffer(buffer.Get(), stagingBuffer.Get(), { dstOffset, srcOffset, srcSize });
}

void Buffer1::downloadToReadbackBuffer(CommandList* commandList, uint32_t srcOffset, uint32_t srcSize, uint32_t dstOffset)
{
    if (!readbackBuffer)
    {
        initReadbackBuffer(m_device);
    }
    commandList->copyBuffer(readbackBuffer.Get(), buffer.Get(), { dstOffset, srcOffset, srcSize });
}

void Texture1::init(Device* device, const TextureDesc& desc, const void* data, bool supportsReadback, qhwi::Texture::Handle handle, qhwi::Texture::Handle handle2)
{
    uint32_t blockHeight = FormatUtil::c_FormatInfo[uint32_t(desc.format)].blockHeight;
    uint32_t w = desc.w, d = desc.d * desc.arrayLayers;
    uint32_t h = qre::AlignUp(desc.h, blockHeight) / blockHeight;

    if (handle)
    {
        texture = device->createTextureFromHandle(desc, handle, handle2);
    }
    else
    {
        texture = device->createTexture(desc);
    }

    if (supportsReadback)
    {
        initReadbackBuffer(device);
    }

    if (!data)
    {
        return;
    }
    uint32_t dstPitch = texture->pitch();
    initStagingBuffer(device);
    uint8_t* srcPtr = (uint8_t*)data;
    uint32_t srcPitch = FormatUtil::getRowPitch(desc.format, w);
    uint8_t* dstPtr = (uint8_t*)stagingBuffer->map();

    for (uint32_t i = 0; i < d; i++)
    {
        for (uint32_t j = 0; j < h; j++)
        {
            memcpy(dstPtr, srcPtr, srcPitch);
            srcPtr += srcPitch;
            dstPtr += dstPitch;
        }
    }

    stagingBuffer->unmap();
}

void Texture1::initReadbackBuffer(Device* device)
{
    auto desc = texture->desc();
    //TODO: if UMA, change layout of offscreen texture and read from it
    //     directly, don't use readback buffer

    size_t sizeInBytes = 0;

    const auto mipLevels = desc->mipLevels;
    const auto h = desc->h;
    const auto d = desc->d;

    for (uint32_t level = 0; level < mipLevels; ++level)
    {
        const auto mipHeight = std::max(h >> level, 1u);
        const auto mipDepth = std::max(d >> level, 1u);
        const auto rowPitch = texture->pitch(uint16_t(level));

        sizeInBytes += mipHeight * rowPitch * mipDepth;
    }

    readbackBuffer.initAsReadbackBuffer(device, "rbTexture", static_cast<uint32_t>(sizeInBytes));
}

void Texture1::initStagingBuffer(Device* device)
{
    auto desc = texture->desc();
    stagingBuffer = device->createBuffer({ .heapType = HeapType::eUpload,
                                           .heapFlags = {},
                                           .usage = {},
                                           .size = size_t(texture->pitch()) * desc->h * desc->d * desc->arrayLayers,
                                           .format = {},
                                           .name = "sb_texture",
                                           .automaticBarriers = {} });
}

void Texture1::uploadFromStagingBuffer(CommandList* commandList)
{
    if (stagingBuffer)
    {
        auto textureDesc = texture->desc();

        uint32_t blockHeight = FormatUtil::c_FormatInfo[uint32_t(textureDesc->format)].blockHeight;
        uint32_t d = textureDesc->d, h = qre::AlignUp(textureDesc->h, blockHeight) / blockHeight;

        auto arrayLayers = textureDesc->arrayLayers;
        auto srcPitch = texture->pitch();
        std::vector<qhwi::CopyBufferToTextureRegion> regions(arrayLayers);
        std::vector<Subresource> subresources(arrayLayers);

        for (uint32_t i = 0; i < arrayLayers; ++i)
        {
            subresources[i].arraySlice = uint16_t(i);
            regions[i].dstSubresource = &subresources[i];
            regions[i].srcOffset = size_t(i) * srcPitch * h * d;
        }

        commandList->copyBufferToTexture(texture.Get(), stagingBuffer.Get(), regions);
    }
}

void Texture1::downloadToReadbackBuffer(CommandList* commandList)
{
    const auto& textureDesc = texture->desc();
    const auto mipLevels = textureDesc->mipLevels;
    const auto w = textureDesc->w;
    const auto h = textureDesc->h;
    const auto d = textureDesc->d;
    size_t offset = 0;

    for (uint16_t mip = 0; mip < mipLevels; ++mip)
    {
        const auto mipWidth = std::max(w >> mip, 1u);
        const auto mipHeight = std::max(h >> mip, 1u);
        const auto mipDepth = std::max(d >> mip, 1u);
        const auto rowPitch = texture->pitch(mip);

        Subresource subresource = { .mipSlice = mip, .arraySlice = {}, .planeSlice = {} };

        CopyTextureToBufferRegion region;
        region.dstOffset = static_cast<uint32_t>(offset);
        region.srcSubresource = &subresource;
        region.srcExtent = { .w = mipWidth, .h = mipHeight, .d = mipDepth };

        offset += mipHeight * rowPitch * mipDepth;

        commandList->copyTextureToBuffer(readbackBuffer.get(), texture.Get(), region);
    }
}

void test_util::TextureUtil::readPng(Texture1& textureObj, const char* file)
{
    ImageData imgData;
    auto resultCode = ext::ImageUtil::readImage(file, imgData);
    if (resultCode != ext::ImageUtil::ResultCode::eSuccess)
    {
        QHWI_ERR("failed to load image: %s", file);
        return;
    }
    void* offscreenSurfaceData = textureObj.stagingBuffer->map();
    memcpy(offscreenSurfaceData, imgData.data, imgData.size);
    textureObj.stagingBuffer->unmap();
}

void test_util::TextureUtil::writeToPng(Texture1& textureObj, const char* file)
{
    void* offscreenSurfaceData = textureObj.readbackBuffer.buffer->map();
    auto d0 = textureObj.texture->desc();
    ext::ImageUtil::writePng(file,
                             { .w = int(d0->w), .h = int(d0->h), .format = d0->format, .numChannels = 4, .data = offscreenSurfaceData, .pitch = int(textureObj.texture->pitch()) });
    textureObj.readbackBuffer.buffer->unmap();
}

void Quad::init(float2 posData[2], float2 uvData[2])
{
    float2* p = pos;
    if (!posData)
    {
        *p++ = { -1.0f, -1.0f };
        *p++ = { -1.0f, +1.0f };
        *p++ = { +1.0f, -1.0f };
        *p++ = { +1.0f, +1.0f };
    }
    else
    {
        const float2 &p0 = posData[0], &p1 = posData[1];
        *p++ = { p0.x, p0.y };
        *p++ = { p0.x, p1.y };
        *p++ = { p1.x, p0.y };
        *p++ = { p1.x, p1.y };
    }
    p = uv;
    if (!uvData)
    {
        *p++ = { 0.0f, 1.0f };
        *p++ = { 0.0f, 0.0f };
        *p++ = { 1.0f, 1.0f };
        *p++ = { 1.0f, 0.0f };
    }
    else
    {
        const float2 &p0 = uvData[0], &p1 = uvData[1];
        *p++ = { p0.x, p0.y };
        *p++ = { p0.x, p1.y };
        *p++ = { p1.x, p0.y };
        *p++ = { p1.x, p1.y };
    }
}

void DrawColorTask::init(const InitParams& p)
{
    Task::init(p.device);
    m_color = p.color;
    m_primitiveTopology = p.primitiveTopology;
    m_numVerts = p.numVerts;
    m_numIndices = p.numIndices;

    CbData cbData = { .color = m_color };
    Quad quad;
    float2* posData = p.posData;
    uint32_t numVerts = p.numVerts;
    if (!posData)
    {
        quad.init();
        posData = quad.pos;
        numVerts = quad.NumVerts;
    }
    m_cb.initAsConstantBuffer(m_device, "cb", &cbData, sizeof(CbData));
    m_bPos.initAsVertexBuffer(m_device, "pos", posData, numVerts * sizeof(float2));
    if (m_numIndices)
    {
        m_bIndices.initAsIndexBuffer(m_device, "indices", p.indicesData, p.numIndices * sizeof(uint16_t));
    }
    initDescriptorSetsAndLayouts();
    initPipeline(p.primitiveTopology, p.colorAttachmentFormat, p.depthStencilFormat);
}

void DrawColorTask::updateVerts(float2* posData, uint32_t numVerts)
{
    m_bPos.update(posData, numVerts * sizeof(float2));
}

void DrawColorTask::updateIndices(uint16_t* indicesData, uint32_t numIndices)
{
    m_bIndices.update(indicesData, numIndices * sizeof(uint16_t));
}

void DrawColorTask::updateColor(float4 color)
{
    m_cb.update(&color, sizeof(color));
}

void DrawColorTask::uploadStagingBuffers(CommandList* cmdList)
{
    m_bPos.uploadFromStagingBuffer(cmdList);

    if (m_numIndices)
    {
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }
    m_cb.uploadFromStagingBuffer(cmdList);
}

void DrawColorTask::draw(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
    }
    cmdList->bindGraphicsDescriptorSet(m_descriptorSets[0].Get());
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void DrawColorTask::initDescriptorSetsAndLayouts()
{
    m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addConstantBuffer(0, m_cb.buffer), ShaderStage_Vertex, 0, m_descriptorSetLayouts[0], m_descriptorSets[0]);
}

void DrawColorTask::initPipeline(PrimitiveTopology primitiveTopology, Format colorAttachmentFormat, Format depthStencilFormat)
{
    m_graphicsPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

    Shader1 vs, ps;
    initVertexShader(m_device, "testUtilDrawColor", vs);
    initPixelShader(m_device, "testUtilDrawColor", ps);

    m_graphicsPipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                            .pixelShader = ps.shader,
                                                            .domainShader = {},
                                                            .hullShader = {},
                                                            .geometryShader = {},
                                                            .amplificationShader = {},
                                                            .meshShader = {},
                                                            .blendState = {},
                                                            .rasterizerState = {},
                                                            .multisampleState = {},
                                                            .depthStencilState = {},
                                                            .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_POS,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float2),
                                                                                                                          .inputRate = {} } } },
                                                            .viewportState = {},
                                                            .scissorState = {},
                                                            .layout = m_graphicsPipelineLayout,
                                                            .primitiveTopology = primitiveTopology,
                                                            .primitiveRestartValue = {},
                                                            .colorAttachmentFormats = { colorAttachmentFormat },
                                                            .depthStencilFormat = depthStencilFormat,
                                                            .meshShaderParams = {},
                                                            .multiViewState = {},
                                                            .colorAttachmentRemapping = {},
                                                            .name = {} });
}

void DrawColor1Task::init(const InitParams& p)
{
    Task::init(p.device);
    m_primitiveTopology = p.primitiveTopology;
    m_numVerts = p.numVerts;
    m_numIndices = p.numIndices;

    m_bVerts.initAsVertexBuffer(m_device, "verts", p.vertsData, p.numVerts * sizeof(VSInput));
    if (m_numIndices)
    {
        m_bIndices.initAsIndexBuffer(m_device, "indices", p.indicesData, p.numIndices * sizeof(uint16_t));
    }
    if (p.cbData)
    {
        m_cb = *p.cbData;
    }
    initPipeline(p.primitiveTopology, p.colorAttachmentFormat, p.depthStencilFormat, p.rasterizerState, p.multiViewState);
}

void DrawColor1Task::updateVerts(VSInput* vertsData, uint32_t numVerts)
{
    m_bVerts.update(vertsData, numVerts * sizeof(VSInput));
}

void DrawColor1Task::updateIndices(uint16_t* indicesData, uint32_t numIndices)
{
    m_bIndices.update(indicesData, numIndices * sizeof(uint16_t));
}

void DrawColor1Task::updateConstants(const CBInput& cb)
{
    m_cb = cb;
}

void DrawColor1Task::uploadStagingBuffers(CommandList* cmdList)
{
    m_bVerts.uploadFromStagingBuffer(cmdList);

    if (m_numIndices)
    {
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }
}

void DrawColor1Task::draw(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bVerts.get(), B_VERTS, 0, sizeof(VSInput));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
    }
    cmdList->pushGraphicsConstants(&m_cb, 0);
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void DrawColor1Task::initPipeline(PrimitiveTopology primitiveTopology,
                                  Format colorAttachmentFormat,
                                  Format depthStencilFormat,
                                  qhwi::RasterizerState rasterizerState,
                                  MultiViewState multiViewState)
{
    m_graphicsPipelineLayout = m_device->create(
        PipelineLayoutDesc().addPushConstantRange({ .shaderRegister = {}, .registerSpace = {}, .shaderStages = ShaderStage_Vertex, .offset = {}, .size = sizeof(CBInput) }));

    Shader1 vs, ps;
    initVertexShader(m_device, "testUtilDrawColor1", vs);
    initPixelShader(m_device, "testUtilDrawColor1", ps);
    bool enableDepthTest = (depthStencilFormat != Format::eUnknown);
    m_graphicsPipeline = m_device->createGraphicsPipeline(
        {
            .vertexShader = vs.shader,
            .pixelShader = ps.shader,
            .domainShader = {},
            .hullShader = {},
            .geometryShader = {},
            .amplificationShader = {},
            .meshShader = {},
            .blendState = {},
            .rasterizerState = rasterizerState,
            .multisampleState = {},
            .depthStencilState =
            {
                .enableDepthTest = enableDepthTest,
                .enableDepthWrite = enableDepthTest,
                .depthCompareOp = CompareOp::eLess,
                .enableStencilTest = {},
                .stencilReadMask = 0xff,
                .stencilWriteMask = 0xff,
                .frontFace = {},
                .backFace = {},
                .stencilReference = {},
            },
            .vertexInputState =
            {
                .vertexInputAttributeDescriptions =
                {
                    {
                        .semanticName = "POSITION",
                        .semanticIndex = 0,
                        .binding = B_VERTS,
                        .format = Format::eRgba32Float,
                        .offset = {},
                        .stride = sizeof(VSInput),
                        .inputRate = {}
                    },
                    {
                        .semanticName = "COLOR",
                        .semanticIndex = 0,
                        .binding = B_VERTS,
                        .format = Format::eRgba32Float,
                        .offset = sizeof(float4),
                        .stride = sizeof(VSInput),
                        .inputRate = {}
                    }
                }
            },
            .viewportState = {},
            .scissorState = {},
            .layout = m_graphicsPipelineLayout,
            .primitiveTopology = primitiveTopology,
            .primitiveRestartValue = {},
            .colorAttachmentFormats =
            {
                colorAttachmentFormat
            },
            .depthStencilFormat = depthStencilFormat,
            .meshShaderParams = {},
            .multiViewState = multiViewState,
            .colorAttachmentRemapping = {},
            .name = {},
        });
}

void DrawCubeTask::init(const InitParams& initParams)
{
    using VSInput = DrawColor1Task::VSInput;
    const float4* colors = initParams.colors;
    const float4 pos[] = { { -.5f, -.5f, -.5f, 1.0f }, { -.5f, -.5f, +.5f, 1.0f }, { -.5f, +.5f, -.5f, 1.0f }, { -.5f, +.5f, +.5f, 1.0f },
                           { +.5f, -.5f, -.5f, 1.0f }, { +.5f, -.5f, +.5f, 1.0f }, { +.5f, +.5f, -.5f, 1.0f }, { +.5f, +.5f, +.5f, 1.0f } };
    const half4 faces[6] = {
        { 0, 2, 4, 6 }, //front
        { 5, 7, 1, 3 }, //back
        { 1, 3, 0, 2 }, //left
        { 4, 6, 5, 7 }, //right
        { 1, 0, 5, 4 }, //bottom
        { 2, 3, 6, 7 }  //top
    };
    const int NumVerts = 36;
    VSInput verts[NumVerts];

    VSInput* vert = verts;
    for (int j = 0; j < 6; j++)
    {
        const half4& face = faces[j];
        const float4& c = colors[j];
        *vert++ = { pos[face.x], c };
        *vert++ = { pos[face.y], c };
        *vert++ = { pos[face.z], c };
        *vert++ = { pos[face.z], c };
        *vert++ = { pos[face.y], c };
        *vert++ = { pos[face.w], c };
    }

    DrawColor1Task::init({ .device = initParams.device,
                           .vertsData = verts,
                           .numVerts = NumVerts,
                           .indicesData = {},
                           .numIndices = {},
                           .cbData = initParams.cbData,
                           .primitiveTopology = PrimitiveTopology::eTriangleList,
                           .colorAttachmentFormat = initParams.colorAttachmentFormat,
                           .depthStencilFormat = initParams.depthStencilFormat,
                           .rasterizerState = initParams.rasterizerState,
                           .multiViewState = initParams.multiViewState });
}

struct DrawTextureTask2DPushConstants
{
    uint32_t samplePixelCenter;
    qre::math::Vec2f frameBufferSize;
    uint32_t padding;
};

void DrawTextureTask2D::init(const InitParams& p)
{
    Task::init(p.device);
    QHWI_ASSERT(p.texture);
    m_texture = p.texture;
    m_numVerts = p.numVerts;
    m_numIndices = p.numIndices;

    Quad quad;
    float2* posData = p.posData;
    float2* uvData = p.uvData;
    if (!posData)
    {
        quad.init();
        posData = quad.pos;
        uvData = quad.uv;
        m_numVerts = quad.NumVerts;
    }
    m_bPos.initAsVertexBuffer(m_device, "pos", posData, m_numVerts * sizeof(float2));
    m_bUv.initAsVertexBuffer(m_device, "uv", uvData, m_numVerts * sizeof(float2));
    auto samplerDesc = p.samplerDesc;
    samplerDesc.maxLod = (float)p.texture->get()->desc()->mipLevels;
    m_sampler = m_device->createSampler(samplerDesc);

    m_subresource = p.subresource;

    initDescriptorSetLayouts();
    initPipeline(p.colorAttachmentFormat, p.depthStencilFormat);
    initDescriptorSets();
}

void DrawTextureTask2D::draw(CommandList* cmdList)
{
    DrawTextureTask2DPushConstants pushConstants;
    pushConstants.samplePixelCenter = m_samplePixelCenter ? 1 : 0;
    pushConstants.frameBufferSize.x = m_frameBufferWidth;
    pushConstants.frameBufferSize.y = m_frameBufferHeight;

    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
    }
    cmdList->bindVertexBuffer(m_bUv.get(), B_UV, 0, sizeof(float2));
    cmdList->bindDescriptorSet(m_descriptorSets[0].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayout.Get());
    cmdList->pushGraphicsConstants(&pushConstants);
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void DrawTextureTask2D::uploadStagingBuffers(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    m_bPos.uploadFromStagingBuffer(cmdList);
    m_bUv.uploadFromStagingBuffer(cmdList);
    m_texture->uploadFromStagingBuffer(cmdList);

    if (hasIndices)
    {
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }
}

void DrawTextureTask2D::samplePixelCenter(bool samplePixelCenter)
{
    m_samplePixelCenter = samplePixelCenter;
}

void DrawTextureTask2D::setFrameBufferSize(float frameBufferWidth, float frameBufferHeight)
{
    m_frameBufferWidth = frameBufferWidth;
    m_frameBufferHeight = frameBufferHeight;
}

void DrawTextureTask2D::initDescriptorSetLayouts()
{
    m_descriptorSetLayouts[0] =
        m_device->create(DescriptorSetLayoutDesc().addTexture(0, ShaderStage_Pixel).addSampler(0, ShaderStage_Pixel).setName("DrawTextureTask2D_DescriptorSetLayout"));
}

void DrawTextureTask2D::initDescriptorSets()
{
    m_descriptorSets[0] = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayouts[0])
                                                            .addTexture(0, m_texture->texture, SubresourceRange(m_subresource), TextureType::e2D)
                                                            .addSampler(0, m_sampler)
                                                            .setName("DrawTextureTask2D_DescriptorSet"));
}

void DrawTextureTask2D::initPipeline(Format colorAttachmentFormat, Format depthStencilFormat)
{
    m_graphicsPipelineLayout = m_device->create(
        PipelineLayoutDesc()
            .addPushConstantRange(
                { .shaderRegister = {}, .registerSpace = {}, .shaderStages = ShaderStage_Vertex | ShaderStage_Pixel, .offset = {}, .size = sizeof(DrawTextureTask2DPushConstants) })
            .addDescriptorSetLayout(m_descriptorSetLayouts[0]));
    Shader1 vs, ps;
    initVertexShader(m_device, "testUtilDrawImage", vs);
    initPixelShader(m_device, "testUtilDrawImage", ps);

    m_graphicsPipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                            .pixelShader = ps.shader,
                                                            .domainShader = {},
                                                            .hullShader = {},
                                                            .geometryShader = {},
                                                            .amplificationShader = {},
                                                            .meshShader = {},
                                                            .blendState = {},
                                                            .rasterizerState = {},
                                                            .multisampleState = {},
                                                            .depthStencilState = {},
                                                            .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_POS,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float2),
                                                                                                                          .inputRate = {} },
                                                                                                                        { .semanticName = "TEXCOORD",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_UV,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float2),
                                                                                                                          .inputRate = {} } } },
                                                            .viewportState = {},
                                                            .scissorState = {},
                                                            .layout = m_graphicsPipelineLayout,
                                                            .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                                            .primitiveRestartValue = {},
                                                            .colorAttachmentFormats = { colorAttachmentFormat },
                                                            .depthStencilFormat = depthStencilFormat,
                                                            .meshShaderParams = {},
                                                            .multiViewState = {},
                                                            .colorAttachmentRemapping = {},
                                                            .name = {} });
}

void DrawTextureTask3D::init(const InitParams& p)
{
    Task::init(p.device);
    m_texture = p.texture;
    m_numVerts = p.numVerts;
    m_numIndices = p.numIndices;
    m_vertexShader = p.vertexShader;
    m_pixelShader = p.pixelShader;

    m_bPos.initAsVertexBuffer(m_device, "pos", p.posData, m_numVerts * sizeof(float3));
    m_bUv.initAsVertexBuffer(m_device, "uvw", p.uvwData, m_numVerts * sizeof(float3));
    m_bIndices.initAsIndexBuffer(m_device, "indices", p.indicesData, m_numIndices * sizeof(uint16_t));
    m_cbTransform.initAsConstantBuffer(m_device, "cbTransform", p.transform, sizeof(float4x4));
    m_sampler = m_device->createSampler(p.samplerDesc);

    initDescriptorSetLayouts();
    initPipeline(p.colorAttachmentFormat, p.depthStencilFormat);
    initDescriptorSets();
}

void DrawTextureTask3D::draw(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float3));
    cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
    cmdList->bindVertexBuffer(m_bUv.get(), B_UV, 0, sizeof(float3));
    cmdList->bindDescriptorSet(m_descriptorSets[0].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayout.Get());
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void DrawTextureTask3D::uploadStagingBuffers(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    m_bPos.uploadFromStagingBuffer(cmdList);
    m_bUv.uploadFromStagingBuffer(cmdList);
    m_texture->uploadFromStagingBuffer(cmdList);
    if (hasIndices)
    {
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }
    m_cbTransform.uploadFromStagingBuffer(cmdList);
}

void DrawTextureTask3D::initDescriptorSetLayouts()
{
    m_descriptorSetLayouts[0] = m_device->create(DescriptorSetLayoutDesc()
                                                     .addTexture(0, ShaderStage_Vertex | ShaderStage_Pixel)
                                                     .addSampler(0, ShaderStage_Vertex | ShaderStage_Pixel)
                                                     .addConstantBuffer(0, ShaderStage_Vertex | ShaderStage_Pixel));
}

void DrawTextureTask3D::initDescriptorSets()
{
    m_descriptorSets[0] =
        m_device->create(DescriptorSetDesc(m_descriptorSetLayouts[0]).addTexture(0, m_texture->texture).addSampler(0, m_sampler).addConstantBuffer(0, m_cbTransform.buffer));
}

void DrawTextureTask3D::initPipeline(Format colorAttachmentFormat, Format depthStencilFormat)
{
    m_graphicsPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));
    Shader1 vs, ps;
    initVertexShader(m_device, m_vertexShader, vs, "VSMain");
    initPixelShader(m_device, m_pixelShader, ps, "PSMain");

    m_graphicsPipeline = m_device->createGraphicsPipeline(
        {
            .vertexShader = vs.shader,
            .pixelShader = ps.shader,
            .domainShader = {},
            .hullShader = {},
            .geometryShader = {},
            .amplificationShader = {},
            .meshShader = {},
            .blendState = {},
            .rasterizerState = {},
            .multisampleState = {},
            .depthStencilState =
                {
                    .enableDepthTest = true,
                    .enableDepthWrite = {},
                    .depthCompareOp = CompareOp::eLess,
                    .enableStencilTest = {},
                    .stencilReadMask = {},
                    .stencilWriteMask = {},
                    .frontFace = {},
                    .backFace = {},
                    .stencilReference = {},
                },
            .vertexInputState =
            {
                .vertexInputAttributeDescriptions =
                {
                    {
                        .semanticName = "POSITION",
                        .semanticIndex = 0,
                        .binding = B_POS,
                        .format = Format::eRgb32Float,
                        .offset = {},
                        .stride = sizeof(float3),
                        .inputRate = {}
                    },
                    {
                        .semanticName = "TEXCOORD",
                        .semanticIndex = 0,
                        .binding = B_UV,
                        .format = Format::eRgb32Float,
                        .offset = {},
                        .stride = sizeof(float3),
                        .inputRate = {}
                    }
                }
            },
            .viewportState = {},
            .scissorState = {},
            .layout = m_graphicsPipelineLayout,
            .primitiveTopology = PrimitiveTopology::eTriangleList,
            .primitiveRestartValue = {},
            .colorAttachmentFormats = { colorAttachmentFormat },
            .depthStencilFormat = depthStencilFormat,
            .meshShaderParams = {},
            .multiViewState = {},
            .colorAttachmentRemapping = {},
            .name = {},
        });
}

void SampleTextureCubeTask::init(const InitParams& p)
{
    Task::init(p.device);
    m_texture = p.texture;
    m_numVerts = p.numVerts;
    m_numIndices = p.numIndices;

    Quad quad;
    float2* posData = p.posData;
    float3* uvwData = p.uvwData;

    m_bPos.initAsVertexBuffer(m_device, "pos", posData, m_numVerts * sizeof(float2));
    m_bUv.initAsVertexBuffer(m_device, "uv", uvwData, m_numVerts * sizeof(float3));
    m_sampler = m_device->createSampler(p.samplerDesc);

    initDescriptorSetsAndLayouts();
    initPipeline(p.colorAttachmentFormat, p.depthStencilFormat);
}

void SampleTextureCubeTask::draw(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
    }
    cmdList->bindVertexBuffer(m_bUv.get(), B_UV, 0, sizeof(float3));
    cmdList->bindDescriptorSet(m_descriptorSets[0].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayout.Get());
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void SampleTextureCubeTask::uploadStagingBuffers(CommandList* cmdList)
{
    m_bPos.uploadFromStagingBuffer(cmdList);
    m_bUv.uploadFromStagingBuffer(cmdList);
    m_texture->uploadFromStagingBuffer(cmdList);
}

void SampleTextureCubeTask::initDescriptorSetsAndLayouts()
{
    m_device->createDescriptorSetAndLayout(DescriptorSetDesc(m_descriptorSetLayouts[0]).addTexture(0, m_texture->texture).addSampler(0, m_sampler),
                                           ShaderStage_Pixel,
                                           0,
                                           m_descriptorSetLayouts[0],
                                           m_descriptorSets[0]);
}

void SampleTextureCubeTask::initPipeline(Format colorAttachmentFormat, Format depthStencilFormat)
{
    m_graphicsPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));
    Shader1 vs, ps;
    initVertexShader(m_device, "sampleTextureCube", vs);
    initPixelShader(m_device, "sampleTextureCube", ps);

    m_graphicsPipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                            .pixelShader = ps.shader,
                                                            .domainShader = {},
                                                            .hullShader = {},
                                                            .geometryShader = {},
                                                            .amplificationShader = {},
                                                            .meshShader = {},
                                                            .blendState = {},
                                                            .rasterizerState = {},
                                                            .multisampleState = {},
                                                            .depthStencilState = {},
                                                            .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_POS,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float2),
                                                                                                                          .inputRate = {} },
                                                                                                                        { .semanticName = "TEXCOORD",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_UV,
                                                                                                                          .format = Format::eRgb32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float3),
                                                                                                                          .inputRate = {} } } },
                                                            .viewportState = {},
                                                            .scissorState = {},
                                                            .layout = m_graphicsPipelineLayout,
                                                            .primitiveTopology = PrimitiveTopology::eTriangleList,
                                                            .primitiveRestartValue = {},
                                                            .colorAttachmentFormats = { colorAttachmentFormat },
                                                            .depthStencilFormat = depthStencilFormat,
                                                            .meshShaderParams = {},
                                                            .multiViewState = {},
                                                            .colorAttachmentRemapping = {},
                                                            .name = {} });
}

template <typename T>
struct SurfaceDesc
{
    T* data = nullptr;
    int w = 0, h = 0, pitch = 0;
};

static inline int clamp(const int v, const int vMin, const int vMax)
{
    return std::min(std::max(v, vMin), vMax);
}

template <typename T>
static inline void getPixel(T* dst, const SurfaceDesc<T>& desc, const int x, const int y, const int numChannels)
{
    const int clampedX = clamp(x, 0, desc.w - 1);
    const int clampedY = clamp(y, 0, desc.h - 1);
    memcpy(dst, desc.data + clampedY * desc.pitch + clampedX * numChannels, numChannels * sizeof(T));
}

#include <iostream>

template <typename T>
static void dumpRegion(const int x0, const int y0, const int x1, const int y1, SurfaceDesc<T>& srcDesc, SurfaceDesc<T>& refDesc, const int numChannels)
{
    for (int x = x0; x <= x1; ++x)
    {
        for (int y = y0; y <= y1; ++y)
        {
            std::vector<T> srcCol(numChannels);
            std::vector<T> refCol(numChannels);

            getPixel<T>(srcCol.data(), srcDesc, x, y, numChannels);
            getPixel<T>(refCol.data(), refDesc, x, y, numChannels);

            std::cerr << "[test_util::imageCompare][INFO]: x: " << x << ", y: " << y << ", src : (";
            for (int i = 0; i < numChannels - 1; ++i)
            {
                std::cerr << std::to_string(srcCol[i]) << ", ";
            }
            std::cerr << std::to_string(srcCol[numChannels - 1]) << ")";
            std::cerr << "ref : (";
            for (int i = 0; i < numChannels - 1; ++i)
            {
                std::cerr << std::to_string(refCol[i]) << ", ";
            }
            std::cerr << std::to_string(refCol[numChannels - 1]) << ")";
            std::cerr << std::endl;
        }
    }
}

#include <limits>

template <typename T>
T getMinVal()
{
    return std::numeric_limits<T>::min();
}

template <typename T>
T getMaxVal()
{
    return std::numeric_limits<T>::max();
}

// check if color is outside range [ min - threshold, max + threshold ]
// where min/max is the min/max color within the local region in the other image
template <typename T>
bool isWithinThreshold(const T src, const T ref, const T srcMin, const T srcMax, const T refMin, const T refMax, const T threshold);

template <>
bool isWithinThreshold(const uint8_t src, const uint8_t ref, const uint8_t srcMin, const uint8_t srcMax, const uint8_t refMin, const uint8_t refMax, const uint8_t threshold)
{
    // cast all to signed int
    const int sSrc = static_cast<int>(src);
    const int sRef = static_cast<int>(ref);
    const int sSrcMin = static_cast<int>(srcMin);
    const int sSrcMax = static_cast<int>(srcMax);
    const int sRefMin = static_cast<int>(refMin);
    const int sRefMax = static_cast<int>(refMax);
    const int sThreshold = static_cast<int>(threshold);

    return sSrc <= sRefMax + sThreshold && sSrc >= sRefMin - sThreshold && sRef <= sSrcMax + sThreshold && sRef >= sSrcMin - sThreshold;
}

template <>
bool isWithinThreshold(const float src, const float ref, const float srcMin, const float srcMax, const float refMin, const float refMax, const float threshold)
{
    return src <= refMax + threshold && src >= refMin - threshold && ref <= srcMax + threshold && ref >= srcMin - threshold;
}

// Implementation derived from imace project.
template <typename T, int numChannels>
static bool imageCompareImpl(void* src, const int width, const int height, void* ref, const int srcPitch, const int refPitch, const int filter, T threshold, const bool verbose)
{
    SurfaceDesc<T> srcDesc{ reinterpret_cast<T*>(src), width, height, srcPitch / static_cast<int>(sizeof(T)) };
    SurfaceDesc<T> refDesc{ reinterpret_cast<T*>(ref), width, height, refPitch / static_cast<int>(sizeof(T)) };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            std::vector<T> srcCol(numChannels), refCol(numChannels);
            getPixel(srcCol.data(), srcDesc, x, y, numChannels);
            getPixel(refCol.data(), refDesc, x, y, numChannels);

            if (0 == std::memcmp(srcCol.data(), refCol.data(), numChannels * sizeof(T)))
            {
                continue;
            }

            std::vector<T> srcMinCol(numChannels, getMaxVal<T>());
            std::vector<T> refMinCol(numChannels, getMaxVal<T>());
            std::vector<T> srcMaxCol(numChannels, getMinVal<T>());
            std::vector<T> refMaxCol(numChannels, getMinVal<T>());

            std::vector<T> tmpSrcCol(numChannels), tmpRefCol(numChannels);

            for (int yy = -filter; yy <= filter; ++yy)
            {
                for (int xx = -filter; xx <= filter; ++xx)
                {
                    getPixel(tmpSrcCol.data(), srcDesc, x + xx, y + yy, numChannels);
                    getPixel(tmpRefCol.data(), refDesc, x + xx, y + yy, numChannels);

                    for (int j = 0; j < numChannels; ++j)
                    {
                        srcMinCol[j] = std::min(tmpSrcCol[j], srcMinCol[j]);
                        srcMaxCol[j] = std::max(tmpSrcCol[j], srcMaxCol[j]);

                        refMinCol[j] = std::min(tmpRefCol[j], refMinCol[j]);
                        refMaxCol[j] = std::max(tmpRefCol[j], refMaxCol[j]);
                    }
                }
            }

            for (int i = 0; i < numChannels; ++i)
            {

                if (!isWithinThreshold(srcCol[i], refCol[i], srcMinCol[i], srcMaxCol[i], refMinCol[i], refMaxCol[i], threshold))
                {
                    if (verbose)
                    {
                        dumpRegion<T>(x - filter, y - filter, x + filter, y + filter, srcDesc, refDesc, numChannels);
                    }
                    return false;
                }
            }
        }
    }
    return true;
}

template <typename T>
bool test_util::imageCompare(void* src,
                             const int width,
                             const int height,
                             void* ref,
                             const int srcPitch,
                             const int refPitch,
                             const int filter,
                             const bool verbose,
                             const qhwi::Format format,
                             const std::optional<T> threshold)
{
    if constexpr (std::is_same_v<T, uint8_t>)
    {
        constexpr uint8_t defaultThreshold = 1;

        switch (format)
        {
        case qhwi::Format::eR8Unorm:
            return imageCompareImpl<uint8_t, 1>(src, width, height, ref, srcPitch, refPitch, filter, threshold.value_or(defaultThreshold), verbose);
        case qhwi::Format::eRg8Unorm:
            return imageCompareImpl<uint8_t, 2>(src, width, height, ref, srcPitch, refPitch, filter, threshold.value_or(defaultThreshold), verbose);
        case qhwi::Format::eRgba8Unorm:
            return imageCompareImpl<uint8_t, 4>(src, width, height, ref, srcPitch, refPitch, filter, threshold.value_or(defaultThreshold), verbose);
        default:
            break;
        }
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        constexpr float defaultThreshold = 1.0f / 255.0f;

        switch (format)
        {
        case qhwi::Format::eR32Float:
            return imageCompareImpl<float, 1>(src, width, height, ref, srcPitch, refPitch, filter, threshold.value_or(defaultThreshold), verbose);
        case qhwi::Format::eRg32Float:
            return imageCompareImpl<float, 2>(src, width, height, ref, srcPitch, refPitch, filter, threshold.value_or(defaultThreshold), verbose);
        case qhwi::Format::eRgba32Float:
            return imageCompareImpl<float, 4>(src, width, height, ref, srcPitch, refPitch, filter, threshold.value_or(defaultThreshold), verbose);
        default:
            break;
        }
    }

    QHWI_ASSERT_MSG(false, "Unsuported format.");
    return false;
}

bool test_util::imageCompare(void* src,
                             const int width,
                             const int height,
                             void* ref,
                             const int srcPitch,
                             const int refPitch,
                             const int filter,
                             const bool verbose,
                             const qhwi::Format format)
{
    switch (format)
    {
    case qhwi::Format::eR8Unorm:
    case qhwi::Format::eRg8Unorm:
    case qhwi::Format::eRgba8Unorm:
        return imageCompare(src, width, height, ref, srcPitch, refPitch, filter, verbose, format, std::optional<uint8_t>{ std::nullopt });
        break;
    case qhwi::Format::eR32Float:
    case qhwi::Format::eRg32Float:
    case qhwi::Format::eRgba32Float:
        return imageCompare(src, width, height, ref, srcPitch, refPitch, filter, verbose, format, std::optional<float>{ std::nullopt });
        break;
    default:
        return false;
    }
}
