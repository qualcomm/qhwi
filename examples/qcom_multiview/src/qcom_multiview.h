//
//
//  qcom_multiview.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "qhwi_example.h"
// TODO: remove dependency on test_util
#include "test_util.h"

namespace qhwi
{
class QcomMultiViewExample : public Example
{
public:
    ~QcomMultiViewExample() override;
    ResultCode init(int argc, const char* const* argv);

protected:
    std::span<const char* const> pilMetrics() const override
    {
        static constexpr const char* k_extras[] = { "VERTICES_SHADED_PER_SECOND",
                                                    "PRE_CLIPPED_POLYGONS_PER_SECOND",
                                                    "PERCENT_SHADERS_BUSY",
                                                    "PERCENT_SHADER_ALU_CAPACITY_UTILIZED",
                                                    "PERCENT_TIME_SHADING_VERTICES" };
        return k_extras;
    }

private:
    void recordCommandList(CommandList* commandList) override;
    void beginMultiViewRenderPass(qhwi::CommandList* cmdList);
    Config m_config;
    bool m_useQcomMultiview = false;
    qre::RefCountPtr<DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_pipeline;
    test_util::Texture1 m_multiViewTexture;
    test_util::DrawTextureTask2D m_drawTextureTask[2];
    static constexpr uint32_t VertexCount = 4;
    static constexpr uint32_t NumViews = 2;
};
} // namespace qhwi