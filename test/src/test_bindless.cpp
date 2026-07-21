//
//
//  test_bindless.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
#include <qhwi/qhwi_enums.h>
using namespace qhwi;
using namespace test_util;
#include <random>

class TestBindless : public UnitTest
{
public:
    TestBindless(GraphicsBackend graphicsBackend, Config config, const char* name = "test_bindless")
        : UnitTest(graphicsBackend, config, name)
    {
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        initBuffers();
        initTextures();
        initSamplers();
        initDescriptorSetLayouts();
        initDescriptorSets();
        initPipelines();

        beginTransfer();
        uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void initBuffers()
    {
        VSInput data[NumVerts];
        auto p = data;
        float xInc = 2.0f / float(DIM_X);
        float yInc = 2.0f / float(DIM_Y);
        int texIndex = 0;
        for (uint32_t y = 0; y < DIM_Y; y++)
        {
            float py = -1.0f + 2.0f * (y / float(DIM_Y));
            for (uint32_t x = 0; x < DIM_X; x++)
            {
                float px = -1.0f + 2.0f * (x / float(DIM_X));
                *p++ = { .pos = { px, py }, .textureIndex = texIndex };
                *p++ = { .pos = { px + xInc, py }, .textureIndex = texIndex };
                *p++ = { .pos = { px, py + yInc }, .textureIndex = texIndex };
                *p++ = { .pos = { px + xInc, py }, .textureIndex = texIndex };
                *p++ = { .pos = { px + xInc, py + yInc }, .textureIndex = texIndex };
                *p++ = { .pos = { px, py + yInc }, .textureIndex = texIndex };
                texIndex = (texIndex + 1) % NumTextures;
            }
        }
        m_vertexBuffer.initAsVertexBuffer(m_device.Get(), "VSInput", data, NumVerts * sizeof(VSInput));
    }

    void initTextures()
    {
        const uint32_t w = 4, h = 4;
        uint32_t r = 0, g = 0, b = 0, a = 0xFF;
        for (uint32_t j = 0; j < NumTextures; j++)
        {
            auto& texture = m_textures[j];
            uint32_t data[w * h];
            uint32_t color = (a << 24) | (b << 16) | (g << 8) | r;
            for (uint32_t k = 0; k < (w * h); k++)
            {
                data[k] = color;
            }
            texture.init(m_device.Get(),
                         { .heapFlags = {},
                           .w = w,
                           .h = h,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Sampled,
                           .type = TextureType::e2D,
                           .name = "texture",
                           .automaticBarriers = true },
                         data);
            r = (r + 7) % 255;
            g = (g + 21) % 255;
            b = (b + 45) % 255;
        }
    }

    void uploadStagingBuffers(CommandList* cmdList)
    {
        m_vertexBuffer.uploadFromStagingBuffer(cmdList);

        for (auto& texture : m_textures)
        {
            if (texture.stagingBuffer)
            {
                texture.uploadFromStagingBuffer(cmdList);
            }
        }
    }

    void initSamplers() { m_sampler = m_device->createSampler({}); }

    void initDescriptorSetLayouts()
    {
        m_descriptorSetLayouts[0] = m_device->create(DescriptorSetLayoutDesc(0).addSampler(0, ShaderStage_Pixel));
        m_descriptorSetLayouts[1] = m_device->create(
            DescriptorSetLayoutDesc(1).addTextures(0, ShaderStage_Pixel, MaxTextures, DescriptorBindingFlag_VariableDescriptorCount | DescriptorBindingFlag_UpdateAfterBind));
    }

    void initDescriptorSets()
    {
        {
            m_descriptorSets[0] = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayouts[0]).addSampler(0, m_sampler));
        }

        {
            m_descriptorSets[1] = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayouts[1]).setVariableDescriptorCount(NumTextures));
            DescriptorTextureInfo textureInfos[NumTextures];
            for (uint32_t j = 0; j < NumTextures; j++)
            {
                textureInfos[j] = { .texture = m_textures[j].get() };
            }
            m_device->updateDescriptorSet(m_descriptorSets[1], textureInfos, DescriptorType::eTexture, 0, 0, NumTextures);
        }
    }

    void initPipelines()
    {
        m_pipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]).addDescriptorSetLayout(m_descriptorSetLayouts[1]));

        Shader1 vs, ps;
        initVertexShader(m_device.Get(), "testBindless", vs);
        initPixelShader(m_device.Get(), "testBindless", ps);

        m_graphicsPipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                                .pixelShader = ps.shader,
                                                                .domainShader = {},
                                                                .hullShader = {},
                                                                .geometryShader = {},
                                                                .amplificationShader = {},
                                                                .meshShader = {},
                                                                .blendState = {},
                                                                .rasterizerState = { .cullMode = CullMode::eNone },
                                                                .multisampleState = {},
                                                                .depthStencilState = {},
                                                                .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                              .semanticIndex = 0,
                                                                                                                              .binding = 0,
                                                                                                                              .format = Format::eRgb32Float,
                                                                                                                              .offset = offsetof(VSInput, pos),
                                                                                                                              .stride = sizeof(VSInput),
                                                                                                                              .inputRate = {} },
                                                                                                                            { .semanticName = "TEXINDEX",
                                                                                                                              .semanticIndex = 0,
                                                                                                                              .binding = 0,
                                                                                                                              .format = Format::eR32Int,
                                                                                                                              .offset = offsetof(VSInput, textureIndex),
                                                                                                                              .stride = sizeof(VSInput),
                                                                                                                              .inputRate = {} } } },
                                                                .viewportState = {},
                                                                .scissorState = {},
                                                                .layout = m_pipelineLayout,
                                                                .primitiveTopology = PrimitiveTopology::eTriangleList,
                                                                .primitiveRestartValue = {},
                                                                .colorAttachmentFormats = { m_colorAttachmentFormat },
                                                                .depthStencilFormat = m_depthStencilAttachmentFormat,
                                                                .meshShaderParams = {},
                                                                .multiViewState = {},
                                                                .colorAttachmentRemapping = {},
                                                                .name = {} });
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);

        commandList->bindDescriptorSet(m_descriptorSets[0].Get(), qhwi::PipelineBindPoint::eGraphics, m_pipelineLayout.Get());
        commandList->bindDescriptorSet(m_descriptorSets[1].Get(), qhwi::PipelineBindPoint::eGraphics, m_pipelineLayout.Get());
        commandList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        commandList->bindVertexBuffer(m_vertexBuffer.get(), 0, 0, sizeof(VSInput));
        commandList->draw(NumVerts);
        endRenderPass(commandList);
        commandList->end();
    }

private:
    struct VSInput
    {
        float2 pos;
        int textureIndex;
    };

    static const uint32_t NumTextures = 100;
    static const uint32_t MaxTextures = NumTextures;
    static const uint32_t DIM_X = 10, DIM_Y = 10;
    static const uint32_t NumVerts = 6 * DIM_X * DIM_Y;
    Texture1 m_textures[NumTextures];
    Buffer1 m_vertexBuffer;
    qre::RefCountPtr<Sampler> m_sampler;
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[2];
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[2];
    qre::RefCountPtr<PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
};

UnitTest* create_test_bindless(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestBindless(backend, config);
}
