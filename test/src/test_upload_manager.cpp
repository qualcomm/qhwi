//
//
//  test_upload_manager.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestUploadManagerVertexBuffer : public UnitTest
{
public:
    TestUploadManagerVertexBuffer(GraphicsBackend graphicsBackend, Config config, const char* name = "test_upload_manager_vertex_buffer")
        : UnitTest(graphicsBackend, config, name)
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

        const float4 color = { 1.0f, 0.0f, 0.0f, 1.0f };

        m_drawColorTask.init({
            .device = m_device.Get(),
            .color = color,
            .posData = nullptr,
            .numVerts = NumVerts,
            .primitiveTopology = PrimitiveTopology::eTriangleList,
            .colorAttachmentFormat = m_colorAttachmentFormat,
            .depthStencilFormat = m_depthStencilAttachmentFormat,
        });
        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    inline float2 f0(uint32_t x, uint32_t y) { return { -1.0f + 2.0f * (x / float(NumCols)), -1.0f + 2.0f * (y / float(NumRows)) }; }

    inline void updateBuffer(CommandList* cmdList, Buffer* dst, void* srcData, uint32_t srcSize)
    {
        endRenderPass(cmdList);

        cmdList->beginTransferPass();
        cmdList->updateBuffer(dst, 0, srcData, srcSize);
        cmdList->endTransferPass();

        beginRenderPass(cmdList, AttachmentLoadOp::eLoad);
        cmdList->bindGraphicsPipeline(m_drawColorTask.graphicsPipeline());
        cmdList->bindGraphicsDescriptorSet(m_drawColorTask.descriptorSet(0));
    }

    void recordCommandList(CommandList* cmdList) override
    {
        Buffer1& bPos = m_drawColorTask.bPos();
        cmdList->begin();
        beginRenderPass(cmdList);
        cmdList->bindGraphicsPipeline(m_drawColorTask.graphicsPipeline());
        cmdList->bindGraphicsDescriptorSet(m_drawColorTask.descriptorSet(0));
        for (uint32_t y = 0; y < NumRows; y++)
        {
            for (uint32_t x = 0; x < NumCols; x++)
            {
                float2 pos[] = { f0(x, y), f0(x + 1, y + 1), f0(x + 1, y) };
                updateBuffer(cmdList, bPos.get(), pos, sizeof(pos));
                cmdList->bindVertexBuffer(bPos.get(), DrawColorTask::B_POS, 0, sizeof(float2));
                cmdList->draw(m_drawColorTask.numVerts());
            }
        }
        endRenderPass(cmdList);
        cmdList->end();
    }

    DrawColorTask m_drawColorTask;
    static const uint32_t NumVerts = 3;
    static const uint32_t NumRows = 10, NumCols = 10;
};

class TestUploadManagerConstantBuffer : public UnitTest
{
public:
    TestUploadManagerConstantBuffer(GraphicsBackend graphicsBackend, Config config, const char* name = "test_upload_manager_constant_buffer")
        : UnitTest(graphicsBackend, config, name)
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

        const float4 color = { 0.0f, 1.0f, 0.0f, 1.0f };

        float2 pos[NumRows * NumCols * 3];
        float2* p = pos;
        for (uint32_t y = 0; y < NumRows; y++)
        {
            for (uint32_t x = 0; x < NumCols; x++)
            {
                float2 p0 = f0(x, y), p1 = f0(x + 1, y + 1);
                *p++ = { .5f * (p0.x + p1.x), p0.y };
                *p++ = { p0.x, p1.y };
                *p++ = p1;
            };
        }
        m_drawColorTask.init({
            .device = m_device.Get(),
            .color = color,
            .posData = pos,
            .numVerts = NumVerts,
            .primitiveTopology = PrimitiveTopology::eTriangleList,
            .colorAttachmentFormat = m_colorAttachmentFormat,
            .depthStencilFormat = m_depthStencilAttachmentFormat,
        });
        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    inline float2 f0(uint32_t x, uint32_t y) { return { -1.0f + 2.0f * (x / float(NumCols)), -1.0f + 2.0f * (y / float(NumRows)) }; }

    inline void updateBuffer(CommandList* cmdList, Buffer* dst, void* srcData, uint32_t srcSize)
    {
        endRenderPass(cmdList);

        cmdList->beginTransferPass();
        cmdList->updateBuffer(dst, 0, srcData, srcSize);
        cmdList->endTransferPass();

        beginRenderPass(cmdList, AttachmentLoadOp::eLoad);
        cmdList->bindGraphicsPipeline(m_drawColorTask.graphicsPipeline());
        cmdList->bindVertexBuffer(m_drawColorTask.bPos().get(), DrawColorTask::B_POS, 0, sizeof(float2));
    }

    void recordCommandList(CommandList* cmdList) override
    {
        Buffer1& constantBuffer = m_drawColorTask.bColor();
        cmdList->begin();
        beginRenderPass(cmdList);
        cmdList->bindGraphicsPipeline(m_drawColorTask.graphicsPipeline());
        cmdList->bindVertexBuffer(m_drawColorTask.bPos().get(), DrawColorTask::B_POS, 0, sizeof(float2));
        uint32_t vertexOffset = 0;
        for (uint32_t y = 0; y < NumRows; y++)
        {
            for (uint32_t x = 0; x < NumCols; x++)
            {
                float4 color = Colors[y];
                updateBuffer(cmdList, constantBuffer.buffer.Get(), &color, sizeof(color));
                cmdList->bindDescriptorSet(m_drawColorTask.descriptorSet(0), PipelineBindPoint::eGraphics, cmdList->getCurrentPipelineLayout());
                cmdList->draw(3, 1, vertexOffset);
                vertexOffset += 3;
            }
        }
        endRenderPass(cmdList);
        cmdList->end();
    }

    DrawColorTask m_drawColorTask;
    static const uint32_t NumRows = 10, NumCols = 10;
    static const uint32_t NumVerts = NumRows * NumCols * 3;
    static constexpr float4 Colors[NumRows] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 },   { 0, 0, 1, 1 },   { 0, 1, 1, 1 },   { 1, 0, 1, 1 },
                                                { 1, 1, 0, 1 }, { 0, .5f, 1, 1 }, { 0, 1, .5f, 1 }, { .5f, 0, 1, 1 }, { 1, 0, .5f, 1 } };
};

UnitTest* create_test_upload_manager_vertex_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestUploadManagerVertexBuffer(backend, config);
}

UnitTest* create_test_upload_manager_constant_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestUploadManagerConstantBuffer(backend, config);
}
