//
//  test_ray_pipeline.h
//  QHWI
//
// Copyright(c) Qualcomm Technologies, Inc. and /or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
#include <qre/memory/StaticVector.h>
using namespace qhwi;
using namespace test_util;

class TestRayPipeline : public UnitTest
{
public:
    TestRayPipeline(GraphicsBackend graphicsBackend, Config config, const char* name, const bool enableIndirectTraceRays = false);

protected:
    ResultCode init() override;
    virtual void initGeometry();
    virtual void initInstanceData();
    virtual void uploadVertexData();
    virtual void initAccelerationStructures();
    virtual void initShaders();
    virtual void initRayPipeline();
    virtual void createShaderBindingTables();
    void recordCommandList(CommandList* commandList) override;

protected:
    enum class RTPipelineShaders
    {
        RayGen,
        Miss,
        ClosestHit,
        Count
    };

    struct ShaderBindingTable
    {
        void init(Device* device, const BufferDesc& desc, void* data, const RayTracingPipelineProperties& properties)
        {
            buffer.init(device, desc, data);
            shaderTable = { .startAddress = buffer.get()->getDeviceAddress(), .stride = properties.shaderGroupHandleSize, .size = desc.size };
        }

        Buffer1 buffer = {};
        StridedDeviceAddressRegion shaderTable = {};
    };

    static constexpr int NumVerts = 3;
    static constexpr int NumIndices = 3;
    static constexpr int NumInstances = 5;

    Buffer1 m_vertexBuffer;
    Buffer1 m_indexBuffer;
    Buffer1 m_indirectBuffer;
    TopLevelDescriptor m_instanceData[NumInstances];
    qre::RefCountPtr<qhwi::Buffer> m_instanceBuffer;
    qre::RefCountPtr<qhwi::AccelerationStructure> m_blas;
    qre::RefCountPtr<qhwi::AccelerationStructure> m_tlas;
    qre::RefCountPtr<qhwi::Buffer> m_tlasScratchBuffer;

    uint32_t m_width;
    uint32_t m_height;

    qre::RefCountPtr<PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<RayTracingPipeline> m_rayTracingPipeline;

    // Shader binding tables
    ShaderBindingTable m_rayGenShaderBindingTable;
    ShaderBindingTable m_missShaderBindingTable;
    ShaderBindingTable m_hitShaderBindingTable;

    TraceRaysIndirectCommand m_unifiedTraceRaysIndirectCommand{};
    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    Texture1 m_outputTexture;
    DrawTextureTask2D m_drawTextureTask;

    // Shaders
    Shader1 shaders1[int(RTPipelineShaders::Count)];
    qre::StaticVector<RayTracingPipelineShaderGroup, int(RTPipelineShaders::Count)> shaderGroups;
    qre::StaticVector<const Shader*, int(RTPipelineShaders::Count)> shaders;

    bool m_enableIndirectTraceRays = false;
};
