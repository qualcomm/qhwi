//
//
//  qhwi_ops.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_OPS_H__
#define __QHWI_OPS_H__

#include <qhwi/qhwi.h>
#include <qhwi/qhwi_enums.h>
#ifdef QHWI_BACKEND_VULKAN
#    include <qhwi/qhwi_vk.h>
#endif
namespace qhwi
{
#include <generateMipmaps_cs.hlsli>
}

namespace qhwi
{
class Device;

class Op : public qre::RefCounterDelete
{
public:
    Op(Device* device)
        : m_device(device)
    {
    }

    virtual ~Op() {}

    virtual void run(CommandList* cmdList) = 0;

protected:
    Device* m_device;
};

class OpticalFlowOp : public Op
{
public:
    enum class FlowGridSize
    {
        eUnknown,
        e1x1,
        e2x2,
        e4x4
    };
    enum class PerformanceLevel
    {
        eUnknown,
        eSlow,
        eMedium,
        eFast
    };

    struct Description
    {
        uint32_t w, h;
        Format imageFormat;
        Format flowVectorFormat;
        FlowGridSize flowGridSize;
        PerformanceLevel performanceLevel;
    };

    OpticalFlowOp(Device* device)
        : Op(device)
    {
    }

    OpticalFlowOp* setInput(Texture* texture);
    OpticalFlowOp* setReference(Texture* texture);
    OpticalFlowOp* setOutput(Texture* texture);

protected:
    Texture* m_input = nullptr;
    Texture* m_ref = nullptr;
    Texture* m_output = nullptr;
};

typedef OpticalFlowOp::Description OpticalFlowOpDesc;

#ifdef QHWI_BACKEND_VULKAN
class VkNvOpticalFlowOp : public OpticalFlowOp
{
public:
    VkNvOpticalFlowOp(Device* device, const OpticalFlowOpDesc& desc);
    void run(CommandList* cmdList) override;

private:
    vk::OpticalFlowGridSizeFlagBitsNV nv(FlowGridSize s);
    vk::OpticalFlowPerformanceLevelNV nv(PerformanceLevel s);
    vk::UniqueOpticalFlowSessionNV m_opticalFlowSession;
    vk::OpticalFlowExecuteInfoNV m_opticalFlowExecuteInfo;
};
#endif

class GenerateMipmapsOp : public Op
{
public:
    GenerateMipmapsOp(Device* device);
    void run(CommandList* commandList) override;
    GenerateMipmapsOp* setTexture(Texture* texture, MipmapTextureUsage usage = MipmapTextureUsage_Generic);

    void setKernelSize(const unsigned int w, const unsigned int h) { m_kernelSize = { w, h }; }

    void setMipmapFilter(const unsigned int filter) { m_mipmapFilter = static_cast<MipmapFilter>(filter); }

private:
    enum MipmapShaderType : uint8_t
    {
        MipmapShaderType_SinglePass_Float1,
        MipmapShaderType_SinglePass_Float2,
        MipmapShaderType_SinglePass_Float3,
        MipmapShaderType_SinglePass_Float4,
        MipmapShaderType_MultiPass_Float1,
        MipmapShaderType_MultiPass_Float2,
        MipmapShaderType_MultiPass_Float3,
        MipmapShaderType_MultiPass_Float4,
        MipmapShaderType_Count
    };

    void runMultiPass(CommandList* commandList);
    void runSinglePass(CommandList* commandList);

    bool isSinglePassOptimizationPossible() const;

    Texture* m_texture = nullptr;
    MipmapTextureUsage m_textureUsage{};
    MipmapFilter m_mipmapFilter = MipmapFilter_Linear;
    MipmapShaderType m_mipmapShaderType{};
    uint2 m_kernelSize{ 2, 2 };
    qre::RefCountPtr<Buffer> m_atomicCounter;
    qre::RefCountPtr<Shader> m_shader[MipmapShaderType_Count];
    qre::RefCountPtr<ComputePipeline> m_generateMipmapPipeline[MipmapShaderType_Count];
    qre::RefCountPtr<PipelineLayout> m_generateMipmapPipelineLayout;
    qre::RefCountPtr<DescriptorSet> m_generateMipmapDescriptorSet[MipmapRegisterSpace_Count];
    qre::RefCountPtr<DescriptorSetLayout> m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Count];
    static constexpr uint32_t s_threadGroupSize[MipmapShaderType_Count] = { 8, 8 };
    static constexpr uint32_t s_maxArrayLayers = MAX_ARRAY_LAYERS;
    static constexpr uint32_t s_maxMipLevels = MAX_MIP_LEVELS;
};

extern qre::RefCountPtr<OpticalFlowOp> createOpticalFlowOp(Device* device, const OpticalFlowOpDesc& desc);
}; // namespace qhwi
#endif
