//
//
//  qcom_image_proc.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <chrono>
#include <thread>
#include <vector>

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <fstream>
#    include <qhwi/qhwi_android_window.h>
#endif

#include "qcom_image_proc.h"
#include <qhwi/qhwi_util.h>
#include <qre/text/textutil.h>

static int clamp(int x, int minVal, int maxVal)
{
    return std::clamp(x, minVal, maxVal);
}

// ------------------------------------------------------------------------------------------------------------------------
//   Constructor
// ------------------------------------------------------------------------------------------------------------------------
QCOMImageProcExample::QCOMImageProcExample()
{
    for (int i = 0; i < MaxTextureArraySize; ++i)
    {
        qre::PrintfSafe(m_textureNames[i], MaxTextureNameSize, "texArray[%i]", i);
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Get coefficients for Mitchell-Netravali m_kernel
//   https://en.wikipedia.org/wiki/Mitchell%E2%80%93Netravali_filters
// ------------------------------------------------------------------------------------------------------------------------
float QCOMImageProcExample::GetMitchellNetravaliCoefficient(float x)
{
    x = fabsf(2.0f * x);
    const float B = 1.0f / 3.0f;
    const float C = 1.0f / 3.0f;
    float x2 = x * x;
    float x3 = x2 * x;

    if (x < 1.0f)
    {
        return (12.0f - 9.0f * B - 6.0f * C) * x3 + (-18.0f + 12.0f * B + 6.0f * C) * x2 + (6.0f - 2.0f * B);
    }
    else if (x <= 2.0f)
    {
        return (-B - 6.0f * C) * x3 + (6.0f * B + 30.0f * C) * x2 + (-12.0f * B - 48.0f * C) * x + (8.0f * B + 24.0f * C);
    }
    else
    {
        return 0.0f;
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Get coefficients for Gaussian m_kernel
// ------------------------------------------------------------------------------------------------------------------------
float QCOMImageProcExample::GetGaussianCoefficient(float x)
{
    x = fabsf(x);
    float sigma = 0.5f * float(m_downscaleFactor);
    return expf(-sigma * x * x);
}

// ------------------------------------------------------------------------------------------------------------------------
//   Get coefficients for triangle m_kernel
// ------------------------------------------------------------------------------------------------------------------------
float QCOMImageProcExample::GetTriangleCoefficient(float x)
{
    return 1.0f - fabsf(x);
}

// ------------------------------------------------------------------------------------------------------------------------
//   Convert 32 bit float to 16 bit float
// ------------------------------------------------------------------------------------------------------------------------
uint16_t QCOMImageProcExample::Half(float f)
{
    uint32_t i;
    memcpy(&i, &f, sizeof(i));
    uint16_t sign = (i >> 16) & 0x8000;
    i &= 0x7fffffff;

    uint16_t value;
    if (i > 0x47ffefff)
    {
        // Inf
        value = 0x7fff;
    }
    else
    {
        if (i < 0x38800000)
        {
            // subnormal
            int shift = 113 - (i >> 23);
            i = (0x800000 | (i & 0x7fffff)) >> shift;
        }
        else
        {
            i += 0xc8000000;
        }
        value = ((i + 0xfff + ((i >> 13) & 1)) >> 13) & 0x7fff;
    }

    return sign | value;
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize descriptor set layouts for downsampling and presentation
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initDescriptorSetLayouts()
{
    m_descriptorSetLayoutDrawImage = m_device->createDescriptorSetLayout({ qhwi::DescriptorSetLayoutDesc().addTexture(0, ShaderStage_Pixel).addSampler(0, ShaderStage_Pixel) });
    if (m_useQcomImageProc)
    {
        m_descriptorSetLayoutDownsample = m_device->create({ DescriptorSetLayoutDesc()
                                                                 .addTexture(0, ShaderStage_Pixel) // inputTex
                                                                 .addSampler(0, ShaderStage_Pixel)
                                                                 .addSampleWeightImageQCOM(1, ShaderStage_Pixel) // kernelTex
                                                                 .addSampler(1, ShaderStage_Pixel) });
    }
    else
    {
        m_descriptorSetLayoutDownsample = m_device->create({ DescriptorSetLayoutDesc()
                                                                 .addTexture(0, ShaderStage_Pixel) //inputTex
                                                                 .addSampler(0, ShaderStage_Pixel)
                                                                 .addTexture(1, ShaderStage_Pixel) // kernelTex
                                                                 .addSampler(1, ShaderStage_Pixel) });
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize descriptor sets for downsampling and presentation
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initDescriptorSets()
{
    for (int i = 0; i < m_nTextureArraySize; ++i)
    {
        m_descriptorSetsDrawImage[i] =
            m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayoutDrawImage).addTexture(0, m_textureArray[i].texture).addSampler(0, m_samplerTexImage));
        if (m_useQcomImageProc)
        {
            m_descriptorSetsDownsample[i] = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayoutDownsample)
                                                                              .addTexture(0, m_textureArray[i].get())
                                                                              .addSampler(0, m_samplerTexImage)
                                                                              .addSampleWeightImageQCOM(1, m_textureKernel.get())
                                                                              .addSampler(1, m_samplerKernel));
        }
        else
        {
            m_descriptorSetsDownsample[i] = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayoutDownsample)
                                                                              .addTexture(0, m_textureArray[i].get())
                                                                              .addSampler(0, m_samplerTexImage)
                                                                              .addTexture(1, m_textureKernel.get())
                                                                              .addSampler(1, m_samplerKernel));
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize graphics pipelines for downsampling and presentation
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initPipeline()
{
    m_graphicsPipelineLayoutDownsample = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayoutDownsample).setName("DownsamplePipelineLayout"));

    Shader1 vs, ps;
    if (m_useQcomImageProc)
    {
        if (m_config.shaderType == Config::ShaderType::glsl)
        {
            initShader(m_device.Get(), "downscale_qcom_glsl_vert", "main", qhwi::ShaderType::eVertex, vs);
            initShader(m_device.Get(), "downscale_qcom_glsl_frag", "main", qhwi::ShaderType::ePixel, ps);
        }
        else
        {
            initShader(m_device.Get(), "downscale_qcom", "VSMain", qhwi::ShaderType::eVertex, vs);
            initShader(m_device.Get(), "downscale_qcom", "PSMain", qhwi::ShaderType::ePixel, ps);
        }
    }
    else
    {
        initShader(m_device.Get(), "downscale", "VSMain", qhwi::ShaderType::eVertex, vs);
        initShader(m_device.Get(), "downscale", "PSMain", qhwi::ShaderType::ePixel, ps);
    }

    m_graphicsPipelineDownsample = m_device.Get()->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                                            .pixelShader = ps.shader,
                                                                            .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                                          .semanticIndex = 0,
                                                                                                                                          .binding = B_POS,
                                                                                                                                          .format = Format::eRg32Float,
                                                                                                                                          .stride = sizeof(float2) },
                                                                                                                                        { .semanticName = "TEXCOORD",
                                                                                                                                          .semanticIndex = 0,
                                                                                                                                          .binding = B_UV,
                                                                                                                                          .format = Format::eRg32Float,
                                                                                                                                          .stride = sizeof(float2) } } },
                                                                            .layout = m_graphicsPipelineLayoutDownsample,
                                                                            .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                                                            .colorAttachmentFormats = { Format::eRgba8Unorm },
                                                                            .name = "DownsamplePipeline" });

    m_graphicsPipelineLayoutDrawImage =
        m_device.Get()->createPipelineLayout({ PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayoutDrawImage).setName("DrawImagePipelineLayout") });

    initShader(m_device.Get(), "drawImage", "VSMain", qhwi::ShaderType::eVertex, vs);
    initShader(m_device.Get(), "drawImage", "PSMain", qhwi::ShaderType::ePixel, ps);
    m_graphicsPipelineDrawImage = m_device.Get()->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                                           .pixelShader = ps.shader,
                                                                           .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                                         .semanticIndex = 0,
                                                                                                                                         .binding = B_POS,
                                                                                                                                         .format = Format::eRg32Float,
                                                                                                                                         .stride = sizeof(float2) },
                                                                                                                                       { .semanticName = "TEXCOORD",
                                                                                                                                         .semanticIndex = 0,
                                                                                                                                         .binding = B_UV,
                                                                                                                                         .format = Format::eRg32Float,
                                                                                                                                         .stride = sizeof(float2) } } },
                                                                           .layout = m_graphicsPipelineLayoutDrawImage,
                                                                           .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                                                           .colorAttachmentFormats = { m_colorAttachmentFormat },
                                                                           .name = "DrawImagePipeline" });
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize geometry to draw full screen quad
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initGeometry()
{
    Quad quad;
    quad.init();
    m_numVerts = quad.NumVerts;

    m_bPos.initAsVertexBuffer(m_device.Get(), "pos", quad.pos, m_numVerts * sizeof(float2));
    m_bUv.initAsVertexBuffer(m_device.Get(), "uv", quad.uv, m_numVerts * sizeof(float2));
    m_bUvRotated.initAsVertexBuffer(m_device.Get(), "uv", quad.uv, m_numVerts * sizeof(float2));
}

// ------------------------------------------------------------------------------------------------------------------------
//   Tries to load a texture from disk, if it fails it creates procedural data instead
//   Source file should be in //sdcard/Android/data/com.qualcomm.qhwi_test/files/data/
//   Result is placed in m_textureImage
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::loadImageData()
{
    bool textureCreated = false;

    if (!m_imageFile.empty())
    {
        std::string extension = m_imageFile.substr(m_imageFile.find_last_of(".") + 1);
        if (extension == "jpg" || extension == "png")
        {
            ImageData dataImage;
            ext::ImageUtil::ResultCode finalResult = ext::ImageUtil::ResultCode::eFailed;

#ifdef QHWI_PLATFORM_ANDROID
            // This code block first tries to read the image from the data dir in the apk
            // If that fails it tries to read the image from //sdcard/Android/data/com.qualcomm.qhwi_test/files
            // If that fails, it creates a procedural test image
            std::string path = std::string("data/") + m_imageFile;
            FileData byteCode;
            qhwi::FileUtil::ResultCode readResult = FileUtil::readAndroidAsset(path.c_str(), byteCode);
            if (readResult == qhwi::FileUtil::ResultCode::eSuccess)
            {
                finalResult = ext::ImageUtil::readImage(byteCode, dataImage);
            }
            else
            {
                path = std::string("sdcard/Android/data/com.qualcomm.qhwi_test/files/") + m_imageFile;
                finalResult = ext::ImageUtil::readImage(path.c_str(), dataImage);
            }
#else
            std::string path = std::string(QHWI_DATA_DIR) + "/" + m_imageFile;
            finalResult = ext::ImageUtil::readImage(path.c_str(), dataImage);
#endif
            // Load image from data directory (if it's there), if not a procedural test image will be created
            if (finalResult == ext::ImageUtil::ResultCode::eSuccess)
            {
                m_textureArray[0].init(m_device.Get(),
                                       { .w = uint32_t(dataImage.w), .h = uint32_t(dataImage.h), .format = Format::eRgba8Unorm, .name = "textureInput", .automaticBarriers = true },
                                       dataImage.data);
                textureCreated = true;
            }
        }
    }

    // If no image was loaded for some reason, create a procedural one
    if (!textureCreated)
    {
        int w = m_thumbWidth * 16;
        int h = m_thumbHeight * 16;

        // Generate test data algorithmically
        std::vector<uint32_t> dataProcedural(w * h);
        const float cx = 0.5f * float(w) - 0.5f;
        const float cy = 0.5f * float(h) - 0.5f;
        const float rScale = 1.0f / float(w);
        for (int j = 0; j < h; ++j)
        {
            float dy = float(j) - cy;
            for (int i = 0; i < w; ++i)
            {
                float dx = float(i) - cx;
                float r2 = (dx * dx + dy * dy);
                float c = 0.5f + 0.5f * sinf(rScale * r2);
                unsigned char u = (unsigned char)(c * 255.0f);
                dataProcedural[j * w + i] = u | u << 8 | u << 16 | u << 24;
            }
        }

        m_textureArray[0].init(m_device.Get(),
                               { .w = uint32_t(w), .h = uint32_t(h), .format = Format::eRgba8Unorm, .name = "textureInput", .automaticBarriers = true },
                               dataProcedural.data());
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize main image to be downsampled and array of textures for intermediate steps
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initDownsampleTextures()
{
    uint32_t w = m_textureArray[0].texture->desc()->w;
    uint32_t h = m_textureArray[0].texture->desc()->h;

    // Figure out how many textures we will need in our downsample chain
    uint32_t wTemp = w;
    m_nTextureArraySize = 1;
    while (wTemp > uint32_t(m_thumbWidth)) // Assume final aspect ratio is almost the same as initial
    {
        wTemp /= m_downscaleFactor;
        ++m_nTextureArraySize;
    }

    QHWI_ASSERT_MSG(m_nTextureArraySize <= MaxTextureArraySize, "This downscale factor would require %i images, but %i is maximum", m_nTextureArraySize, MaxTextureArraySize);

    // Make sure step number is less than texture array size
    m_imageSteps = clamp(m_imageSteps, 1, m_nTextureArraySize);

    // Allocate them
    wTemp = w;
    uint32_t hTemp = h;

    for (uint32_t i = 1; i < uint32_t(m_nTextureArraySize); ++i)
    {
        // Smallest level should be exact thumbwidth and thumbheight.
        // This may not be equivalent to downscale multiple from previous level.
        if (i == uint32_t(m_nTextureArraySize - 1))
        {
            wTemp = m_thumbWidth;
            hTemp = m_thumbHeight;
        }
        else
        {
            wTemp /= m_downscaleFactor;
            hTemp /= m_downscaleFactor;
        }

        m_textureArray[i].init(m_device.Get(),
                               { .w = wTemp,
                                 .h = hTemp,
                                 .format = Format::eRgba8Unorm,
                                 .usage = TextureUsage_ColorAttachment | TextureUsage_Sampled,
                                 .name = m_textureNames[i],
                                 .automaticBarriers = true },
                               nullptr);
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize normalized m_kernel/weight texture. Coefficients should sum to 1.0
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initKernelTexture()
{
    QHWI_LOGI("Creating m_kernel texture dim = %i", m_dimKernel);

    const float cx = 0.5f * float(m_dimKernel) - 0.5f;
    const float cy = 0.5f * float(m_dimKernel) - 0.5f;
    const float scale = 1.0f / sqrtf(cx * cx + cy * cy);
    float sum = 0.0f;
    float* dataKernel = new float[m_dimKernel * m_dimKernel];
    for (int j = 0; j < m_dimKernel; ++j)
    {
        float dy = float(j) - cy;
        for (int i = 0; i < m_dimKernel; ++i)
        {
            float dx = float(i) - cx;
            float d = scale * sqrtf(dx * dx + dy * dy);
            float c;
            switch (m_kernel)
            {
            case Kernel::eTriangle:
                c = GetTriangleCoefficient(d);
                break;
            case Kernel::eGaussian:
            default:
                c = GetGaussianCoefficient(d);
                break;
            case Kernel::eMitchellNetravali:
                c = GetMitchellNetravaliCoefficient(d);
                break;
            }
            //QHWI_LOGI("(%i,%i) d = %f, c = %f", i, j, d, c);
            dataKernel[j * m_dimKernel + i] = c;
            sum += c;
        }
    }

    // Normalize and convert to 16 bit float
    float norm = 1.0f / sum;
    uint16_t* cData = (uint16_t*)dataKernel;
    for (int i = 0; i < m_dimKernel * m_dimKernel; ++i)
    {
        cData[i] = Half(dataKernel[i] * norm);
    }

    TextureUsageMask usage = TextureUsage_Sampled;
    if (m_useQcomImageProc)
    {
        usage |= TextureUsage_SampleWeightQCOM;
    }
    m_textureKernel.init(m_device.Get(),
                         { .w = uint32_t(m_dimKernel),
                           .h = uint32_t(m_dimKernel),
                           .mipLevels = 1,
                           .arrayLayers = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eR16Float,
                           .usage = usage,
                           .type = TextureType::e2DArray,
                           .name = "textureKernel",
                           .automaticBarriers = false },
                         dataKernel);

    delete[] dataKernel;
}

// ------------------------------------------------------------------------------------------------------------------------
//   Initialize samplers for linear, nearest, and m_kernel filtering
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::initSamplers()
{
    SamplerDesc inputTexSamplerDesc = SamplerDesc().setAllFilters(Filter::eLinear).setAllAddressModes(SamplerAddressMode::eClampToEdge);

    SamplerDesc kernelTexSamplerDesc = SamplerDesc().setAllFilters(Filter::eNearest).setAllAddressModes(SamplerAddressMode::eClampToEdge);

    if (m_useQcomImageProc)
    {
        kernelTexSamplerDesc.flags = SamplerCreateFlag_ImageProcessingQCOM;
        kernelTexSamplerDesc.reductionMode = SamplerReductionMode::eWeightedAverage;
        kernelTexSamplerDesc.maxLod = 0.0f;
    }

    m_samplerTexImage = m_device->createSampler(inputTexSamplerDesc);
    m_samplerKernel = m_device->createSampler(kernelTexSamplerDesc);
}

// ------------------------------------------------------------------------------------------------------------------------
//   Upload texture data from staging buffers to GPU
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::uploadTextureData()
{
    static bool first = true;

    beginTransfer();

    auto cmdList = m_copyCommandList.Get();

    if (first)
    {
        first = false;
        m_bPos.uploadFromStagingBuffer(cmdList);
        m_bUv.uploadFromStagingBuffer(cmdList);
        m_bUvRotated.uploadFromStagingBuffer(cmdList);

        m_textureArray[0].uploadFromStagingBuffer(cmdList);
    }

    cmdList->addTextureBarrier(
        { .texture = m_textureKernel.get(), .src = qhwi::TextureState_NoAccess, .dst = qhwi::TextureState_Transfer_Dst, .subresourceRange = EntireResource });

    m_textureKernel.uploadFromStagingBuffer(cmdList);

    cmdList->addTextureBarrier(
        { .texture = m_textureKernel.get(), .src = qhwi::TextureState_Transfer_Dst, .dst = qhwi::TextureState_PixelShader_Sample, .subresourceRange = EntireResource });

    endTransfer();
}

// ------------------------------------------------------------------------------------------------------------------------
//   Run downsample multiple times and store the final results
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::createThumbs()
{
    m_dimKernel = vKernel.start;
    for (int j = 0; j < vKernel.nIter; ++j, m_dimKernel += vKernel.step)
    {
        initKernelTexture();
        m_downscaleFactor = vDownscale.start;
        for (int i = 0; i < vDownscale.nIter; ++i, m_downscaleFactor += vDownscale.step)
        {
            QHWI_LOGI("Generating m_dimKernel = %i, m_downscaleFactor = %i: ", m_dimKernel, m_downscaleFactor);

            initDownsampleTextures();
            initDescriptorSetLayouts();
            initDescriptorSets();
            initPipeline();
            uploadTextureData();
            downsample();

            m_textureThumbs[j][i] = m_textureArray[m_nTextureArraySize - 1];
        }
    }

    // Create descriptor sets
    for (int j = 0; j < vKernel.nIter; ++j)
    {
        for (int i = 0; i < vDownscale.nIter; ++i)
        {
            int index = j * vDownscale.nIter + i;
            m_descriptorSetsThumbs[index] =
                m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayoutDrawImage).addTexture(0, m_textureThumbs[j][i].texture).addSampler(0, m_samplerTexImage));
        }
    }
}

QCOMImageProcExample::~QCOMImageProcExample()
{
    shutdown();
}

// ------------------------------------------------------------------------------------------------------------------------
//   Main initialization function
// ------------------------------------------------------------------------------------------------------------------------
ResultCode QCOMImageProcExample::init(int argc, const char* const* argv)
{
    cxxopts::Options opts("QCOMImageProcExample", "Run QCOM image proc example");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    parse(argc, argv, opts, parseResult, m_config);

    Example::init("QCOMImageProcExample", &m_config);

    m_useQcomImageProc = m_config.useExtension && m_device->supportsImageProcessingQCOM();

    // Copy and validate config options
    m_downscaleFactor = clamp(m_config.downscaleFactor, 2, 16);
    m_dimKernel = clamp(m_config.kernelDim, 2, 16);
    m_kernel = Kernel(clamp(m_config.kernelType, 0, 1));
    m_thumbWidth = m_config.thumbWidth;
    m_thumbHeight = m_config.thumbHeight;
    m_imageFile = m_config.imageFile;
    m_frameHold = m_config.frameHold;
    m_imageSteps = clamp(m_config.imageSteps, 1, 100);
    m_winWidth = m_config.w;
    m_winHeight = m_config.h;
    if (m_winWidth == 0)
    {
        m_winWidth = m_window->getWidth();
    }
    if (m_winHeight == 0)
    {
        m_winHeight = m_window->getHeight();
    }
    m_displayMode = DisplayMode(clamp(m_config.displayMode, 0, 2));

    loadImageData();

    initDownsampleTextures();

    initKernelTexture();

    initGeometry();

    initSamplers();

    initDescriptorSetLayouts();

    initDescriptorSets();

    initPipeline();

    uploadTextureData();

    downsample();

    if (m_displayMode == DisplayMode::ePermutations)
    {
        createThumbs();
    }

    return ResultCode::eSuccess;
}

void QCOMImageProcExample::initCmdLineOpts(cxxopts::Options& opts)
{
    Example::initCmdLineOpts(opts);
    opts.add_options()
        // Parameters specific to downscaling test
        ("kernel-type",
         "Filter type: 0=triangle, 1=Gaussian, 2=Mitchell-Netravali",
         cxxopts::value<uint32_t>()->default_value("1"))("kernel-dim", "Dimensions of kernel (weight) texture", cxxopts::value<uint32_t>()->default_value("9"))(
            "downscale-factor",
            "Width and height are divided by this at step of downscaling.  Must be > 1",
            cxxopts::value<uint32_t>()->default_value("4"))("thumbnail-width", "Final width for downscaled image", cxxopts::value<uint32_t>()->default_value("255"))(
            "thumbnail-height",
            "Final height for downscaled image",
            cxxopts::value<uint32_t>()->default_value("191"))("display-mode", "0=spatial sequence , 1=temporal sequence", cxxopts::value<uint32_t>()->default_value("1"))(
            "frame-hold",
            "Number of frames to hold each image if display-mode is 1",
            cxxopts::value<uint32_t>()->default_value(
                "400"))("image-steps", "Number of image steps to show when display-mode is 1.  Must be >= 1", cxxopts::value<uint32_t>()->default_value("400"))(
            "image-file",
            "File name of image to load.",
            cxxopts::value<std::string>()->default_value(""))("shader-type", "0=hlsl, 1=glsl", cxxopts::value<uint32_t>()->default_value("1"));
}

void QCOMImageProcExample::parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c)
{
    Example::parse(argc, argv, opts, p, c);
    c.kernelDim = p["kernel-dim"].as<uint32_t>();
    c.kernelType = p["kernel-type"].as<uint32_t>();
    c.downscaleFactor = p["downscale-factor"].as<uint32_t>();
    c.thumbWidth = p["thumbnail-width"].as<uint32_t>();
    c.thumbHeight = p["thumbnail-height"].as<uint32_t>();
    c.displayMode = p["display-mode"].as<uint32_t>();
    c.frameHold = p["frame-hold"].as<uint32_t>();
    c.imageSteps = p["image-steps"].as<uint32_t>();
    c.imageFile = p["image-file"].as<std::string>();
    c.shaderType = (Config::ShaderType)p["shader-type"].as<uint32_t>();
}

// ------------------------------------------------------------------------------------------------------------------------
//   Downsample input image.  Only called if downsample parameters change.
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::downsample()
{
    // This allows this function to be identified in the power profile
    const long long int sleepTimeMs = 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));

    qre::RefCountPtr<CommandList> cmdList = m_device->createCommandList(m_commandQueue, { .name = "downsampleCmdList", .supportsAutomaticBarriers = true });
    cmdList->begin();

    qre::RefCountPtr<TimerQuery> timer = m_device->createTimerQuery();
    cmdList->resetTimerQueryPool(timer);
    cmdList->beginTimerQuery(timer);

    for (int i = 0; i < m_nTextureArraySize - 1; ++i)
    {
        uint32_t w = m_textureArray[i + 1].texture->desc()->w;
        uint32_t h = m_textureArray[i + 1].texture->desc()->h;

        // Bind array element i+1 as color attachment target
        ColorAttachmentDescription colorAttachmentDesc = { .textureData =
                                                               TextureAttachmentData{
                                                                   .texture = m_textureArray[i + 1].get(),
                                                               },
                                                           .loadOp = AttachmentLoadOp::eDontCare,
                                                           .storeOp = AttachmentStoreOp::eStore,
                                                           .initialState = TextureState_ColorAttachment };

        // Begin
        cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr);

        cmdList->setViewport({ .w = float(w), .h = float(h) });
        cmdList->setScissor({ .w = w, .h = h });

        // Render source element i
        cmdList->bindGraphicsPipeline(m_graphicsPipelineDownsample.Get());
        cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
        cmdList->bindVertexBuffer(m_bUv.get(), B_UV, 0, sizeof(float2));
        cmdList->bindDescriptorSet(m_descriptorSetsDownsample[i].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayoutDownsample.Get());
        cmdList->draw(m_numVerts);

        // End
        cmdList->endRenderPass();
    }

    cmdList->endTimerQuery(timer);
    cmdList->resolveTimerQueryData(timer);
    cmdList->end();

    m_commandQueue->executeCommandList(cmdList);
    m_commandQueue->waitForIdle();

    float time = 0.0f;
    m_device->getTimerQueryTimeInSec(timer, m_commandQueue, true, time);

    // This allows this function to be identified in the power profile
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));

    // Add timing result to log file
    addLogEntry(time * 1000.0f);
}

void QCOMImageProcExample::addLogEntry(float timeMs)
{
    QHWI_LOGI("KERNEL_TYPE %d", int(m_kernel));
    QHWI_LOGI("KERNEL_DIM %d", m_dimKernel);
    QHWI_LOGI("VK_QCOM_IMAGE_PROCESSING %d", m_device->supportsImageProcessingQCOM() ? 1 : 0);
    QHWI_LOGI("DOWNSAMPLE_GPU_TIME_MS %g", timeMs);
    for (int i = 0; i < m_nTextureArraySize; ++i)
    {
        QHWI_LOGI("DOWNSAMPLE_RESOLUTION_STEP_%d %d %d", i, int(m_textureArray[i].texture->desc()->w), int(m_textureArray[i].texture->desc()->h));
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Main draw function
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::draw(CommandList* cmdList)
{
    if (m_displayMode == DisplayMode::eSpatialSequence)
    {
        cmdList->bindGraphicsPipeline(m_graphicsPipelineDrawImage.Get());
        cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
        cmdList->bindVertexBuffer(m_bUvRotated.get(), B_UV, 0, sizeof(float2));
        int x = 0;
        bool onscreen = true;
        for (int i = m_nTextureArraySize - 1; i >= 0 && onscreen; --i)
        {
            int w = m_textureArray[i].texture->desc()->w;
            int h = m_textureArray[i].texture->desc()->h;

            cmdList->setViewport({ .x = float(x), .y = float(m_winHeight - int(h)) * 0.5f, .w = float(w), .h = float(h) });

            cmdList->bindDescriptorSet(m_descriptorSetsDrawImage[i].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayoutDrawImage.Get());
            cmdList->draw(m_numVerts);

            x += w;

            onscreen = x < m_winWidth;
        }
    }
    else if (m_displayMode == DisplayMode::eTemporalSequence)
    {
        // Draw each successive downsample level for a duration of nFrames
        static uint32_t index = m_nTextureArraySize - 1;
        static uint32_t count = 0;
        static uint32_t w = m_textureArray[index].texture->desc()->w;
        static uint32_t h = m_textureArray[index].texture->desc()->h;

        ++count;
        if (count >= uint32_t(m_frameHold))
        {
            count = 0;
            index = m_nTextureArraySize - 1 - index; //invert
            index = (index + 1) % m_imageSteps;      // wrap
            index = m_nTextureArraySize - 1 - index; // invert
            w = m_textureArray[index].texture->desc()->w;
            h = m_textureArray[index].texture->desc()->h;
            QHWI_LOGI("downscale level = %i, w = %i h = %i", index, w, h);
        }

        // If you want to see each downsample blown up to window size for inspection just comment out the next line
        cmdList->setViewport({ .x = float(m_winWidth - int(w)) * 0.5f, .y = float(m_winHeight - int(h)) * 0.5f, .w = float(w), .h = float(h) });

        cmdList->bindGraphicsPipeline(m_graphicsPipelineDrawImage.Get());
        cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
        cmdList->bindVertexBuffer(m_bUvRotated.get(), B_UV, 0, sizeof(float2));
        cmdList->bindDescriptorSet(m_descriptorSetsDrawImage[index].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayoutDrawImage.Get());
        cmdList->draw(m_numVerts);
    }
    else if (m_displayMode == DisplayMode::ePermutations)
    {
        cmdList->bindGraphicsPipeline(m_graphicsPipelineDrawImage.Get());
        cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
        cmdList->bindVertexBuffer(m_bUvRotated.get(), B_UV, 0, sizeof(float2));

        for (int j = 0; j < vKernel.nIter; ++j)
        {
            for (int i = 0; i < vDownscale.nIter; ++i)
            {
                cmdList->bindDescriptorSet(m_descriptorSetsThumbs[j * vDownscale.nIter + i].Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayoutDrawImage.Get());
                cmdList->setViewport({ .x = float(i * m_thumbWidth), .y = float(j * m_thumbHeight), .w = float(m_thumbWidth), .h = float(m_thumbHeight) });
                cmdList->draw(m_numVerts);
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------
//   Callback to render this test
// ------------------------------------------------------------------------------------------------------------------------
void QCOMImageProcExample::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    beginRenderPass(commandList);
    draw(commandList);
    endRenderPass(commandList);
    commandList->end();
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

    int argc;
    char** argv;
    GetCommandLineArgsOp op(state, "qcom_image_proc", argc, argv);
    op.run();

    AppState appState;
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    QCOMImageProcExample app;
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
    qre::OsSetLogFilePath("");
    QCOMImageProcExample app;
    app.init(argc, argv);
    auto window = app.window();
    while (true)
    {
        window->pollEvents();
        if (window->shouldClose() || app.isDone())
        {
            break;
        }
        app.runIteration();
    }
    return 0;
}
#endif