//
//
//  test_vk_specialization_constants.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
#if defined(QHWI_BACKEND_VULKAN)
#    include "qhwi/qhwi_vk.h"
#endif

using namespace test_util;

class TestSpecializationConstants : public UnitTest
{
public:
    TestSpecializationConstants(qhwi::GraphicsBackend graphicsBackend, Config config, const char* name = "test_specialization_constants")
        : UnitTest(graphicsBackend, config, name)
    {
    }

protected:
    qhwi::ResultCode init() override
    {
        if (!supportsSpecializationConstants(m_graphicsBackend))
        {
            return qhwi::ResultCode::eUnsupported;
        }
        qhwi::ResultCode resultCode = UnitTest::init();
        if (resultCode != qhwi::ResultCode::eSuccess)
        {
            return resultCode;
        }
        initPipelines(m_colorAttachmentFormat, m_depthStencilAttachmentFormat);
        initBuffers();

        return qhwi::ResultCode::eSuccess;
    }

    void initPipelines(qhwi::Format colorAttachmentFormat, qhwi::Format depthStencilFormat)
    {
        m_graphicsPipelineLayout = m_device->createPipelineLayout({});

        Shader1 vs, ps;
        initVertexShader(m_device.Get(), "testSpecializationConstants", vs);
        initPixelShader(m_device.Get(), "testSpecializationConstants", ps);
        qhwi::VertexInputState vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                            .semanticIndex = 0,
                                                                                            .binding = 0,
                                                                                            .format = qhwi::Format::eRg32Float,
                                                                                            .offset = 0,
                                                                                            .stride = sizeof(float2),
                                                                                            .inputRate = qhwi::VertexInputRate::eVertex } } };
        auto gpDesc = qhwi::GraphicsPipelineDesc()
                          .setVertexShader(vs.shader)
                          .setPixelShader(ps.shader)
                          .setLayout(m_graphicsPipelineLayout)
                          .setColorAttachmentFormats({ colorAttachmentFormat })
                          .setDepthStencilFormat(depthStencilFormat)
                          .setVertexInputState(vertexInputState)
                          .setPrimitiveTopology(qhwi::PrimitiveTopology::eTriangleStrip);
#if defined(QHWI_BACKEND_VULKAN)
        qhwi::VkGraphicsPipelineDesc vkGpDesc(gpDesc);
        auto vkDevice = static_cast<qhwi::VkDevice*>(m_device.Get());

        vk::SpecializationMapEntry specInfoMapEntry(0, 0, sizeof(uint32_t));

        uint32_t color = 0;
        auto specializationInfo = vk::SpecializationInfo(1, &specInfoMapEntry, sizeof(color), &color);
        vkGpDesc.vk.shaderStages[1].setPSpecializationInfo(&specializationInfo);
        vkGpDesc.init();
        m_graphicsPipeline[0] = vkDevice->createGraphicsPipeline(vkGpDesc);

        color = 1;
        m_graphicsPipeline[1] = vkDevice->createGraphicsPipeline(vkGpDesc);
        vkGpDesc.init();
#endif
    }

    void initBuffers()
    {
        float2 posData0[] = { { 0, 0 }, { 1, 1 }, { 1, 0 } };
        float2 posData1[] = { { 0, 0 }, { 1, 0 }, { 1, -1 } };
        m_bPos[0].initAsVertexBuffer(m_device.Get(), "pos0", posData0, sizeof(posData0));
        m_bPos[1].initAsVertexBuffer(m_device.Get(), "pos1", posData1, sizeof(posData1));
        beginTransfer();
        m_bPos[0].uploadFromStagingBuffer(m_copyCommandList);
        m_bPos[1].uploadFromStagingBuffer(m_copyCommandList);
        endTransfer();
    }

    void recordCommandList(qhwi::CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        commandList->bindGraphicsPipeline(m_graphicsPipeline[0].Get());
        commandList->bindVertexBuffer(m_bPos[0].get(), 0, 0, sizeof(float2));
        commandList->draw(3);
        commandList->bindGraphicsPipeline(m_graphicsPipeline[1].Get());
        commandList->bindVertexBuffer(m_bPos[1].get(), 0, 0, sizeof(float2));
        commandList->draw(3);
        endRenderPass(commandList);
        commandList->end();
    }

    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline[2];
    Buffer1 m_bPos[2];
};

UnitTest* create_test_specialization_constants(qhwi::GraphicsBackend backend, UnitTest::Config config)
{
    return new TestSpecializationConstants(backend, config);
}
