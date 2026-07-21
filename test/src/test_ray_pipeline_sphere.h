//
//  test_ray_pipeline_sphere.h
//  QHWI
//
// Copyright(c) Qualcomm Technologies, Inc. and /or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_ray_pipeline.h"
using namespace qhwi;
using namespace test_util;

class TestRayPipelineSphere : public TestRayPipeline
{
public:
    TestRayPipelineSphere(GraphicsBackend graphicsBackend, Config config, const char* name);

protected:
    virtual void initGeometry() override;
    virtual void initInstanceData() override;
    virtual void initAccelerationStructures() override;
    void recordCommandList(CommandList* commandList) override;

protected:
    static constexpr int NumLatitude = 64;  // Number of latitude lines
    static constexpr int NumLongitude = 64; // Number of longitude lines
    static constexpr int NumVerts = (NumLatitude + 1) * (NumLongitude + 1);
    static constexpr int NumTriangles = (NumLatitude * NumLongitude * 2) + 1;
    static constexpr int NumIndices = NumTriangles * 3;
    static constexpr int NumInstances = 1;

    float m_R;
};
