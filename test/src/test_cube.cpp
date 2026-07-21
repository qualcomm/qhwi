//
//
//  test_cube.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
#include <qre/math/vecmath.h>
using namespace qhwi;
using namespace test_util;
using Mat4f = qre::math::Mat4f;
using Affine3f = qre::math::Affine3f;

class TestCube : public UnitTest
{
public:
    TestCube(GraphicsBackend graphicsBackend, Config config, const char* name = "test_cube")
        : UnitTest(graphicsBackend, config, name)
    {
        m_config.depthStencilType = DepthStencilType::eDepthOnly;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        constexpr float toRad = (float(M_PI) / 180.0f);
        float fov = 60.0f * toRad;
        float aspect = float(m_colorAttachmentWidth) / float(m_colorAttachmentHeight);
        Affine3f view = Affine3f::Translation({ 0.0f, 0.0f, 3.0f }) * Affine3f::RotationX(30.0f * toRad) * Affine3f::RotationY(30.0f * toRad);
        Mat4f proj = Mat4f::PerspectiveFov(fov, aspect, 1.0f, 100.0f).first;
        Mat4f m_modelViewProj = proj * view;
        using CBInput = test_util::DrawColor1Task::CBInput;
        CBInput cb;
        memcpy(cb.modelViewProj[0].v, m_modelViewProj.m, sizeof(m_modelViewProj.m));
        m_drawCubeTask.init({ .device = m_device.Get(), .cbData = &cb, .colorAttachmentFormat = m_colorAttachmentFormat, .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_drawCubeTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawCubeTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DrawCubeTask m_drawCubeTask;
};

UnitTest* create_test_cube(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestCube(backend, config);
}
