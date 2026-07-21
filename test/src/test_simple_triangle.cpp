//
//
//  test_simple_triangle.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestSimpleTriangle : public UnitTest
{
public:
    TestSimpleTriangle(GraphicsBackend graphicsBackend, Config config, const char* name = "test_simple_triangle")
        : UnitTest(graphicsBackend, config, name)
    {
    }

protected:
    ResultCode init() override
    {
        const ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        // Define triangle vertices in normalized device coordinates
        // Yellow color: (1.0, 1.0, 0.0, 1.0)
        // polygon winding is clock-wise
        DrawColor1Task::VSInput verts[] = {
            { { 0.5f, -0.5f, 0.5f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },  // Bottom-right vertex
            { { -0.5f, -0.5f, 0.5f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } }, // Bottom-left vertex
            { { 0.0f, 0.5f, 0.5f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } }    // Top vertex
        };

        uint16_t indices[] = { 0, 1, 2 };

        // Create identity matrix for modelViewProj
        DrawColor1Task::CBInput cb;
        cb.modelViewProj[0] = float4x4(1.0f);
        cb.modelViewProj[1] = float4x4(1.0f);

        m_drawTriangleTask.init({ .device = m_device.Get(),
                                  .vertsData = verts,
                                  .numVerts = sizeof(verts) / sizeof(verts[0]),
                                  .indicesData = indices,
                                  .numIndices = sizeof(indices) / sizeof(indices[0]),
                                  .cbData = &cb,
                                  .primitiveTopology = PrimitiveTopology::eTriangleList,
                                  .colorAttachmentFormat = m_colorAttachmentFormat,
                                  .depthStencilFormat = m_depthStencilAttachmentFormat,
                                  .multiViewState = { .numViews = 1 } });

        beginTransfer();
        m_drawTriangleTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawTriangleTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DrawColor1Task m_drawTriangleTask;
};

UnitTest* create_test_simple_triangle(qhwi::GraphicsBackend backend, UnitTest::Config config)
{
    return new TestSimpleTriangle(backend, config);
}
