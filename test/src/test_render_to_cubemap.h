//
//
//  test_to_cubemap.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class RenderToCubemapTask : public Task
{
public:
    void init(Device* pDevice);

    inline void uploadStagingBuffers(CommandList* commandList) { m_bPos.uploadFromStagingBuffer(commandList); };

    inline Texture1* getCubeMap() { return &m_cubemap; };

    void draw(CommandList* commandList);

private:
    void initPipeline();

    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayout;

    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descriptorSet;

    DescriptorTextureInfo m_textureInfo;

    uint32_t m_numVerts;
    Buffer1 m_bPos;
    Texture1 m_cubemap;
    Device* m_device;
};

class TestRenderToCubemap : public UnitTest
{
public:
    TestRenderToCubemap(GraphicsBackend graphicsBackend, Config config, const char* testName, Format format, const void* srcData);

protected:
    ResultCode init() override;
    void recordCommandList(CommandList* commandList) override;

    RenderToCubemapTask m_renderToCubemapTask;
    SampleTextureCubeTask m_drawTextureTask;
    const void* m_pSrcData;
    Format m_format;
    bool m_renderedToCubemap = false;
};
