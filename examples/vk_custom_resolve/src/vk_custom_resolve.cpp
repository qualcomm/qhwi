//
//
//  vk_custom_resolve.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <array>

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <qhwi/qhwi_android_window.h>
#endif

#include "vk_custom_resolve.h"
#include <qhwi/qhwi_util.h>
#include <qhwi/qhwi_vk.h>
#include <qre/text/textutil.h>
using namespace test_util;

bool VkCustomResolveExample::usesStandardResolvePath() const
{
    return !m_supportsCustomResolve && !m_supportsQcomShaderResolve;
}

ResultCode VkCustomResolveExample::init(int argc, const char* const* argv)
{
    cxxopts::Options opts("VkCustomResolveExample", "Run VK custom resolve example");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    parse(argc, argv, opts, parseResult, m_config);
    if (m_config.backend != GraphicsBackend::eVulkan)
    {
        QHWI_LOGW("VkCustomResolveExample is Vulkan-only; forcing backend to vk.");
        m_config.backend = GraphicsBackend::eVulkan;
    }

    Example::init("VkCustomResolveExample", &m_config);

    const bool customResolveRequested = m_config.enableCustomResolve;
    m_supportsCustomResolve = customResolveRequested && m_device->supportsCustomResolve();

    {
        auto* vkDevice = static_cast<qhwi::VkDevice*>(m_device.Get());
        m_supportsQcomShaderResolve = !m_supportsCustomResolve && customResolveRequested && vkDevice->isExtensionEnabled(VK_QCOM_RENDER_PASS_SHADER_RESOLVE_EXTENSION_NAME);
        QHWI_LOGI("VkCustomResolveExample: supportsQcomShaderResolve=%d", static_cast<int>(m_supportsQcomShaderResolve));
        if (m_supportsQcomShaderResolve)
        {
            initQcomPath(vkDevice);
        }
    }

    if (customResolveRequested && !m_supportsCustomResolve && !m_supportsQcomShaderResolve)
    {
        QHWI_LOGW("Custom resolve was requested but neither VK_EXT_custom_resolve nor VK_QCOM_render_pass_shader_resolve is available; falling back to standard MSAA resolve.");
    }

    const bool useStandardResolvePath = usesStandardResolvePath();

    // Gradient draw pass - shared by both paths
    Shader1 vs, ps;
    initVertexAndPixelShader(m_device.Get(), "gradient", vs, ps);

    m_gradientPipelineLayout = m_device->create(PipelineLayoutDesc().setName("Gradient Pipeline"));
    GraphicsPipelineDesc gradientPipelineDesc = {
        .vertexShader = vs.shader,
        .pixelShader = ps.shader,
        .layout = m_gradientPipelineLayout,
        .primitiveTopology = PrimitiveTopology::eTriangleList,
        .colorAttachmentFormats = { Format::eRgba8Unorm },
        .customResolve = false,
    };
    gradientPipelineDesc.setName("Gradient Pipeline");
    gradientPipelineDesc.setMultisampleState({ .sampleCount = kSampleCount });
    m_gradientPipeline = m_device->createGraphicsPipeline(gradientPipelineDesc);

    // MSAA texture - when custom resolve is used it also needs InputAttachment for SubpassInputMS.
    TextureUsageMask msaaTextureUsage = TextureUsage_ColorAttachment;
    if (m_supportsCustomResolve)
    {
        msaaTextureUsage |= TextureUsage_InputAttachment;
    }
    m_msaaTexture.init(m_device.Get(),
                       TextureDesc()
                           .setName("vk_custom_resolve_msaa_texture")
                           .setWidth(m_colorAttachmentWidth)
                           .setHeight(m_colorAttachmentHeight)
                           .setType(TextureType::e2D)
                           .setFormat(Format::eRgba8Unorm)
                           .setUsage(msaaTextureUsage)
                           .setNumSamples(kSampleCount)
                           .setAutomaticBarriers(true));

    if (useStandardResolvePath)
    {
        m_resolvedTexture.init(m_device.Get(),
                               TextureDesc()
                                   .setName("vk_custom_resolve_resolved_texture")
                                   .setWidth(m_colorAttachmentWidth)
                                   .setHeight(m_colorAttachmentHeight)
                                   .setType(TextureType::e2D)
                                   .setUsage(TextureUsage_ColorAttachment | TextureUsage_Sampled)
                                   .setAutomaticBarriers(true));

        m_tonemapSampler = m_device->createSampler(SamplerDesc{
            .addressModeU = SamplerAddressMode::eClampToEdge,
            .addressModeV = SamplerAddressMode::eClampToEdge,
        });
    }

    if (m_supportsCustomResolve)
    {
        // Custom resolve pass: reads m_msaaTexture as SubpassInputMS, writes to swapchain image
        Shader1 resolveVs, resolvePs;
        initVertexAndPixelShader(m_device.Get(), "vk_custom_resolve", resolveVs, resolvePs);

        m_customResolveDescSetLayout = m_device->create(DescriptorSetLayoutDesc().addInputAttachment(0, ShaderStage_Pixel).setName("vk_custom_resolve_descSetLayout"));

        m_customResolveDescSet =
            m_device->createDescriptorSet(DescriptorSetDesc(m_customResolveDescSetLayout).addInputAttachment(0, m_msaaTexture.texture).setName("vk_custom_resolve_descSet"));

        m_customResolvePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_customResolveDescSetLayout).setName("vk_custom_resolve_pipelineLayout"));

        auto customResolvePipelineDesc = GraphicsPipelineDesc{
            .vertexShader = resolveVs.shader,
            .pixelShader = resolvePs.shader,
            .layout = m_customResolvePipelineLayout,
            .primitiveTopology = PrimitiveTopology::eTriangleStrip,
            .colorAttachmentFormats = { Format::eRgba8Unorm },
            .customResolve = true,
            .customResolveAttachmentFormats = { m_colorAttachmentFormat },
        };
        customResolvePipelineDesc.setName("Custom Resolve Pipeline");
        customResolvePipelineDesc.setMultisampleState({ .sampleCount = SampleCount::e1 });
        m_customResolvePipeline = m_device->createGraphicsPipeline(customResolvePipelineDesc);
    }

    if (useStandardResolvePath)
    {
        Shader1 tonemapVs, tonemapPs;
        initVertexAndPixelShader(m_device.Get(), "tonemap", tonemapVs, tonemapPs);

        m_tonemapDescSetLayout =
            m_device->create(DescriptorSetLayoutDesc().addTexture(0, ShaderStage_Pixel).addSampler(0, ShaderStage_Pixel).setName("vk_custom_resolve_tonemapDescSetLayout"));

        m_tonemapDescSet = m_device->createDescriptorSet(DescriptorSetDesc(m_tonemapDescSetLayout)
                                                             .addTexture(0, m_resolvedTexture.texture, SubresourceRange(EntireResource), TextureType::e2D)
                                                             .addSampler(0, m_tonemapSampler)
                                                             .setName("vk_custom_resolve_tonemapDescSet"));

        m_tonemapPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_tonemapDescSetLayout).setName("vk_custom_resolve_tonemapPipelineLayout"));

        auto tonemapPipelineDesc = GraphicsPipelineDesc{
            .vertexShader = tonemapVs.shader,
            .pixelShader = tonemapPs.shader,
            .layout = m_tonemapPipelineLayout,
            .primitiveTopology = PrimitiveTopology::eTriangleStrip,
            .colorAttachmentFormats = { m_colorAttachmentFormat },
            .customResolve = false,
        };
        tonemapPipelineDesc.setName("Tonemap Pipeline");
        m_tonemapPipeline = m_device->createGraphicsPipeline(tonemapPipelineDesc);
    }

    return ResultCode::eSuccess;
}

void VkCustomResolveExample::initCmdLineOpts(cxxopts::Options& opts)
{
    Example::initCmdLineOpts(opts);
    opts.add_options()("custom_resolve",
                       "Enable the use of the Custom Resolve extension (falls back to VK_QCOM_render_pass_shader_resolve or standard MSAA resolve if unavailable)",
                       cxxopts::value<bool>()->default_value("false"));
}

void VkCustomResolveExample::parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c)
{
    Example::parse(argc, argv, opts, p, c);
    c.enableCustomResolve = p["custom_resolve"].as<bool>();
}

void VkCustomResolveExample::recordCommandList(CommandList* commandList)
{
    const auto w = static_cast<float>(m_colorAttachmentWidth), h = static_cast<float>(m_colorAttachmentHeight);

    commandList->begin();
    commandList->beginMarker("Gradient Draw Pass");

    if (m_supportsCustomResolve)
    {
        // Pass 1: draw gradient into MSAA (no resolve, just store the samples).
        ColorAttachmentDescription msaaDesc = {
            .textureData = TextureAttachmentData().setTexture(m_msaaTexture.get()).setSubresourceRange(EntireResource),
            .loadOp = AttachmentLoadOp::eClear,
            .storeOp = AttachmentStoreOp::eStore,
        };
        commandList->beginRenderPass(1, &msaaDesc, nullptr);
        commandList->setViewportAndScissor({ .w = w, .h = h });
        commandList->bindGraphicsPipeline(m_gradientPipeline.Get());
        commandList->draw(VertexCount);
        commandList->endRenderPass();

        commandList->endMarker();
        commandList->beginMarker("Display Pass");

        // Pass 2: MSAA as attachment (resolve target = swapchain). VK_RENDERING_CUSTOM_RESOLVE_BIT_EXT
        // is set automatically; QHWI emits the local-read barrier when the input attachment is bound.
        // beginCustomResolve() transitions to the resolve phase; the shader reads MSAA as SubpassInputMS
        // and writes the tonemapped result directly to the swapchain via the custom resolve mechanism.
        ColorAttachmentDescription customResolveDesc = {
            .textureData = TextureAttachmentData().setTexture(m_msaaTexture.get()).setSubresourceRange(EntireResource),
            .loadOp = AttachmentLoadOp::eLoad,
            .storeOp = AttachmentStoreOp::eStore,
        };
        customResolveDesc.setResolveTexture(m_swapchain->getCurrentImage());
        commandList->beginRenderPass(1, &customResolveDesc, nullptr, nullptr, true /* customResolve */);
        commandList->beginCustomResolve();
        commandList->setViewportAndScissor({ .w = w, .h = h });
        commandList->bindGraphicsPipeline(m_customResolvePipeline.Get());
        commandList->bindDescriptorSet(m_customResolveDescSet.Get(), PipelineBindPoint::eGraphics, m_customResolvePipelineLayout.Get());
        commandList->draw(4);
        endRenderPass(commandList);
    }
    else if (m_supportsQcomShaderResolve)
    {
        commandList->endMarker();
        commandList->beginMarker("QCOM Shader Resolve Pass");
        recordQcomPath(commandList);
        // The raw render pass transitions the swapchain image to ePresentSrcKHR via finalLayout.
        // Notify QHWI's tracker without emitting a redundant barrier.
        commandList->notifyState(m_swapchain->getCurrentImage(), TextureState_Present);
    }
    else
    {
        // Standard path: MSAA to average resolve into resolvedTexture via driver.
        ColorAttachmentDescription msaaResolveDesc = {
            .textureData = TextureAttachmentData().setTexture(m_msaaTexture.get()).setSubresourceRange(EntireResource),
            .loadOp = AttachmentLoadOp::eClear,
            .storeOp = AttachmentStoreOp::eStore,
        };
        msaaResolveDesc.setResolveTexture(m_resolvedTexture.get());
        commandList->beginRenderPass(1, &msaaResolveDesc, nullptr);
        commandList->setViewportAndScissor({ .w = w, .h = h });
        commandList->bindGraphicsPipeline(m_gradientPipeline.Get());
        commandList->draw(VertexCount);
        commandList->endRenderPass();

        commandList->endMarker();
        commandList->beginMarker("Display Pass");

        // Display the resolved texture through the tonemap shader.
        ColorAttachmentDescription tonemapOutputDesc = { .loadOp = AttachmentLoadOp::eClear, .storeOp = AttachmentStoreOp::eStore };
        tonemapOutputDesc.setTexture(m_swapchain->getCurrentImage());
        commandList->beginRenderPass(1, &tonemapOutputDesc, nullptr);
        commandList->setViewportAndScissor({ .w = w, .h = h });
        commandList->bindGraphicsPipeline(m_tonemapPipeline.Get());
        commandList->bindDescriptorSet(m_tonemapDescSet.Get(), PipelineBindPoint::eGraphics, m_tonemapPipelineLayout.Get());
        commandList->draw(4);
        endRenderPass(commandList);
    }

    commandList->endMarker();
    commandList->end();
}

// Helper: create a minimal graphics pipeline targeting a specific render pass subpass.
static vk::UniquePipeline createRenderPassPipeline(vk::Device device,
                                                   vk::RenderPass rp,
                                                   uint32_t subpass,
                                                   vk::PipelineLayout layout,
                                                   vk::ShaderModule vsModule,
                                                   vk::ShaderModule psModule,
                                                   vk::SampleCountFlagBits sampleCount)
{
    const std::array<vk::PipelineShaderStageCreateInfo, 2> stages = { {
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vsModule, "VSMain" },
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, psModule, "PSMain" },
    } };
    const vk::PipelineVertexInputStateCreateInfo vi{};
    const vk::PipelineInputAssemblyStateCreateInfo ia{ {}, vk::PrimitiveTopology::eTriangleStrip };
    const vk::PipelineViewportStateCreateInfo vp{ {}, 1, nullptr, 1, nullptr };
    const vk::PipelineRasterizationStateCreateInfo rs =
        vk::PipelineRasterizationStateCreateInfo{}.setLineWidth(1.f).setCullMode(vk::CullModeFlagBits::eNone).setFrontFace(vk::FrontFace::eCounterClockwise);
    const vk::PipelineMultisampleStateCreateInfo ms{ {}, sampleCount };
    const vk::PipelineColorBlendAttachmentState blendAtt = vk::PipelineColorBlendAttachmentState{}.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    const vk::PipelineColorBlendStateCreateInfo cb{ {}, false, vk::LogicOp::eCopy, 1, &blendAtt };
    const std::array<vk::DynamicState, 2> dynStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    const vk::PipelineDynamicStateCreateInfo dyn{ {}, static_cast<uint32_t>(dynStates.size()), dynStates.data() };

    auto [result, pipeline] = device.createGraphicsPipelineUnique({},
                                                                  vk::GraphicsPipelineCreateInfo{}
                                                                      .setStageCount(static_cast<uint32_t>(stages.size()))
                                                                      .setPStages(stages.data())
                                                                      .setPVertexInputState(&vi)
                                                                      .setPInputAssemblyState(&ia)
                                                                      .setPViewportState(&vp)
                                                                      .setPRasterizationState(&rs)
                                                                      .setPMultisampleState(&ms)
                                                                      .setPColorBlendState(&cb)
                                                                      .setPDynamicState(&dyn)
                                                                      .setLayout(layout)
                                                                      .setRenderPass(rp)
                                                                      .setSubpass(subpass));
    if (result != vk::Result::eSuccess)
    {
        QHWI_LOGW("createRenderPassPipeline: vkCreateGraphicsPipelines failed: %s", vk::to_string(result).c_str());
    }
    QHWI_ASSERT(result == vk::Result::eSuccess);
    return std::move(pipeline);
}

void VkCustomResolveExample::initQcomPath(qhwi::VkDevice* vkDev)
{
    vk::Device device = vkDev->get();

    // Separate MSAA texture for QCOM path (no auto-barriers to avoid conflicts with qhwi tracking).
    m_qcomMsaaTexture.init(m_device.Get(),
                           TextureDesc()
                               .setName("vk_custom_resolve_qcom_msaa")
                               .setWidth(m_colorAttachmentWidth)
                               .setHeight(m_colorAttachmentHeight)
                               .setType(TextureType::e2D)
                               .setFormat(Format::eRgba8Unorm)
                               .setUsage(TextureUsage_ColorAttachment | TextureUsage_InputAttachment)
                               .setNumSamples(kSampleCount)
                               .setAutomaticBarriers(false));

    // VkRenderPass - 2 subpasses:
    //   subpass 0: gradient draw to MSAA attachment
    //   subpass 1: VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM, reads MSAA as input to swapchain
    std::array<vk::AttachmentDescription, 2> attachments;
    attachments[0] = vk::AttachmentDescription{} // MSAA
                         .setFormat(vk::Format::eR8G8B8A8Unorm)
                         .setSamples(vk::SampleCountFlagBits::e4)
                         .setLoadOp(vk::AttachmentLoadOp::eClear)
                         .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                         .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                         .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                         .setInitialLayout(vk::ImageLayout::eUndefined)
                         .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    attachments[1] = vk::AttachmentDescription{} // Swapchain / present target
                         .setFormat(vk(m_colorAttachmentFormat))
                         .setSamples(vk::SampleCountFlagBits::e1)
                         .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                         .setStoreOp(vk::AttachmentStoreOp::eStore)
                         .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                         .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                         .setInitialLayout(vk::ImageLayout::eUndefined)
                         .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference subpass0Color = { 0, vk::ImageLayout::eColorAttachmentOptimal };
    vk::AttachmentReference subpass1Input = { 0, vk::ImageLayout::eShaderReadOnlyOptimal };
    vk::AttachmentReference subpass1Color = { 1, vk::ImageLayout::eColorAttachmentOptimal };

    std::array<vk::SubpassDescription, 2> subpasses;
    subpasses[0] = vk::SubpassDescription{}.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics).setColorAttachmentCount(1).setPColorAttachments(&subpass0Color);
    subpasses[1] = vk::SubpassDescription{}
                       .setFlags(static_cast<vk::SubpassDescriptionFlags>(VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM))
                       .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                       .setInputAttachmentCount(1)
                       .setPInputAttachments(&subpass1Input)
                       .setColorAttachmentCount(1)
                       .setPColorAttachments(&subpass1Color);

    // EXTERNAL → subpass 0: order prior frame's MSAA storeOp before this frame's clear/load.
    // EXTERNAL → subpass 1: gate the swapchain write on the acquire semaphore stage
    //   (avoids SYNC-HAZARD-WRITE-AFTER-READ at vkCmdNextSubpass).
    // subpass 0 → subpass 1: MSAA samples written in 0 readable as input attachment in 1.
    std::array<vk::SubpassDependency, 3> deps;
    deps[0] = vk::SubpassDependency{}
                  .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                  .setDstSubpass(0)
                  .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                  .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                  .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                  .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
                  .setDependencyFlags(vk::DependencyFlagBits::eByRegion);
    deps[1] = vk::SubpassDependency{}
                  .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                  .setDstSubpass(1)
                  .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                  .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                  .setSrcAccessMask(vk::AccessFlagBits::eNone)
                  .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                  .setDependencyFlags(vk::DependencyFlagBits::eByRegion);
    deps[2] = vk::SubpassDependency{}
                  .setSrcSubpass(0)
                  .setDstSubpass(1)
                  .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                  .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eColorAttachmentOutput)
                  .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                  .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
                  .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    m_qcomRenderPass = device.createRenderPassUnique(vk::RenderPassCreateInfo{}
                                                         .setAttachmentCount(static_cast<uint32_t>(attachments.size()))
                                                         .setPAttachments(attachments.data())
                                                         .setSubpassCount(static_cast<uint32_t>(subpasses.size()))
                                                         .setPSubpasses(subpasses.data())
                                                         .setDependencyCount(static_cast<uint32_t>(deps.size()))
                                                         .setPDependencies(deps.data()));

    // Gradient pipeline (subpass 0) - load gradient shaders and build a render-pass pipeline.
    Shader1 gradVs, gradPs;
    initVertexAndPixelShader(m_device.Get(), "gradient", gradVs, gradPs);
    auto gradVsMod = static_cast<qhwi::VkShader*>(gradVs.shader.Get())->get();
    auto gradPsMod = static_cast<qhwi::VkShader*>(gradPs.shader.Get())->get();
    m_qcomGradientPipelineLayout = device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{});
    m_qcomGradientPipeline = createRenderPassPipeline(device, m_qcomRenderPass.get(), 0, m_qcomGradientPipelineLayout.get(), gradVsMod, gradPsMod, vk::SampleCountFlagBits::e4);

    // Descriptor set layout and pool for the resolve subpass (input attachment at binding 0).
    vk::DescriptorSetLayoutBinding inputAttBnd = vk::DescriptorSetLayoutBinding{}
                                                     .setBinding(0)
                                                     .setDescriptorType(vk::DescriptorType::eInputAttachment)
                                                     .setDescriptorCount(1)
                                                     .setStageFlags(vk::ShaderStageFlagBits::eFragment);
    m_qcomResolveDescSetLayout = device.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{}.setBindingCount(1).setPBindings(&inputAttBnd));

    vk::DescriptorPoolSize poolSize{ vk::DescriptorType::eInputAttachment, 1 };
    m_qcomDescriptorPool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{}.setMaxSets(1).setPoolSizeCount(1).setPPoolSizes(&poolSize));

    auto descSets = device.allocateDescriptorSets(
        vk::DescriptorSetAllocateInfo{}.setDescriptorPool(m_qcomDescriptorPool.get()).setDescriptorSetCount(1).setPSetLayouts(&m_qcomResolveDescSetLayout.get()));
    m_qcomResolveDescSet = descSets[0];

    // Bind QCOM MSAA texture as the input attachment.
    const auto msaaImageView = static_cast<const qhwi::VkTexture*>(m_qcomMsaaTexture.texture.Get())->getDefaultImageView();
    vk::DescriptorImageInfo imgInfo{ VK_NULL_HANDLE, msaaImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
    device.updateDescriptorSets(vk::WriteDescriptorSet{}
                                    .setDstSet(m_qcomResolveDescSet)
                                    .setDstBinding(0)
                                    .setDescriptorCount(1)
                                    .setDescriptorType(vk::DescriptorType::eInputAttachment)
                                    .setPImageInfo(&imgInfo),
                                {});

    // Resolve pipeline (subpass 1) - reuses vk_custom_resolve shaders (SubpassInputMS + grayscale).
    Shader1 resolveVs, resolvePs;
    initVertexAndPixelShader(m_device.Get(), "vk_custom_resolve", resolveVs, resolvePs);
    auto resolveVsMod = static_cast<qhwi::VkShader*>(resolveVs.shader.Get())->get();
    auto resolvePsMod = static_cast<qhwi::VkShader*>(resolvePs.shader.Get())->get();
    m_qcomResolvePipelineLayout = device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{}.setSetLayoutCount(1).setPSetLayouts(&m_qcomResolveDescSetLayout.get()));
    m_qcomResolvePipeline = createRenderPassPipeline(device, m_qcomRenderPass.get(), 1, m_qcomResolvePipelineLayout.get(), resolveVsMod, resolvePsMod, vk::SampleCountFlagBits::e1);
}

void VkCustomResolveExample::ensureQcomFramebuffer(vk::Device device, vk::ImageView swapView, vk::Image swapImage)
{
    if (m_qcomFramebufferCache.count(swapImage))
    {
        return;
    }
    auto msaaView = static_cast<qhwi::VkTexture*>(m_qcomMsaaTexture.texture.Get())->getDefaultImageView();
    std::array<vk::ImageView, 2> views = { msaaView, swapView };
    m_qcomFramebufferCache[swapImage] = device.createFramebufferUnique(vk::FramebufferCreateInfo{}
                                                                           .setRenderPass(m_qcomRenderPass.get())
                                                                           .setAttachmentCount(static_cast<uint32_t>(views.size()))
                                                                           .setPAttachments(views.data())
                                                                           .setWidth(m_colorAttachmentWidth)
                                                                           .setHeight(m_colorAttachmentHeight)
                                                                           .setLayers(1));
}

void VkCustomResolveExample::recordQcomPath(CommandList* commandList)
{
    auto* vkSwapTex = static_cast<qhwi::VkTexture*>(m_swapchain->getCurrentImage());
    auto device = static_cast<qhwi::VkDevice*>(m_device.Get())->get();
    ensureQcomFramebuffer(device, vkSwapTex->getDefaultImageView(), vkSwapTex->getImage());

    auto* vkCmdList = static_cast<qhwi::VkCommandList*>(commandList);
    const float w = static_cast<float>(m_colorAttachmentWidth);
    const float h = static_cast<float>(m_colorAttachmentHeight);

    constexpr vk::ClearColorValue kBlack{ std::array<float, 4>{ 0.f, 0.f, 0.f, 1.f } };
    const std::array<vk::ClearValue, 2> clearValues = { { kBlack, kBlack } };

    vkCmdList->beginVkRenderPass(vk::RenderPassBeginInfo{}
                                     .setRenderPass(m_qcomRenderPass.get())
                                     .setFramebuffer(m_qcomFramebufferCache.at(vkSwapTex->getImage()).get())
                                     .setRenderArea({ { 0, 0 }, { m_colorAttachmentWidth, m_colorAttachmentHeight } })
                                     .setClearValueCount(static_cast<uint32_t>(clearValues.size()))
                                     .setPClearValues(clearValues.data()));

    const vk::Viewport viewport{ 0.f, 0.f, w, h, 0.f, 1.f };
    const vk::Rect2D scissor{ { 0, 0 }, { m_colorAttachmentWidth, m_colorAttachmentHeight } };
    auto cmd = vkCmdList->get();
    cmd.setViewport(0, viewport);
    cmd.setScissor(0, scissor);

    // Subpass 0: gradient draw (3-vertex triangle, same as other paths).
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_qcomGradientPipeline.get());
    cmd.draw(VertexCount, 1, 0, 0);

    vkCmdList->nextSubpass();

    // Subpass 1: QCOM shader-resolve - reads MSAA via SubpassInputMS, writes grayscale to swapchain.
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_qcomResolvePipeline.get());
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_qcomResolvePipelineLayout.get(), 0, 1, &m_qcomResolveDescSet, 0, nullptr);
    cmd.draw(4, 1, 0, 0);

    vkCmdList->endVkRenderPass();
}

#ifdef QHWI_PLATFORM_ANDROID

enum class AppState
{
    eStart,
    eInit,
    eRun
};

static void onAppCmd(android_app* app, int32_t cmd)
{
    auto* appState = static_cast<AppState*>(app->userData);
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
    {
        ext::SetANativeWindow(app->window);
        *appState = AppState::eInit;
        break;
    }
    }
}

void android_main(android_app* state)
{
    FileUtil::setAndroidAssetManager(state->activity->assetManager);

    int argc;
    char** argv;
    GetCommandLineArgsOp op(state, "vk_custom_resolve", argc, argv);
    op.run();

    AppState appState{};
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    VkCustomResolveExample app;
    while (true)
    {
        int ident = 0;
        int events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollAll(0, nullptr, &events, (void**)&source)) >= 0)
        {
            if (source != nullptr)
            {
                source->process(state, source);
            }
            if (state->destroyRequested != 0)
            {
                return;
            }
        }
        if (appState == AppState::eInit)
        {
            app.init(argc, (char**)argv);
            appState = AppState::eRun;
        }
        else if (appState == AppState::eRun)
        {
            app.runIteration();
        }
    }
}
#else
int main(int argc, char** argv)
{
    qre::OsSetLogConsole(true);
    VkCustomResolveExample app;
    app.init(argc, argv);
    auto window = app.window();
    while (true)
    {
        window->pollEvents();
        if (window->shouldClose())
        {
            break;
        }
        app.runIteration();
    }
    return 0;
}
#endif
