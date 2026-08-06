//
//
//  qcom_image_proc.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "qhwi_example.h"
// TODO: remove dependency on test_util
#include "test_util.h"
using namespace test_util;

namespace qhwi
{
class QCOMImageProcExample : public Example
{
public:
    QCOMImageProcExample();
    ~QCOMImageProcExample() override;

    struct Config : public Example::Config
    {
        uint32_t kernelDim = 0;
        uint32_t kernelType = 0;
        uint32_t downscaleFactor = 0;
        uint32_t thumbWidth = 0;
        uint32_t thumbHeight = 0;
        uint32_t displayMode = 0;
        uint32_t frameHold = 0;
        uint32_t imageSteps = 0;
        std::string imageFile;
        enum class ShaderType
        {
            hlsl,
            glsl
        };
        ShaderType shaderType = ShaderType::hlsl;
    };

    ResultCode init(int argc, const char* const* argv);
    ResultCode init(const Config& config);

protected:
    // Callbacks
    void recordCommandList(CommandList* commandList) override;

    std::span<const char* const> pilMetrics() const override
    {
        static constexpr const char* k_extras[] = { "PERCENT_TIME_COMPUTE",
                                                    "PERCENT_SHADERS_BUSY",
                                                    "PERCENT_SHADER_ALU_CAPACITY_UTILIZED",
                                                    "SP_MEMORY_READ",
                                                    "GLOBAL_MEMORY_LOAD_INSTRUCTIONS" };
        return k_extras;
    }

private:
    // Perform the downsampling
    void downsample();

    // Draw the screen
    void draw(CommandList* cmdList);

    // Initialization
    void loadImageData();
    void initDownsampleTextures();
    void initKernelTexture();
    void initGeometry();
    void initSamplers();
    void initDescriptorSetLayouts();
    void initDescriptorSets();
    void initPipeline();
    void uploadTextureData();
    void createThumbs();

    // Filter coefficients.  All fall off to 0 at > 1.0 and < -1.0
    float GetMitchellNetravaliCoefficient(float x);
    float GetGaussianCoefficient(float x);
    float GetTriangleCoefficient(float x);

    // Convert 32 bit float to 16 bit float
    static uint16_t Half(float f);

    // Add string to log file
    void addLogEntry(float timeMs);

protected:
    void initCmdLineOpts(cxxopts::Options& opts);
    void parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& config);

    // Kernel/weight texture
    Texture1 m_textureKernel;

    // Image textures.  Element 0 stores original image.
    // 2^14 = 16384 so 14 should be enough to reduce max size image down to 1x1
    // even with small half-size steps.
    static const int MaxTextureArraySize = 14;
    static const int MaxTextureNameSize = 32;
    char m_textureNames[MaxTextureArraySize][MaxTextureNameSize];
    Texture1 m_textureArray[MaxTextureArraySize];
    qre::RefCountPtr<DescriptorSet> m_descriptorSetsDownsample[MaxTextureArraySize];
    qre::RefCountPtr<DescriptorSet> m_descriptorSetsDrawImage[MaxTextureArraySize];
    qre::RefCountPtr<DescriptorSet> m_descriptorSetsThumbs[MaxTextureArraySize];

    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayoutDownsample;
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayoutDrawImage;

    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayoutDownsample;
    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayoutDrawImage;

    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipelineDownsample;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipelineDrawImage;

    qre::RefCountPtr<Sampler> m_samplerTexImage;
    qre::RefCountPtr<Sampler> m_samplerKernel;

    Buffer1 m_bPos;
    Buffer1 m_bUv;
    Buffer1 m_bUvRotated;

    uint32_t m_numVerts = 0;
    int m_nTextureArraySize = 0;

    // config options
    enum class Kernel
    {
        eTriangle,
        eGaussian,
        eMitchellNetravali
    };
    enum class DisplayMode
    {
        eSpatialSequence,
        eTemporalSequence,
        ePermutations
    };
    DisplayMode m_displayMode;
    Kernel m_kernel;
    int m_winWidth;
    int m_winHeight;
    int m_thumbWidth;
    int m_thumbHeight;
    int m_dimKernel;
    int m_downscaleFactor;
    int m_frameHold;
    int m_imageSteps;
    std::string m_imageFile;

    // Array of outputs for quality comparison mode
    struct PermutationParam
    {
        int start;
        int step;
        int nIter;
    };

    static constexpr const PermutationParam vKernel = { 4, 2, 4 };
    static constexpr PermutationParam vDownscale = { 2, 1, 4 };
    Texture1 m_textureThumbs[vKernel.nIter][vDownscale.nIter];

    static const uint32_t B_POS = 0;
    static const uint32_t B_UV = 1;

    Config m_config;
    cxxopts::ParseResult m_params;
    bool m_useQcomImageProc = false;
};
} // namespace qhwi