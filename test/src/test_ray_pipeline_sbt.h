//
//  test_ray_pipeline_sbt.h
//  QHWI
//
// Copyright(c) Qualcomm Technologies, Inc. and /or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_ray_pipeline_sphere.h"
using namespace qhwi;
using namespace test_util;

class TestRayPipelineSBT : public TestRayPipelineSphere
{
public:
    TestRayPipelineSBT(GraphicsBackend graphicsBackend, Config config, const char* name);

protected:
    void initInstanceData() override;
    void initAccelerationStructures() override;
    void initShaders() override;
    void initRayPipeline() override;
    void createShaderBindingTables() override;
    void recordCommandList(CommandList* commandList) override;

protected:
    enum class RTPipelineShaders
    {
        RayGen,
        Miss,
        ClosestHit,
        ClosestHit_blue,
        Count
    };

    static constexpr int NumInstances = 2;

    // Shaders
    Shader1 shaders1[int(RTPipelineShaders::Count)];
    qre::StaticVector<RayTracingPipelineShaderGroup, int(RTPipelineShaders::Count)> shaderGroups;
    qre::StaticVector<const Shader*, int(RTPipelineShaders::Count)> shaders;
};
