//
//
//  test_graphics_pipeline.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestGraphicsPipeline : public UnitTest
{
public:
    TestGraphicsPipeline(GraphicsBackend graphicsBackend, Config config, const char* testName, bool enableConservativeRasterization = false)
        : UnitTest(graphicsBackend, config, testName)
    {
        m_enableConservativeRasterization = enableConservativeRasterization;
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

        return ResultCode::eSuccess;
    }

    void initPipelines(Format colorAttachmentFormat, Format depthStencilFormat)
    {
        m_graphicsPipelineLayout = m_device->createPipelineLayout({});

        Shader1 vs, ps;
        initVertexShader(m_device.Get(), "testUtilDrawTriangle", vs);
        initPixelShader(m_device.Get(), "testUtilDrawTriangle", ps);
        m_graphicsPipeline = m_device->createGraphicsPipeline({
            .vertexShader = vs.shader,
            .pixelShader = ps.shader,
            .domainShader = {},
            .hullShader = {},
            .geometryShader = {},
            .amplificationShader = {},
            .meshShader = {},
            .blendState = {},
            .rasterizerState = { .enableConservativeRasterization = m_enableConservativeRasterization },
            .multisampleState = {},
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
        beginRenderPass(commandList);
        commandList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        commandList->draw(3);
        endRenderPass(commandList);
        commandList->end();
    }

    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    bool m_enableConservativeRasterization;
};

UnitTest* create_test_graphics_pipeline(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestGraphicsPipeline(backend, config, "test_graphics_pipeline");
}

UnitTest* create_test_conservative_rasterization(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestGraphicsPipeline(backend, config, "test_conservative_rasterization", true);
}
