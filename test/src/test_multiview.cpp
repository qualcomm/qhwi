//
//
//  test_multiview.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"

using namespace qhwi;
using namespace test_util;

class TestMultiView : public UnitTest
{
public:
    TestMultiView(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_multiview")
    {
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        if (!m_device->supportsMultiView())
        {
            return ResultCode::eUnsupported;
        }
        bool supportsPerViewViewports = m_device->supportsMultiViewPerViewViewportsQCOM();
        Shader1 vs, ps;
        initVertexAndPixelShader(m_device.Get(), "testMultiView", vs, ps);
        m_descSetLayout = m_device->create(DescriptorSetLayoutDesc().setName("multiView_descSetLayout"));
        m_pipelineLayout = m_device->create(PipelineLayoutDesc().setName("multiView_pipelineLayout"));
        m_pipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
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
                                                        .vertexInputState = {},
                                                        .viewportState{ .numViewports = supportsPerViewViewports ? 2U : 1U },
                                                        .scissorState = { .numScissors = supportsPerViewViewports ? 2U : 1U },
                                                        .layout = m_pipelineLayout,
                                                        .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                                        .primitiveRestartValue = {},
                                                        .colorAttachmentFormats = { Format::eRgba8Unorm },
                                                        .depthStencilFormat = {},
                                                        .meshShaderParams = {},
                                                        .multiViewState = { .numViews = 2 },
                                                        .colorAttachmentRemapping = {},
                                                        .name = {} });
        m_multiViewTexture.init(m_device.Get(),
                                TextureDesc()
                                    .setName("multiViewTexture")
                                    .setWidth(m_colorAttachmentWidth)
                                    .setHeight(m_colorAttachmentHeight)
                                    .setType(TextureType::e2DArray)
                                    .setUsage(TextureUsage_ColorAttachment | TextureUsage_Sampled)
                                    .setArrayLayers(2)
                                    .setAutomaticBarriers(true));
        for (uint32_t j = 0; j < 2; j++)
        {
            m_drawTextureTask[j].init({ .device = m_device.Get(),
                                        .texture = &m_multiViewTexture,
                                        .posData = {},
                                        .numVerts = {},
                                        .indicesData = {},
                                        .numIndices = {},
                                        .uvData = {},
                                        .samplerDesc = {},
                                        .colorAttachmentFormat = m_colorAttachmentFormat,
                                        .depthStencilFormat = m_depthStencilAttachmentFormat,
                                        .subresource = { .mipSlice = {}, .arraySlice = uint16_t(j), .planeSlice = {} } });
        }
        beginTransfer();
        for (uint32_t j = 0; j < 2; j++)
        {
            m_drawTextureTask[j].uploadStagingBuffers(m_copyCommandList.Get());
        }
        endTransfer();
        return ResultCode::eSuccess;
    }

    void beginMultiViewRenderPass(qhwi::CommandList* cmdList)
    {
        uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
        ColorAttachmentDescription colorAttachmentDesc = {
            .textureData = TextureAttachmentData().setTexture(m_multiViewTexture.get()).setSubresourceRange(EntireResource),
            .resolveTarget = {},
            .loadOp = AttachmentLoadOp::eClear,
            .storeOp = AttachmentStoreOp::eStore,
            .clearValue = { .rgba = m_config.clearColor, .depth = {}, .stencil = {} },
            .initialState = {},
            .finalState = {},
        };
        MultiViewState multiViewState = { .numViews = 2 };
        cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr, &multiViewState);
        cmdList->setViewportAndScissor({ .w = float(w), .h = float(h) });
    }

    void recordCommandList(CommandList* commandList) override
    {
        uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
        float w2 = float(w) / 2.0f;
        float h2 = float(h) / 2.0f;
        commandList->begin();
        // render to each layer of the multiview texture
        beginMultiViewRenderPass(commandList);
        commandList->bindGraphicsPipeline(m_pipeline.Get());
        commandList->draw(VertexCount);
        endRenderPass(commandList);
        // render the multiview texture
        beginRenderPass(commandList);
        commandList->setViewportAndScissor({ .w = w2, .h = h2 });
        m_drawTextureTask[0].draw(commandList);
        commandList->setViewportAndScissor({ .x = w2, .w = w2, .h = h2 });
        m_drawTextureTask[1].draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    qre::RefCountPtr<DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_pipeline;
    DrawTextureTask2D m_drawTextureTask[2];
    Texture1 m_multiViewTexture;
    const uint32_t VertexCount = 4;
};

UnitTest* create_test_multiview(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestMultiView(backend, config);
}
