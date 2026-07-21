//
//
//  test_multisample.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestMultisample : public UnitTest
{
public:
    TestMultisample(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_multisample")
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
        initPipelines(m_colorAttachmentFormat, m_depthStencilAttachmentFormat);

        m_msaaTexture = m_device->createTexture(TextureDesc()
                                                    .setWidth(m_colorAttachmentWidth)
                                                    .setHeight(m_colorAttachmentHeight)
                                                    .setFormat(m_colorAttachmentFormat)
                                                    .setNumSamples(sampleCount)
                                                    .setUsage(TextureUsage_ColorAttachment | TextureUsage_TransientAttachment)
                                                    .setName("MSAA texture")
                                                    .setAutomaticBarriers(true));

        return ResultCode::eSuccess;
    }

    void initPipelines(Format colorAttachmentFormat, Format depthStencilFormat)
    {
        m_graphicsPipelineLayout = m_device->createPipelineLayout({});

        Shader1 vs, ps;
        initVertexShader(m_device.Get(), "testUtilDrawTriangle", vs);
        initPixelShader(m_device.Get(), "testUtilDrawTriangle", ps);
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
            .multisampleState =
            {
                .sampleCount = sampleCount,
                .enableAlphaToCoverage = {},
            },
            .depthStencilState = {},
            .vertexInputState = {},
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
            .name = {},
        });
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();

        uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
        ColorAttachmentDescription colorAttachmentDesc = { .textureData = {},
                                                           .resolveTarget = {},
                                                           .loadOp = AttachmentLoadOp::eClear,
                                                           .storeOp = AttachmentStoreOp::eDontCare,
                                                           .clearValue = { .rgba = m_config.clearColor, .depth = {}, .stencil = {} },
                                                           .initialState = {},
                                                           .finalState = {} };
        colorAttachmentDesc.setTexture(m_msaaTexture);
        colorAttachmentDesc.initialState = m_colorCurrentState;
        colorAttachmentDesc.finalState = TextureState_ColorAttachment;
        m_colorCurrentState = colorAttachmentDesc.finalState;

        auto resolveTexture = (m_config.onscreen) ? m_swapchain->getCurrentImage() : m_offscreenSurface.get();

        colorAttachmentDesc.setResolveTexture(resolveTexture);

        DepthStencilAttachmentDescription depthStencilAttachmentDesc = { .textureData = {},
                                                                         .resolveTarget = {},
                                                                         .depthLoadOp = AttachmentLoadOp::eClear,
                                                                         .depthStoreOp = AttachmentStoreOp::eDontCare,
                                                                         .stencilLoadOp = AttachmentLoadOp::eClear,
                                                                         .stencilStoreOp = AttachmentStoreOp::eDontCare,
                                                                         .clearValue = { .rgba = {}, .depth = 1.0f, .stencil = 0 },
                                                                         .depthResolveMode = {},
                                                                         .stencilResolveMode = {},
                                                                         .initialState = {},
                                                                         .finalState = {} };

        bool hasDepthStencilAttachment = (m_config.depthStencilType != DepthStencilType::eNone);
        if (hasDepthStencilAttachment)
        {
            depthStencilAttachmentDesc.textureData.texture = m_depthStencilImage;
        }

        commandList->beginRenderPass(1, &colorAttachmentDesc, hasDepthStencilAttachment ? &depthStencilAttachmentDesc : nullptr);
        commandList->setViewport({ .w = float(w), .h = float(h) });
        commandList->setScissor({ .w = w, .h = h });

        commandList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        commandList->draw(3);

        commandList->endRenderPass();

        commandList->end();
    }

    qre::RefCountPtr<Texture> m_msaaTexture;
    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    qhwi::TextureState m_colorCurrentState;
    static constexpr SampleCount sampleCount = SampleCount::e4;
};

UnitTest* create_test_multisample(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestMultisample(backend, config);
}
