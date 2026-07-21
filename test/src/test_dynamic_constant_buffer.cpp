//
//
//  test_dynamic_constant_buffer.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

// This test reads from a constant buffer multiple times within the same frame, using different
// offsets for each draw.  It uses the same descriptor with dynamic offsets instead of
// creating separate descriptors per draw

#include "test_util.h"
#include "unit_test.h"
#include <qre/memory/align.h>
using namespace qhwi;
using namespace test_util;

namespace test_dynamic_constant_buffer
{
class DrawColorTask1 : public DrawColorTask
{
public:
    struct InitParams
    {
        Device* device;
        float2* posData = nullptr;
        uint32_t numVerts = 0;
        float4* colorData = nullptr;
        uint32_t numColors = 0;
        PrimitiveTopology primitiveTopology = PrimitiveTopology::eTriangleStrip;
        Format colorAttachmentFormat;
        Format depthStencilFormat = Format::eUnknown;
    };

    void init(const InitParams& p)
    {
        Task::init(p.device);
        m_primitiveTopology = p.primitiveTopology;
        m_numVerts = p.numVerts;

        float2* posData = p.posData;
        uint32_t numVerts = p.numVerts;

        auto& cbDstElemAlign = m_cbElemAlign;
        cbDstElemAlign = qre::AlignUp(sizeof(float4), m_device->getConstantBufferOffsetAlignment());
        size_t cbDstSize = p.numColors * cbDstElemAlign;
        void* cbDstData = malloc(p.numColors * cbDstElemAlign);
        QHWI_ASSERT(cbDstData);
        uint8_t* cbSrcPtr = (uint8_t*)p.colorData;
        uint8_t* cbDstPtr = (uint8_t*)cbDstData;
        for (uint32_t j = 0; j < p.numColors; j++)
        {
            memcpy(cbDstPtr, cbSrcPtr, sizeof(float4));
            cbSrcPtr += sizeof(float4);
            cbDstPtr += cbDstElemAlign;
        }

        m_cb.initAsConstantBuffer(m_device, "cb", cbDstData, uint32_t(cbDstSize));
        free(cbDstData);
        m_bPos.initAsVertexBuffer(m_device, "pos", posData, numVerts * sizeof(float2));
        initDescriptorSetsAndLayouts();
        initPipeline(p.primitiveTopology, p.colorAttachmentFormat, p.depthStencilFormat);
    }

    size_t getConstantBufferStride() const { return m_cbElemAlign; }

protected:
    void initDescriptorSetsAndLayouts() override
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().setName("drawColorTask1_descSet").addDynamicConstantBuffer(0, m_cb.buffer, 0, sizeof(float4)),
                                               ShaderStage_Vertex,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    size_t m_cbElemAlign = 0;
};

class TestDynamicConstantBuffer : public UnitTest
{
public:
    TestDynamicConstantBuffer(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_dynamic_constant_buffer")
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
        float2 pos[NumVertsPerInstance * NumInstances];
        float2* p = pos;
        float x = 0.0f;
        for (uint32_t j = 0; j < NumInstances; j++)
        {
            *p++ = { x, 0 };
            *p++ = { x, 1 };
            *p++ = { x + .1f, 0 };
            *p++ = { x + .1f, 1 };
            x += .2f;
        };
        float4 color[] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 0, 1 }, { 1, 0, 1, 1 } };
        m_drawColorTask.init({
            .device = m_device.Get(),
            .posData = pos,
            .numVerts = NumVertsPerInstance * NumInstances,
            .colorData = color,
            .numColors = NumInstances,
            .primitiveTopology = PrimitiveTopology::eTriangleStrip,
            .colorAttachmentFormat = m_colorAttachmentFormat,
            .depthStencilFormat = m_depthStencilAttachmentFormat,
        });
        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        commandList->bindGraphicsPipeline(m_drawColorTask.graphicsPipeline());
        uint32_t bPosOffset = 0;
        uint32_t bColorOffset = 0;
        auto descSet = m_drawColorTask.descriptorSet(0);
        size_t bColorStride = m_drawColorTask.getConstantBufferStride();
        for (uint32_t j = 0; j < NumInstances; j++)
        {
            commandList->bindVertexBuffer(m_drawColorTask.bPos().get(), DrawColorTask::B_POS, bPosOffset, sizeof(float2));
            commandList->bindGraphicsDescriptorSet(descSet, 1, &bColorOffset);
            commandList->draw(NumVertsPerInstance);
            bPosOffset += NumVertsPerInstance * sizeof(float2);
            bColorOffset += uint32_t(bColorStride);
        }
        endRenderPass(commandList);
        commandList->end();
    }

    DrawColorTask1 m_drawColorTask;
    static constexpr uint32_t NumInstances = 5;
    static constexpr uint32_t NumVertsPerInstance = 4;
};
} // namespace test_dynamic_constant_buffer

using namespace test_dynamic_constant_buffer;

UnitTest* create_test_dynamic_constant_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDynamicConstantBuffer(backend, config);
}
