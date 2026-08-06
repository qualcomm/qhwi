//
//
//  graphics_pipeline_library.cpp
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <chrono>
#include <thread>

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <qhwi/qhwi_android_window.h>
#endif

#include "graphics_pipeline_library.h"
#include <qhwi/qhwi_util.h>
#include <qre/text/textutil.h>

#define SHADER_TYPE "hlsl"

#define V(func)                                                                                                                                                                    \
    {                                                                                                                                                                              \
        vk::Result result = vk::Result(func);                                                                                                                                      \
        if (result != vk::Result::eSuccess)                                                                                                                                        \
        {                                                                                                                                                                          \
            QHWI_ERR("function: %s, result: %s", #func, vk::to_string(result).c_str());                                                                                            \
        }                                                                                                                                                                          \
    }

static uint64_t s_numFramesRendered = 0;

static float s_microSecondsToMilliSeconds = 0.001f;

std::chrono::system_clock::time_point s_renderingStartTime;

using Mat4f = qre::math::Mat4f;
using Affine3f = qre::math::Affine3f;
std::vector<std::pair<int, int>> s_shaderpairs;

char s_shaderName[64];

static char s_testName[256] = "";

static uint32_t s_smallViewportWidth = 32;
static uint32_t s_smallViewportHeight = 32;
static uint32_t s_numBooleanOptions = 2;

qhwi_performance::PLMMetricID s_metricId;

void GraphicsPipelineLibraryExample::determineShaderName(int shaderIndex)
{
    if (m_config.shaderVariation == ShaderVariation::Permutations)
    {
        qre::PrintfSafe(s_shaderName, "permutations%d", shaderIndex);
    }
    else if (m_config.shaderVariation == ShaderVariation::DeadVarying)
    {
        qre::PrintfSafe(s_shaderName, "deadVaryings");
    }
    else if (m_config.shaderVariation == ShaderVariation::HeavyVec4)
    {
        qre::PrintfSafe(s_shaderName, "heavyVec4");
    }
    else if (m_config.shaderVariation == ShaderVariation::LargeUbo)
    {
        qre::PrintfSafe(s_shaderName, "largeUbo");
    }
}

void GraphicsPipelineLibraryExample::pipelineLibraryCreation()
{
    auto t0 = std::chrono::system_clock::now();

    {
        GraphicsPipelineDesc desc;
        desc.vertexInputState = m_vertexInputState;
        desc.layout = m_pipelineLayout;
        desc.primitiveTopology = PrimitiveTopology::eTriangleList;

        VkGraphicsPipeline::Description desc1(desc);
        desc1.vk.graphicsPipelineLibraryCreateInfo.flags = (vk::GraphicsPipelineLibraryFlagsEXT)VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

        if (m_config.retainLinkTimeOpt)
        {
            desc1.vk.createInfo.flags |= (vk::PipelineCreateFlags)VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
        }

        m_vertexInputPipelineLibraries.push_back(qhwi::vk(m_device.Get())->createGraphicsPipeline(desc1));
    }
    for (int j = 0; j < (int)m_config.numPipelines; j++)
    {
        int shaderpairIndex = std::min((int)s_shaderpairs.size() - 1, j);
        {
            Shader1 vs;
            determineShaderName(s_shaderpairs[shaderpairIndex].first);
            initVertexShader(m_device.Get(), s_shaderName, vs);

            GraphicsPipelineDesc desc;
            desc.vertexShader = vs.shader;
            desc.layout = m_pipelineLayout;
            desc.colorAttachmentFormats = { m_colorAttachmentFormat };
            desc.multiViewState.numViews = 0;

            VkGraphicsPipeline::Description desc1(desc);
            desc1.vk.graphicsPipelineLibraryCreateInfo.flags = (vk::GraphicsPipelineLibraryFlagsEXT)VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
            desc1.vk.createInfo.flags = (vk::PipelineCreateFlags)VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
            m_preRasterizationPipelineLibraries.push_back(qhwi::vk(m_device.Get())->createGraphicsPipeline(desc1));
        }
        {
            Shader1 ps;
            determineShaderName(s_shaderpairs[shaderpairIndex].second);
            initPixelShader(m_device.Get(), s_shaderName, ps);

            GraphicsPipelineDesc desc;
            desc.pixelShader = ps.shader;
            desc.vertexInputState = m_vertexInputState;
            desc.layout = m_pipelineLayout;
            desc.colorAttachmentFormats = { m_colorAttachmentFormat };
            desc.multiViewState.numViews = 0;
            VkGraphicsPipeline::Description desc1(desc);
            desc1.vk.graphicsPipelineLibraryCreateInfo.flags = (vk::GraphicsPipelineLibraryFlagsEXT)VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
            desc1.vk.createInfo.flags = (vk::PipelineCreateFlags)VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
            m_fragmentPipelineLibraries.push_back(qhwi::vk(m_device.Get())->createGraphicsPipeline(desc1));
        }
    }
    {
        GraphicsPipelineDesc desc;
        desc.layout = m_pipelineLayout;
        desc.colorAttachmentFormats = { m_colorAttachmentFormat };
        desc.multiViewState.numViews = 0;
        VkGraphicsPipeline::Description desc1(desc);
        desc1.vk.graphicsPipelineLibraryCreateInfo.flags = (vk::GraphicsPipelineLibraryFlagsEXT)VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
        desc1.vk.createInfo.flags = (vk::PipelineCreateFlags)VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
        m_fragmentOutputPipelineLibraries.push_back(qhwi::vk(m_device.Get())->createGraphicsPipeline(desc1));
    }
    auto t1 = std::chrono::system_clock::now();
    m_pipelineLibraryCreationTime = duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

void GraphicsPipelineLibraryExample::pipelinePrep()
{
    {
        DescriptorSetLayoutDesc desc;
        desc.name = "graphcs_pipeline_library_descriptor_set_layout";
        if (m_config.shaderVariation == ShaderVariation::LargeUbo)
        {
            desc.addConstantBuffer(1, ShaderStage_Vertex);
        }
        m_descSetLayout = m_device->create(desc);
    }
    {
        PipelineLayoutDesc desc;
        desc.name = "graphcs_pipeline_library_pipeline_layout";

        VkPushConstantRange pcr = {};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcr.offset = 0;
        pcr.size = 4 * 4 * sizeof(float);
        desc.addPushConstantRange({ .shaderStages = ShaderStage_Vertex, .size = 4 * 4 * sizeof(float) });
        if (m_config.shaderVariation == ShaderVariation::LargeUbo)
        {
            desc.addDescriptorSetLayout(m_descSetLayout);
        }
        m_pipelineLayout = m_device->create(desc);
    }

    VertexInputState
        vis{ .vertexInputAttributeDescriptions = {
                 {
                     .semanticName = "POSITION",
                     .semanticIndex = 0,
                     .binding = 0,
                     .format = Format::eRgba32Float,
                     .offset = 0,
                     .stride = sizeof(VSInput),
                 },
                 { .semanticName = "COLOR", .semanticIndex = 1, .binding = 0, .format = Format::eRgba32Float, .offset = sizeof(float4), .stride = sizeof(VSInput) } } };
    m_vertexInputState = vis;

    RasterizerState rs{ .fillMode = FillMode::eSolid, //FillMode::eWireframe,
                        .cullMode = CullMode::eBack,
                        .frontFaceCounterClockwise = false,
                        .depthBias = 0,
                        .depthBiasClamp = 0.0f,
                        .enableDepthClipping = true,
                        .enableConservativeRasterization = false };
    m_rasterizerState = rs;

    if (m_config.pipelineType == (Config::PipelineType)Config::PipelineType::pipelineLibrary)
    {
        pipelineLibraryCreation();
    }
}

qre::RefCountPtr<qhwi::GraphicsPipeline> GraphicsPipelineLibraryExample::createPipeline(int index)
{
    if (m_config.pipelineType == (Config::PipelineType)Config::PipelineType::monolithic)
    {
        // Monolithic
        Shader1 vs;
        if (m_config.shareShaders)
        {
            // Always use the same shader
            determineShaderName(s_shaderpairs[0].first);
        }
        else
        {
            determineShaderName(s_shaderpairs[index].first);
        }
        initVertexShader(m_device.Get(), s_shaderName, vs);

        Shader1 ps;
        if (m_config.shareShaders)
        {
            // Always use the same shader
            determineShaderName(s_shaderpairs[0].second);
        }
        else
        {
            determineShaderName(s_shaderpairs[index].second);
        }
        initPixelShader(m_device.Get(), s_shaderName, ps);

        GraphicsPipelineDesc desc;
        desc.vertexShader = vs.shader;
        desc.pixelShader = ps.shader;
        desc.vertexInputState = m_vertexInputState;
        desc.primitiveTopology = PrimitiveTopology::eTriangleList;
        desc.layout = m_pipelineLayout;
        desc.colorAttachmentFormats = { m_colorAttachmentFormat };
        desc.multiViewState.numViews = 0;
        desc.rasterizerState = m_rasterizerState;
        VkGraphicsPipeline::Description desc1(desc);
        desc1.vk.enablePipelineCache = m_config.usePipelineCache;

        return qhwi::vk(m_device.Get())->createGraphicsPipeline(desc1);
    }
    else
    {
        // Graphics Pipeline Library
        GraphicsPipelineDesc desc;
        desc.colorAttachmentFormats = { m_colorAttachmentFormat };
        desc.layout = m_pipelineLayout;
        VkGraphicsPipeline::Description desc1(desc);

        desc1.vk.graphicPipelineLibraries.clear();
        desc1.vk.graphicPipelineLibraries.push_back(static_cast<VkGraphicsPipeline*>(m_vertexInputPipelineLibraries[0].Get())->get());
        desc1.vk.graphicPipelineLibraries.push_back(static_cast<VkGraphicsPipeline*>(m_fragmentOutputPipelineLibraries[0].Get())->get());
        if (m_config.shareShaders)
        {
            // Always use the same graphic pipeline libraries
            desc1.vk.graphicPipelineLibraries.push_back(static_cast<VkGraphicsPipeline*>(m_preRasterizationPipelineLibraries[0].Get())->get());
            desc1.vk.graphicPipelineLibraries.push_back(static_cast<VkGraphicsPipeline*>(m_fragmentPipelineLibraries[0].Get())->get());
        }
        else
        {
            desc1.vk.graphicPipelineLibraries.push_back(static_cast<VkGraphicsPipeline*>(m_preRasterizationPipelineLibraries[index].Get())->get());
            desc1.vk.graphicPipelineLibraries.push_back(static_cast<VkGraphicsPipeline*>(m_fragmentPipelineLibraries[index].Get())->get());
        }

        if (m_config.linkTimeOpt)
        {
            desc1.vk.createInfo.flags |= (vk::PipelineCreateFlags)VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
        }
        desc1.vk.enablePipelineCache = m_config.usePipelineCache;
        return qhwi::vk(m_device.Get())->createGraphicsPipeline(desc1);
    }
}

void GraphicsPipelineLibraryExample::createPipelines()
{
    auto t0 = std::chrono::system_clock::now();
    for (uint32_t index = 0; index < m_config.numPipelines; index++)
    {
        m_gfxPipelines.push_back(createPipeline(index));
    }
    auto t1 = std::chrono::system_clock::now();
    m_pipelineCreationTime = duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

static float rotAngle = 0.0f;
static float rotIncrement = 0.02f;

void GraphicsPipelineLibraryExample::updateView()
{
    rotAngle += rotIncrement;

    constexpr float toRad = (float(M_PI) / 180.0f);
    float fov = 60.0f * toRad;
    float aspect = float(m_colorAttachmentWidth) / float(m_colorAttachmentHeight);
    Affine3f view = Affine3f::Translation({ 0.0f, 0.0f, 3.0f }) * Affine3f::RotationX(rotAngle * toRad) * Affine3f::RotationY(rotAngle * 0.40f * toRad);
    Mat4f proj = Mat4f::PerspectiveFov(fov, aspect, 1.0f, 100.0f).first;
    Mat4f m_modelViewProj = proj * view;
    memcpy(m_cb.modelViewProj.v, m_modelViewProj.m, sizeof(m_modelViewProj.m));
}

void GraphicsPipelineLibraryExample::recordCommandList(CommandList* commandList)
{
    // recordCommandList is called once per frame
    if (s_numFramesRendered >= (uint64_t)m_config.numFramesToRender)
    {
        auto t1 = std::chrono::system_clock::now();
        m_renderingTime = duration_cast<std::chrono::milliseconds>(t1 - s_renderingStartTime).count();

        logReport();
        exit(1);
    }

    // Update Cameraview matrix
    updateView();

    commandList->begin();

    // Start GPU timing
    m_plm.LogGPUBeginTimer(s_metricId, commandList);

    beginRenderPass(commandList);

    if (m_config.useSmallViewport)
    {
        commandList->setViewportAndScissor({ .w = (float)s_smallViewportWidth, .h = (float)s_smallViewportHeight });
    }
    else
    {
        commandList->setViewportAndScissor({ .w = (float)m_colorAttachmentWidth, .h = (float)m_colorAttachmentHeight });
    }

    // Determine the pipeline that will be used for this frame
    int pipelineIndex = (int)(s_numFramesRendered / m_config.numFramesPerPipeline) % m_config.numPipelines;

    if (m_config.runTimePipelines)
    {
        // We are creating the pipelines at runtime
        if ((int)m_gfxPipelines.size() <= pipelineIndex)
        {
            m_gfxPipelines.push_back(createPipeline(pipelineIndex));
        }
    }
    commandList->bindGraphicsPipeline(m_gfxPipelines[pipelineIndex].Get());
    commandList->bindVertexBuffer(m_bVerts.get(), B_VERTS, 0, sizeof(VSInput));

    // For the UBO shader, we need to use a descriptor set
    if (m_config.shaderVariation == ShaderVariation::LargeUbo)
    {
        commandList->bindGraphicsDescriptorSet(m_descriptorSetUBO);
    }

    commandList->pushGraphicsConstants(&m_cb, 0);

    commandList->draw(m_numVerts);

    endRenderPass(commandList);

    // End GPU timing and calculate any available timer queries
    m_plm.LogGPUEndTimer();
    m_plm.UpdateLastCompletedSubmissionID(commandList);

    commandList->end();

    s_numFramesRendered++;
}

void GraphicsPipelineLibraryExample::uploadStagingBuffers(CommandList* cmdList)
{
    m_bVerts.uploadFromStagingBuffer(cmdList, 0, m_numVerts * sizeof(VSInput), 0);

    if (m_config.shaderVariation == ShaderVariation::LargeUbo)
    {
        m_constantbufferUBO.uploadFromStagingBuffer(cmdList);
    }
}

void GraphicsPipelineLibraryExample::logReport()
{
    QHWI_LOGI("");
    if (strlen(s_testName) > 0)
    {
        QHWI_LOGI("Test Configuration: %s", s_testName);
    }
    QHWI_LOGI("GPU Property FastLinking: %d", m_fastLinking);
    QHWI_LOGI("GPU Property IndependentInterpolationDecoration: %d", m_independentInterpolationDecoration);
    QHWI_LOGI("");
    QHWI_LOGI("Configuration - pipeline type: %s", m_config.pipelineType == Config::PipelineType::monolithic ? "Monolithic" : "Pipeline Library");
    QHWI_LOGI("Configuration - number of pipelines: %d", m_config.numPipelines);
    QHWI_LOGI("Configuration - retain link time optimization: %s", m_config.retainLinkTimeOpt ? "true" : "false");
    QHWI_LOGI("Configuration - link time optimization: %s", m_config.linkTimeOpt ? "true" : "false");
    QHWI_LOGI("Configuration - run time pipeline creation: %s", m_config.runTimePipelines ? "true" : "false");
    QHWI_LOGI("Configuration - number of frames to render: %d", m_config.numFramesToRender);
    QHWI_LOGI("Configuration - number of frames per pipeline change: %d", m_config.numFramesPerPipeline);
    QHWI_LOGI("Configuration - vsync: %s", m_config.vsync ? "true" : "false");
    QHWI_LOGI("Configuration - shader complexity: %s",
              m_config.shaderVariation == 0 ? "permutations" :
              m_config.shaderVariation == 1 ? "dead varyings" :
              m_config.shaderVariation == 2 ? "heavy vec4" :
                                              "large UBO ");
    QHWI_LOGI("Configuration - max number of shaders: %d", m_config.maxNumShaders);
    QHWI_LOGI("Configuration - share shaders: %s", m_config.shareShaders ? "true" : "false");
    QHWI_LOGI("Configuration - use pipeline cache: %s", m_config.usePipelineCache ? "true" : "false");
    QHWI_LOGI("Configuration - small viewport: %s", m_config.useSmallViewport ? "true" : "false");
    QHWI_LOGI("");
    QHWI_LOGI("Pipeline Library Creation Time (ms): %d", m_pipelineLibraryCreationTime);
    QHWI_LOGI("Pipeline Creation Time (ms): %d", m_pipelineCreationTime);
    QHWI_LOGI("Rendering Time (ms): %d", m_renderingTime);
    QHWI_LOGI("GPU Rendering Time Total(ms): %f", m_plm.GetAggregatedRecordedMetric(s_metricId) * s_microSecondsToMilliSeconds);
    QHWI_LOGI("GPU Rendering Time per Frame(ms): %f",
              m_config.numFramesToRender == 0 ? 0.0 : m_plm.GetAggregatedRecordedMetric(s_metricId) * s_microSecondsToMilliSeconds / m_config.numFramesToRender);
    QHWI_LOGI("Rendering Complete");
}

void GraphicsPipelineLibraryExample::initialVertexBuffer()
{
    float4 colors[6] = { { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f },
                         { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } };

    const float4 pos[] = { { -.5f, -.5f, -.5f, 1.0f }, { -.5f, -.5f, +.5f, 1.0f }, { -.5f, +.5f, -.5f, 1.0f }, { -.5f, +.5f, +.5f, 1.0f },
                           { +.5f, -.5f, -.5f, 1.0f }, { +.5f, -.5f, +.5f, 1.0f }, { +.5f, +.5f, -.5f, 1.0f }, { +.5f, +.5f, +.5f, 1.0f } };
    const half4 faces[6] = {
        { 0, 2, 4, 6 }, //front
        { 5, 7, 1, 3 }, //back
        { 1, 3, 0, 2 }, //left
        { 4, 6, 5, 7 }, //right
        { 1, 0, 5, 4 }, //bottom
        { 2, 3, 6, 7 }  //top
    };
    const int NumVerts = 36;
    VSInput verts[NumVerts];

    VSInput* vert = verts;
    for (int j = 0; j < 6; j++)
    {
        const half4& face = faces[j];
        const float4& c = colors[j];
        *vert++ = { pos[face.x], c };
        *vert++ = { pos[face.y], c };
        *vert++ = { pos[face.z], c };
        *vert++ = { pos[face.z], c };
        *vert++ = { pos[face.y], c };
        *vert++ = { pos[face.w], c };
    }

    m_bVerts.initAsVertexBuffer(m_device.Get(), "verts", verts, NumVerts * sizeof(VSInput));
    m_numVerts = NumVerts;
}

GraphicsPipelineLibraryExample::~GraphicsPipelineLibraryExample()
{
    shutdown();
}

ResultCode GraphicsPipelineLibraryExample::init(int argc, const char* const* argv)
{
    cxxopts::Options opts("GraphicsPipelineLibraryExample", "Run Graphics Pipeline Library example");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;

    createTestConfigurations();
    parse(argc, argv, opts, parseResult, m_config);

    // Initialize configuration
    m_config.autoBarrier = true;
    m_config.debug = true; // if true, Vulkan validation will be run
    m_config.apiDump = false;
    m_config.backend = GraphicsBackend::eVulkan;
    m_config.vsync = false;

    srand(0);

    // Adjust the maximum number of shaders based on the Shader complexity
    int maxShaderId = m_config.maxNumShaders;

    if (m_config.shaderVariation == ShaderVariation::DeadVarying || m_config.shaderVariation == ShaderVariation::HeavyVec4 || m_config.shaderVariation == ShaderVariation::LargeUbo)
    {
        maxShaderId = 1;
    }

    if (m_config.shareShaders)
    {
        s_shaderpairs.push_back(std::pair<int, int>(0, 0));
    }
    else
    {
        for (int i = 0; i < (int)m_config.numPipelines; i++)
        {
            s_shaderpairs.push_back(std::pair<int, int>(rand() % maxShaderId, rand() % maxShaderId));
        }
    }

    // Initialize timing
    m_pipelineLibraryCreationTime = 0;
    m_pipelineCreationTime = 0;

    Example::init("GraphicsPipelineLibraryExample", &m_config);

    qhwi::VkDevice* vkDevice = qhwi::vk(m_device.Get());

    const auto& properties = vkDevice->getVkPropertiesInfo().properties2.properties;
    qre::PrintfSafe(m_gpuDeviceName, "%s", properties.deviceName.data());

    QHWI_LOGI("GPU: %s", m_gpuDeviceName);
    // Determine the GPU's GPL feature support.  Exit if no GPL support
    bool gplSupport = vkDevice->supportsExtension(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
    if (!gplSupport)
    {

        QHWI_LOGW("No GPL support..exiting");
        return ResultCode::eUnsupported;
    }

    // Determine the GPL properties
    const auto& gplProperties = vkDevice->getVkPropertiesInfo().graphicsPipelineLibrary;
    m_fastLinking = gplProperties.graphicsPipelineLibraryFastLinking;
    m_independentInterpolationDecoration = gplProperties.graphicsPipelineLibraryIndependentInterpolationDecoration;

    initialVertexBuffer();

    if (m_config.shaderVariation == ShaderVariation::LargeUbo)
    {
        float cbData[2048];
        memset(cbData, 0, 8192);
        m_constantbufferUBO.initAsConstantBuffer(m_device.Get(), "UBOConstantBuffer", &cbData, 8192);

        m_descriptorSetLayoutUBO = m_device->createDescriptorSetLayout({ qhwi::DescriptorSetLayoutDesc().addConstantBuffer(1, ShaderStage_Vertex) });
        m_descriptorSetUBO = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayoutUBO).addConstantBuffer(1, m_constantbufferUBO.get()));
    }

    beginTransfer();
    uploadStagingBuffers(m_copyCommandList.Get());
    endTransfer();

    pipelinePrep();
    if (!m_config.runTimePipelines)
    {
        createPipelines();
    }

    // Initialize the performance logging manager to capture gpu timing
    m_plm.Create(nullptr, "", "", &s_numFramesRendered, 0, false, nullptr);
    m_plm.Initialize(m_device.Get(), "", m_commandQueue);
    s_metricId = m_plm.RegisterMetric("GPL",
                                      "frameTime",
                                      qhwi_performance::PLMMetricType::eGPUTime,
                                      qhwi_performance::PLMMetricUnit::eMicrosec,
                                      "GPU time in usec",
                                      "TBD",
                                      1,
                                      qhwi_performance::PLMMetricAggregate::eSum,
                                      1);

    m_plm.ActivateMetrics({ "GPL" }, {}, {});

    // Start rendering time
    s_renderingStartTime = std::chrono::system_clock::now();

    return ResultCode::eSuccess;
}

void GraphicsPipelineLibraryExample::initCmdLineOpts(cxxopts::Options& opts)
{
    Example::initCmdLineOpts(opts);
    opts.add_options()("shaderVariation",
                       "0=permutations,1=deadVarying,2=HeavyVec4,3=largeUBO",
                       cxxopts::value<uint32_t>()->default_value("0"))("numPipelines", "Number of Pipelines", cxxopts::value<uint32_t>()->default_value("1024"))(
        "pipelineType",
        "0=monolithic 1=pipeline library",
        cxxopts::value<uint32_t>()->default_value("0"))("retainLinkTimeOpt", "Retain Link Time Optimization", cxxopts::value<bool>()->default_value("true"))(
        "linkTimeOpt",
        "Link Time Optimization",
        cxxopts::value<bool>()->default_value("true"))("runTimePipelines", "Runtime Pipeline Creation", cxxopts::value<bool>()->default_value("false"))(
        "numFramesToRender",
        "Number of Frames to Render",
        cxxopts::value<uint32_t>()->default_value("10000"))("numFramesPerPipeline", "Number of frames per pipeline change", cxxopts::value<uint32_t>()->default_value("10"))(
        "maxNumShaders",
        "Maximum number of shaders (encourage reuse)",
        cxxopts::value<uint32_t>()->default_value("10"))("vsync", "VSync", cxxopts::value<bool>()->default_value("false"))(
        "testConfig",
        "Test Configuration 0-10",
        cxxopts::value<int32_t>()->default_value("-1"))("shareShaders", "Share Shaders between the pipeline libraries", cxxopts::value<bool>()->default_value("false"))(
        "usePipelineCache",
        "Use pipeline cache when creating the pipelines",
        cxxopts::value<bool>()->default_value("true"))("smallViewport", "Render to a small viewport", cxxopts::value<bool>()->default_value("false"));
}

void GraphicsPipelineLibraryExample::createTestConfigurations()
{
    Config cfg;
    cfg.numPipelines = 100;
    cfg.runTimePipelines = false;
    cfg.numFramesToRender = 1000; //10000;
    cfg.numFramesPerPipeline = 10;
    cfg.maxNumShaders = 10;
    cfg.vsync = false;

    for (int shaderVariation = 0; shaderVariation < ShaderVariation::Count; shaderVariation++)
    {
        cfg.shaderVariation = (ShaderVariation)shaderVariation;
        for (int pipelineType = 0; pipelineType < (int)Config::PipelineType::Count; pipelineType++)
        {
            cfg.pipelineType = (Config::PipelineType)pipelineType;

            for (uint32_t optimization = 0; optimization < s_numBooleanOptions; optimization++)
            {
                cfg.linkTimeOpt = optimization ? true : false;
                cfg.retainLinkTimeOpt = optimization ? true : false;

                for (uint32_t sharedShaders = 0; sharedShaders < s_numBooleanOptions; sharedShaders++)
                {
                    cfg.shareShaders = sharedShaders ? true : false;

                    for (uint32_t usePipelineCache = 0; usePipelineCache < s_numBooleanOptions; usePipelineCache++)
                    {
                        cfg.usePipelineCache = usePipelineCache;
                        for (uint32_t smallViewport = 0; smallViewport < s_numBooleanOptions; smallViewport++)
                        {
                            cfg.useSmallViewport = smallViewport;
                            m_configurations.push_back(cfg);
                        }
                    }
                }
            }
        }
    }
}

void GraphicsPipelineLibraryExample::parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c)
{
    Example::parse(argc, argv, opts, p, c);
    int testId = p["testConfig"].as<int32_t>();

    if (testId >= 0)
    {
        if (testId < (int)m_configurations.size())
        {
            c = m_configurations[testId];
        }
        else
        {
            QHWI_LOGI("Unsupported test configuration: %d", testId);
            exit(0);
        }
        return;
    }
    c.shaderVariation = (ShaderVariation)p["shaderVariation"].as<uint32_t>();
    c.numPipelines = p["numPipelines"].as<uint32_t>();
    c.pipelineType = c.useExtension ? Config::PipelineType::pipelineLibrary : Config::PipelineType::monolithic;
    if (p.count("pipelineType"))
    {
        c.pipelineType = (Config::PipelineType)p["pipelineType"].as<uint32_t>();
    }
    c.retainLinkTimeOpt = p["retainLinkTimeOpt"].as<bool>();
    c.linkTimeOpt = p["linkTimeOpt"].as<bool>();
    c.runTimePipelines = p["runTimePipelines"].as<bool>();
    c.numFramesToRender = p["numFramesToRender"].as<uint32_t>();
    c.numFramesPerPipeline = p["numFramesPerPipeline"].as<uint32_t>();
    c.maxNumShaders = p["maxNumShaders"].as<uint32_t>();
    c.vsync = p["vsync"].as<bool>();
    c.shareShaders = p["shareShaders"].as<bool>();
    c.usePipelineCache = p["usePipelineCache"].as<bool>();
    c.useSmallViewport = p["smallViewport"].as<bool>();
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
    AppState* appState = (AppState*)app->userData;
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
    qre::OsSetLogConsole(true);

    char filePathBuf[256];
    qre::PrintfSafe(filePathBuf, "%s/%s", state->activity->internalDataPath, "gpl.log");
    qre::OsSetLogFilePath(filePathBuf);
    char internalDataPath[256] = ".";
    qre::PrintfSafe(internalDataPath, "%s", state->activity->internalDataPath);
    qre::OsSetInternalDataPath(internalDataPath);

    int argc;
    char** argv;
    GetCommandLineArgsOp op(state, "graphics_pipeline_library", argc, argv);
    op.run();

    AppState appState;
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    GraphicsPipelineLibraryExample app;
    while (true)
    {
        int ident = 0;
        int events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) >= 0)
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
            if (app.isDone())
            {
                ANativeActivity_finish(state->activity);
            }
        }
    }
}
#else
int main(int argc, char** argv)
{
    qre::OsSetLogConsole(true);
    qre::OsSetLogFilePath("gpl.log");
    GraphicsPipelineLibraryExample app;
    ResultCode ret = app.init(argc, argv);
    if (ret != ResultCode::eSuccess)
    {
        return -1;
    }

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
