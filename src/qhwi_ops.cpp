//
//
//  qhwi_ops.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_ops.h>
#include <qhwi_shaders.h>
#include <qhwi/qhwi_util.h>
#include <qre/memory/align.h>

qre::RefCountPtr<qhwi::OpticalFlowOp> qhwi::createOpticalFlowOp(Device* device, const OpticalFlowOpDesc& desc)
{
    if (device->supportsOpticalFlowNV())
    {
#ifdef QHWI_BACKEND_VULKAN
        return qre::MakeRefCountPtr<OpticalFlowOp>(new VkNvOpticalFlowOp(device, desc));
#endif
    }
    return nullptr;
}

qhwi::OpticalFlowOp* qhwi::OpticalFlowOp::setInput(qhwi::Texture* texture)
{
    m_input = texture;
    return this;
}

qhwi::OpticalFlowOp* qhwi::OpticalFlowOp::setReference(qhwi::Texture* texture)
{
    m_ref = texture;
    return this;
}

qhwi::OpticalFlowOp* qhwi::OpticalFlowOp::setOutput(Texture* texture)
{
    m_output = texture;
    return this;
}

#ifdef QHWI_BACKEND_VULKAN
qhwi::VkNvOpticalFlowOp::VkNvOpticalFlowOp(qhwi::Device* device, const Description& desc)
    : OpticalFlowOp(device)
{
    auto vkDevice = static_cast<qhwi::VkDevice*>(device);
    vk::OpticalFlowSessionCreateInfoNV opticalFlowSessionCreateInfo = vk::OpticalFlowSessionCreateInfoNV()
                                                                          .setWidth(desc.w)
                                                                          .setHeight(desc.h)
                                                                          .setImageFormat(vk(desc.imageFormat))
                                                                          .setFlowVectorFormat(vk(desc.flowVectorFormat))
                                                                          .setOutputGridSize(nv(desc.flowGridSize))
                                                                          .setPerformanceLevel(nv(desc.performanceLevel));
    m_opticalFlowSession = vkDevice->get().createOpticalFlowSessionNVUnique(opticalFlowSessionCreateInfo);
    m_opticalFlowExecuteInfo = vk::OpticalFlowExecuteInfoNV({}, 0, nullptr);
}

void qhwi::VkNvOpticalFlowOp::run(CommandList* cmdList)
{
    QHWI_ASSERT(m_input);
    QHWI_ASSERT(m_output);
    QHWI_ASSERT(m_ref);
    auto vkDevice = static_cast<qhwi::VkDevice*>(m_device)->get();
    auto vkCommandList = static_cast<qhwi::VkCommandList*>(cmdList);
    auto vkInput = static_cast<qhwi::VkTexture*>(m_input);
    auto vkOutput = static_cast<qhwi::VkTexture*>(m_output);
    auto vkRef = static_cast<qhwi::VkTexture*>(m_ref);

    vk::ImageView inputView = vkInput->getImageView(vk::ImageViewType::e2D), refView = vkRef->getImageView(vk::ImageViewType::e2D),
                  outputView = vkOutput->getImageView(vk::ImageViewType::e2D);
    vkDevice.bindOpticalFlowSessionImageNV(m_opticalFlowSession.get(), vk::OpticalFlowSessionBindingPointNV::eInput, inputView, vk::ImageLayout::eGeneral);
    vkDevice.bindOpticalFlowSessionImageNV(m_opticalFlowSession.get(), vk::OpticalFlowSessionBindingPointNV::eReference, refView, vk::ImageLayout::eGeneral);
    vkDevice.bindOpticalFlowSessionImageNV(m_opticalFlowSession.get(), vk::OpticalFlowSessionBindingPointNV::eFlowVector, outputView, vk::ImageLayout::eGeneral);
    vkCommandList->get().opticalFlowExecuteNV(m_opticalFlowSession.get(), m_opticalFlowExecuteInfo);
}

vk::OpticalFlowGridSizeFlagBitsNV qhwi::VkNvOpticalFlowOp::nv(qhwi::VkNvOpticalFlowOp::FlowGridSize s)
{
    switch (s)
    {
    default:
    case FlowGridSize::eUnknown:
        return vk::OpticalFlowGridSizeFlagBitsNV::eUnknown;
    case FlowGridSize::e1x1:
        return vk::OpticalFlowGridSizeFlagBitsNV::e1X1;
    case FlowGridSize::e2x2:
        return vk::OpticalFlowGridSizeFlagBitsNV::e2X2;
    case FlowGridSize::e4x4:
        return vk::OpticalFlowGridSizeFlagBitsNV::e4X4;
    }
}

vk::OpticalFlowPerformanceLevelNV qhwi::VkNvOpticalFlowOp::nv(qhwi::VkNvOpticalFlowOp::PerformanceLevel s)
{
    switch (s)
    {
    default:
    case PerformanceLevel::eUnknown:
        return vk::OpticalFlowPerformanceLevelNV::eUnknown;
    case PerformanceLevel::eSlow:
        return vk::OpticalFlowPerformanceLevelNV::eSlow;
    case PerformanceLevel::eMedium:
        return vk::OpticalFlowPerformanceLevelNV::eMedium;
    case PerformanceLevel::eFast:
        return vk::OpticalFlowPerformanceLevelNV::eFast;
    }
}
#endif

qhwi::GenerateMipmapsOp::GenerateMipmapsOp(qhwi::Device* device)
    : Op(device)
{
    QHWI_ASSERT(device != nullptr);

    const uint32_t* shaderBlob = nullptr;
    size_t blobSize = 0;

    for (uint32_t shaderType = 0; shaderType < MipmapShaderType_Count; ++shaderType)
    {
        switch (device->getGraphicsBackend())
        {
        case GraphicsBackend::eDirectX12:
            shaderBlob = MipmapShaderDXIL[shaderType].shaderBlob;
            blobSize = MipmapShaderDXIL[shaderType].blobSize;
            break;

        case GraphicsBackend::eVulkan:
            shaderBlob = MipmapShaderSPIRV[shaderType].shaderBlob;
            blobSize = MipmapShaderSPIRV[shaderType].blobSize;
            break;

        default:
            QHWI_TODO();
        }

        m_shader[shaderType] = m_device->createShader({ .name = shaderType < MipmapShaderType_MultiPass_Float1 ? "mipmapShader_singlePass" : "MipmapShaderType_MultiPass",
                                                        .entryPointName = "CSMain",
                                                        .shaderType = ShaderType::eCompute },
                                                      shaderBlob,
                                                      blobSize);
    }

    m_atomicCounter = device->createBuffer({ .usage = BufferUsage_UnorderedAccess, .size = sizeof(uint32_t), .name = "atomicCounter", .automaticBarriers = true });

    // Create descriptor set and layout for textures - register space 0
    {
        m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Textures] =
            m_device->createDescriptorSetLayout(DescriptorSetLayoutDesc()
                                                    .setRegisterSpace(MipmapRegisterSpace_Textures)
                                                    .addRWTextures(0, ShaderStage_Compute, s_maxArrayLayers * s_maxMipLevels, qhwi::DescriptorBindingFlag_VariableDescriptorCount));

        m_generateMipmapDescriptorSet[MipmapRegisterSpace_Textures] = m_device->createDescriptorSet(
            DescriptorSetDesc().setLayout(m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Textures]).setVariableDescriptorCount(s_maxArrayLayers * s_maxMipLevels));
    }

    // Create descriptor set and layout - register space 1
    {
        m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Buffers] =
            m_device->createDescriptorSetLayout(DescriptorSetLayoutDesc().setRegisterSpace(MipmapRegisterSpace_Buffers).addRWStructuredBuffer(0, ShaderStage_Compute));

        m_generateMipmapDescriptorSet[MipmapRegisterSpace_Buffers] = m_device->createDescriptorSet(
            DescriptorSetDesc().setLayout(m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Buffers]).addRWStructuredBuffer(0, m_atomicCounter, sizeof(uint32_t)));
    }

    m_generateMipmapPipelineLayout = m_device->createPipelineLayout(PipelineLayoutDesc()
                                                                        .setName("mipmapPipelineLayout")
                                                                        .addDescriptorSetLayout(m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Textures].Get())
                                                                        .addDescriptorSetLayout(m_generateMipmapDescriptorSetLayout[MipmapRegisterSpace_Buffers].Get())
                                                                        .addPushConstantRange({ .shaderStages = ShaderStage_Compute, .size = sizeof(MipmapConfig) }));

    for (uint32_t shaderType = 0; shaderType < MipmapShaderType_Count; ++shaderType)
    {
        m_generateMipmapPipeline[shaderType] = m_device->createComputePipeline(
            ComputePipelineDesc().setName("mipmapComputePipeline").setLayout(m_generateMipmapPipelineLayout.Get()).setComputeShader(m_shader[shaderType]));
    }
}

qhwi::GenerateMipmapsOp* qhwi::GenerateMipmapsOp::setTexture(Texture* texture, MipmapTextureUsage usage)
{
    QHWI_ASSERT(texture != nullptr);
    m_texture = texture;
    m_textureUsage = usage;

    const auto& textureDesc = texture->desc();

    QHWI_ASSERT((textureDesc->arrayLayers == MAX_ARRAY_LAYERS && (textureDesc->w == textureDesc->h)) || textureDesc->arrayLayers == 1);

    const unsigned int numChannels = qhwi::FormatUtil::getNumChannels(textureDesc->format);
    QHWI_ASSERT(numChannels != 0);

    constexpr uint8_t firstSinglePassShaderType = MipmapShaderType::MipmapShaderType_SinglePass_Float1;
    constexpr uint8_t firstMultipassShaderType = MipmapShaderType::MipmapShaderType_MultiPass_Float1;

    m_mipmapShaderType = static_cast<MipmapShaderType>((isSinglePassOptimizationPossible() ? firstSinglePassShaderType : firstMultipassShaderType) + numChannels - 1);

    {
        // Write texture mip level subresource info
        std::vector<qhwi::DescriptorTextureInfo> textureInfos(s_maxMipLevels * s_maxArrayLayers);
        for (uint32_t arrayLayer = 0; arrayLayer < s_maxArrayLayers; ++arrayLayer)
        {
            // Binding mip level zero for binding index = [textureDesc->mipLevels, MAX_MIP_LEVELS)
            // spirv-cross doesn't have support for unbounded/variable sized texture arrays, hence binding mip level
            // zero for the rest of the descriptors
            for (uint32_t mipLevel = 0; mipLevel < s_maxMipLevels; ++mipLevel)
            {
                textureInfos[arrayLayer * s_maxMipLevels + mipLevel] = { .texture = texture,
                                                                         .subresourceRange = SubresourceRange(
                                                                             { .mipSlice = (mipLevel < textureDesc->mipLevels) ? uint16_t(mipLevel) : uint16_t(0),
                                                                               .arraySlice = (arrayLayer < textureDesc->arrayLayers) ? uint16_t(arrayLayer) : uint16_t(0) }),
                                                                         .imageViewType = qhwi::TextureType::e2D };
            }
        }

        m_device->updateDescriptorSet(m_generateMipmapDescriptorSet[MipmapRegisterSpace_Textures],
                                      textureInfos.data(),
                                      DescriptorType::eRWTexture,
                                      0,
                                      0,
                                      static_cast<uint32_t>(textureInfos.size()));
    }

    return this;
}

bool qhwi::GenerateMipmapsOp::isSinglePassOptimizationPossible() const
{
    const auto& textureDesc = m_texture->desc();
    QHWI_ASSERT((textureDesc->arrayLayers == MAX_ARRAY_LAYERS && (textureDesc->w == textureDesc->h)) || textureDesc->arrayLayers == 1);

    const bool isHeightPowerOfTwo = (textureDesc->h != 0) && ((textureDesc->h & (textureDesc->h - 1)) == 0);
    const bool isWidthPowerOfTwo = (textureDesc->w != 0) && ((textureDesc->w & (textureDesc->w - 1)) == 0);
    const bool isDefaultKernel = m_kernelSize.x == 2 && m_kernelSize.y == 2;
    const bool isFilterSupported = m_mipmapFilter == MipmapFilter_Linear;
    return isHeightPowerOfTwo && isWidthPowerOfTwo && isDefaultKernel && isFilterSupported;
}

void qhwi::GenerateMipmapsOp::runMultiPass(CommandList* commandList)
{
    auto textureDesc = m_texture->desc();
    QHWI_ASSERT(textureDesc->mipLevels >= 1);
    commandList->beginComputePass();

    uint32_t mipLevels = textureDesc->mipLevels;
    uint32_t width = textureDesc->w;
    uint32_t height = textureDesc->h;

    const uint32_t numThreadsPerThreadgroup[3] = { s_threadGroupSize[0], s_threadGroupSize[1], 1 };
    commandList->bindComputePipeline(m_generateMipmapPipeline[MipmapShaderType_MultiPass_Float4]);
    commandList->bindComputeDescriptorSet(m_generateMipmapDescriptorSet[MipmapRegisterSpace_Textures]);
    commandList->bindComputeDescriptorSet(m_generateMipmapDescriptorSet[MipmapRegisterSpace_Buffers]); // Unused for multi-pass

    for (uint32_t i = 0; i < mipLevels - 1; ++i)
    {
        uint2 srcSize{ std::max(width >> (i), 1u), std::max(height >> (i), 1u) };
        uint2 dstSize{ std::max(width >> (i + 1), 1u), std::max(height >> (i + 1), 1u) };

        MipmapConfig pushConstant = { .mipmapTextureUsage = uint32_t(m_textureUsage),
                                      .dstMipLevel = i + 1,
                                      .numLayers = textureDesc->arrayLayers,
                                      .kernelSize = m_kernelSize,
                                      .srcSize = srcSize,
                                      .dstSize = dstSize,
                                      .mipmapFilter = uint32_t(m_mipmapFilter) };
        commandList->pushComputeConstants(&pushConstant, 0);

        uint32_t numThreadGroups[3] = { qre::AlignUp(dstSize.x, numThreadsPerThreadgroup[0]) / numThreadsPerThreadgroup[0],
                                        qre::AlignUp(dstSize.y, numThreadsPerThreadgroup[1]) / numThreadsPerThreadgroup[1],
                                        1 };

        commandList->dispatch(numThreadGroups, numThreadsPerThreadgroup);
    }

    commandList->endComputePass();
}

void qhwi::GenerateMipmapsOp::runSinglePass(CommandList* commandList)
{
    auto textureDesc = m_texture->desc();
    QHWI_ASSERT(textureDesc->mipLevels >= 1);
    commandList->beginComputePass();

    const uint32_t numThreadsPerThreadgroup[3] = { s_threadGroupSize[0], s_threadGroupSize[1], 1 };
    uint32_t numThreadGroups[3] = { qre::AlignUp(std::max(1u, textureDesc->w >> 2), numThreadsPerThreadgroup[0]) / numThreadsPerThreadgroup[0],
                                    qre::AlignUp(std::max(1u, textureDesc->h >> 2), numThreadsPerThreadgroup[1]) / numThreadsPerThreadgroup[1],
                                    1 };

    commandList->bindComputePipeline(m_generateMipmapPipeline[MipmapShaderType_SinglePass_Float4]);
    commandList->bindComputeDescriptorSet(m_generateMipmapDescriptorSet[MipmapRegisterSpace_Textures]);
    commandList->bindComputeDescriptorSet(m_generateMipmapDescriptorSet[MipmapRegisterSpace_Buffers]);

    MipmapConfig pushConstant = {};
    pushConstant.mipmapTextureUsage = uint32_t(m_textureUsage);
    pushConstant.mipmapFilter = uint32_t(MipmapFilter_Linear);
    pushConstant.targetCounterValue = numThreadGroups[0] * numThreadGroups[1] * numThreadGroups[2] - 1;
    pushConstant.numLayers = textureDesc->arrayLayers;

    commandList->pushComputeConstants(&pushConstant);
    commandList->dispatch(numThreadGroups, numThreadsPerThreadgroup);

    commandList->endComputePass();
}

void qhwi::GenerateMipmapsOp::run(CommandList* commandList)
{
    QHWI_ASSERT(commandList != nullptr);
    uint32_t atomicCounter = {};
    commandList->beginTransferPass();
    commandList->updateBuffer(m_atomicCounter, 0, &atomicCounter, sizeof(uint32_t));
    commandList->endTransferPass();

    (m_mipmapShaderType < MipmapShaderType_MultiPass_Float1) ? runSinglePass(commandList) : runMultiPass(commandList);
}
