//
//
//  test_mipmap.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
#include "qhwi/qhwi_ops.h"
#include <random>
#include <iostream>

constexpr uint8_t patternSizeInPixels = 64;
constexpr uint32_t g_rgba8SrcPattern[] = {
    4286069420, 4288662909, 4283575040, 4293557083, 4291578455, 4293455206, 4287144301, 4292202061, 4287910344, 4294880140, 4288076212, 4282825201, 4282090557,
    4280065837, 4290558694, 4285371632, 4288340412, 4291722735, 4282265899, 4288997813, 4293782737, 4284176573, 4291053060, 4284854096, 4290478902, 4289077693,
    4278962630, 4285492491, 4292785213, 4283057815, 4280853446, 4281017074, 4287843038, 4290762426, 4284195948, 4290911314, 4279489448, 4291503096, 4288966719,
    4285505129, 4288679615, 4294554815, 4291157771, 4279531757, 4283755219, 4293917006, 4282421069, 4282031568, 4293213675, 4291445368, 4287509198, 4286806092,
    4281867838, 4288582081, 4278890742, 4287240276, 4283228800, 4290489525, 4281916257, 4290829881, 4289553641, 4291088973, 4282634061, 4279564765,
};

test_util::float2 vertices[] = {
    // -X
    { -1.0f, -0.33f },
    { -1.0f, 0.34f },
    { -0.5f, +0.34f },
    { -1.0f, -0.33f },
    { -0.5f, 0.34f },
    { -0.5f, -0.33f },

    // +Z
    { -0.5f, -0.33f },
    { -0.5f, 0.34f },
    { 0.0f, +0.34f },
    { -0.5f, -0.33f },
    { +0.0f, 0.34f },
    { 0.0f, -0.33f },

    // +X
    { 0.0f, -0.33f },
    { 0.0f, 0.34f },
    { 0.5f, +0.34f },
    { 0.0f, -0.33f },
    { 0.5f, 0.34f },
    { 0.5f, -0.33f },

    // -Z
    { 0.5f, -0.33f },
    { 0.5f, 0.34f },
    { 1.0f, +0.34f },
    { 0.5f, -0.33f },
    { 1.0f, 0.34f },
    { 1.0f, -0.33f },

    // +Y
    { -0.5f, 0.34f },
    { -0.5f, 1.0f },
    { 0.0f, 1.00f },
    { -0.5f, 0.34f },
    { +0.0f, 1.0f },
    { 0.0f, 0.34f },

    // -Y
    { -0.5f, -1.0f },
    { -0.5f, -0.33f },
    { 0.0f, -0.33f },
    { -0.5f, -1.0f },
    { +0.0f, -0.33f },
    { 0.0f, -1.00f },
};

test_util::float3 uvwData[] = {
    // -X
    { -1.0f, 0.0f, 0.0f },
    { -1.0f, 0.0f, 0.0f },
    { -1.0f, 0.0f, 0.0f },
    { -1.0f, 0.0f, 0.0f },
    { -1.0f, 0.0f, 0.0f },
    { -1.0f, 0.0f, 0.0f },

    // +Z
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f },

    // +X
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },

    // -Z
    { 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, -1.0f },
    { 0.0f, 0.0f, -1.0f },

    // +Y
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },

    // -Y
    { 0.0f, -1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
};

class TestMipmap final : public UnitTest
{
public:
    TestMipmap(qhwi::GraphicsBackend graphicsBackend,
               Config config,
               qhwi::TextureType textureType,
               qhwi::Format format,
               unsigned int kernelWidth,
               unsigned int kernelHeight,
               unsigned int mipmapFilter,
               const char* testName)
        : UnitTest(graphicsBackend, config, testName)
    {
        m_generateMipmaps = true;
        m_textureType = textureType;
        m_colorAttachmentFormat = format;
        m_kernelSize = { kernelWidth, kernelHeight };
        m_mipmapFilter = mipmapFilter;
    }

protected:
    qhwi::ResultCode init() override
    {
        UnitTest::init();

        // Init input texture
        initTextureWithProceduralData();

        // Init output texture
        qhwi::TextureDesc outputTextureDesc = qhwi::TextureDesc()
                                                  .setWidth(m_config.w)
                                                  .setHeight(m_config.h)
                                                  .setFormat(m_colorAttachmentFormat)
                                                  .setMipLevels(0)
                                                  .setUsage(qhwi::TextureUsage_ColorAttachment | qhwi::TextureUsage_Sampled | qhwi::TextureUsage_Storage)
                                                  .setName("outputTexture")
                                                  .setArrayLayers(m_textureType == qhwi::TextureType::eCube ? 6 : 1)
                                                  .setAutomaticBarriers(true)
                                                  .setType(m_textureType);

        m_outputTexture.init(m_device.Get(), outputTextureDesc, nullptr, true);

        // Init draw texture task
        m_drawTextureTask.init({ .device = m_device.Get(),
                                 .texture = &m_patternTexture,
                                 .posData = {},
                                 .numVerts = {},
                                 .indicesData = {},
                                 .numIndices = {},
                                 .uvData = {},
                                 .samplerDesc = {},
                                 .colorAttachmentFormat = m_colorAttachmentFormat,
                                 .depthStencilFormat = m_depthStencilAttachmentFormat,
                                 .subresource = {} });

        m_sampleCubeTask.init({ .device = m_device.Get(),
                                .texture = &m_outputTexture,
                                .posData = vertices,
                                .numVerts = std::size(vertices),
                                .indicesData = {},
                                .numIndices = {},
                                .uvwData = uvwData,
                                .samplerDesc = {},
                                .colorAttachmentFormat = m_colorAttachmentFormat,
                                .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
        m_sampleCubeTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        // Init blit task
        m_blitTask.init({ .device = m_device.Get(),
                          .texture = &m_patternTexture,
                          .posData = {},
                          .numVerts = {},
                          .indicesData = {},
                          .numIndices = {},
                          .uvData = {},
                          .samplerDesc = {},
                          .colorAttachmentFormat = m_colorAttachmentFormat,
                          .depthStencilFormat = m_depthStencilAttachmentFormat,
                          .subresource = {} });

        beginTransfer();
        m_blitTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        // Initialize qhwi mipmap generator
        m_mipmapGenerator = qre::MakeRefCountPtr<qhwi::GenerateMipmapsOp>(new qhwi::GenerateMipmapsOp(m_device.Get()));
        m_mipmapGenerator->setKernelSize(m_kernelSize.first, m_kernelSize.second);
        m_mipmapGenerator->setMipmapFilter(m_mipmapFilter);

        return qhwi::ResultCode::eSuccess;
    }

    void initTextureWithProceduralData()
    {
        // Generate test data algorithmically
        const auto& w = m_config.w;
        const auto& h = m_config.h;

        constexpr uint32_t numGridX = 16;
        constexpr uint32_t numGridY = 16;
        const uint32_t pixelsPerGridX = w / numGridX;
        const uint32_t pixelsPerGridY = h / numGridY;

        const unsigned int numChannels = qhwi::FormatUtil::getNumChannels(m_colorAttachmentFormat);

        std::vector<uint8_t> dataProcedural(w * h * numChannels);
        const float cx = 0.5f * static_cast<float>(w) - 0.5f;
        const float cy = 0.5f * static_cast<float>(h) - 0.5f;
        const float rScale = 1.0f / static_cast<float>(w);
        for (uint32_t j = 0; j < h; ++j)
        {
            const float dy = static_cast<float>(j) - cy;
            for (uint32_t i = 0; i < w; ++i)
            {
                if (i < (w >> 1))
                {
                    const auto gridIndexY = j / pixelsPerGridY;
                    const auto gridIndexX = i / pixelsPerGridX;

                    const auto linearGridIndex = (gridIndexY * (numGridX >> 1) + gridIndexX);

                    for (uint32_t k = 0; k < numChannels; ++k)
                    {
                        const uint32_t fullColor = g_rgba8SrcPattern[linearGridIndex % patternSizeInPixels];
                        const uint8_t val = (fullColor >> (k * 8)) & 0xff;
                        dataProcedural[(j * w + i) * numChannels + k] = val;
                    }
                }
                else
                {
                    const float dx = static_cast<float>(i) - cx;
                    const float r2 = (dx * dx + dy * dy);
                    const float c = 0.5f + 0.5f * sinf(rScale * r2);
                    const auto u = static_cast<uint8_t>(c * 255.0f);

                    for (uint32_t k = 0; k < numChannels; ++k)
                    {
                        dataProcedural[(j * w + i) * numChannels + k] = u;
                    }
                }
            }
        }

        // Init pattern texture
        const qhwi::TextureDesc patternTextureDesc =
            qhwi::TextureDesc().setWidth(w).setHeight(h).setFormat(m_colorAttachmentFormat).setName("patternTexture").setAutomaticBarriers(true);

        if (m_colorAttachmentFormat == qhwi::Format::eR32Float || m_colorAttachmentFormat == qhwi::Format::eRg32Float || m_colorAttachmentFormat == qhwi::Format::eRgba32Float)
        {
            std::vector<float> floatDataProcedural(w * h * numChannels);
            for (unsigned int i = 0; i < floatDataProcedural.size(); ++i)
            {
                floatDataProcedural[i] = static_cast<float>(dataProcedural[i]) / 255.0f;
            }

            if (m_colorAttachmentFormat == qhwi::Format::eRgba32Float)
            {
                // .hdr format does not support transparency when writing / reading these textures to a file
                for (unsigned int i = 3; i < floatDataProcedural.size(); i += 4)
                {
                    floatDataProcedural[i] = 1.0f;
                }
            }
            m_patternTexture.init(m_device.Get(), patternTextureDesc, floatDataProcedural.data());
        }
        else
        {
            m_patternTexture.init(m_device.Get(), patternTextureDesc, dataProcedural.data());
        }
    }

    void blitPatternToMip0(qhwi::CommandList* commandList, qhwi::Texture* texture)
    {
        uint32_t numLayers = texture->desc()->type == qhwi::TextureType::eCube ? 6 : 1;

        for (uint32_t arrayLayer = 0; arrayLayer < numLayers; ++arrayLayer)
        {
            qhwi::ColorAttachmentDescription colorAttachmentDescription{};
            colorAttachmentDescription = qhwi::ColorAttachmentDescription()
                                             .setTexture(texture, { .mipSlice = 0, .arraySlice = static_cast<uint16_t>(arrayLayer) })
                                             .setLoadOp(qhwi::AttachmentLoadOp::eClear)
                                             .setStoreOp(qhwi::AttachmentStoreOp::eStore);

            commandList->beginRenderPass(1, &colorAttachmentDescription);
            commandList->setViewport({ .w = static_cast<float>(m_config.w), .h = static_cast<float>(m_config.h) });
            commandList->setScissor({ .w = m_config.w, .h = m_config.h });
            m_blitTask.draw(commandList);
            commandList->endRenderPass();
        }
    }

    void generateMipmaps(qhwi::CommandList* commandList) { m_mipmapGenerator->setTexture(m_outputTexture.get())->run(commandList); }

    void recordCommandList(qhwi::CommandList* commandList) override
    {
        commandList->begin();

        if (m_generateMipmaps)
        {
            blitPatternToMip0(commandList, m_outputTexture.get());
            generateMipmaps(commandList);
            m_generateMipmaps = false;
        }

        beginRenderPass(commandList);
        if (m_textureType == qhwi::TextureType::eCube)
        {
            m_sampleCubeTask.draw(commandList);
        }
        else
        {
            m_drawTextureTask.draw(commandList);
        }
        endRenderPass(commandList);
        commandList->end();
    }

    static std::string getStorageExtension(const unsigned int bytesPerChannel)
    {
        switch (bytesPerChannel)
        {
        case 1:
            return "png";
        case 4:
            return "hdr";
        default:
            QHWI_ASSERT_MSG(false, "Storage format not specified for this block size.");
            return "";
        }
    }

    void storeMipLevelAsImage(uint8_t* data,
                              const uint16_t level,
                              const unsigned int mipWidth,
                              const unsigned int mipHeight,
                              const uint32_t rowPitch,
                              const unsigned int numChannels,
                              const unsigned int bytesPerChannel) const
    {
        char filename[QRE_MAX_PATH];
        qre::PrintfSafe(filename, "%s/%s/level_%02d.%s", QHWI_TEST_REF_DATA_DIR, m_name, level, getStorageExtension(bytesPerChannel).c_str());

        qhwi::ext::ImageUtil::writePng(filename,
                                       { .w = static_cast<int>(mipWidth),
                                         .h = static_cast<int>(mipHeight),
                                         .format = m_colorAttachmentFormat,
                                         .numChannels = static_cast<int>(numChannels),
                                         .data = data,
                                         .pitch = static_cast<int>(rowPitch) });
    }

    qhwi::ResultCode compareOutputImage() override
    {
        beginTransfer();
        m_outputTexture.downloadToReadbackBuffer(m_copyCommandList);
        endTransfer();

        test_util::ScopedBufferMap scopedDataMap(m_outputTexture.readbackBuffer.get());

        uint8_t* data = { static_cast<uint8_t*>(scopedDataMap.map()) };
        const uint32_t mipLevels = m_outputTexture.get()->desc()->mipLevels;

        const unsigned int numChannels = qhwi::FormatUtil::getNumChannels(m_colorAttachmentFormat);
        const unsigned int bytesPerChannel = qhwi::FormatUtil::getFormatInfo(m_colorAttachmentFormat).bytesPerBlock / numChannels;

        bool result = true;

        const auto& textureDesc = m_outputTexture.get()->desc();
        const auto w = textureDesc->w;
        const auto h = textureDesc->h;
        uint64_t offset = 0;

        // Store mip info to disk
        if (m_config.genRefs)
        {
            for (uint16_t level = 0; level < mipLevels; ++level)
            {
                const auto mipWidth = std::max(w >> level, 1u);
                const auto mipHeight = std::max(h >> level, 1u);
                const auto rowPitch = m_outputTexture.get()->pitch(level);

                storeMipLevelAsImage(data + offset, level, mipWidth, mipHeight, rowPitch, numChannels, bytesPerChannel);

                offset += mipHeight * rowPitch;
            }
        }
        else
        {
            for (uint16_t level = 0; level < mipLevels; ++level)
            {
                const auto mipWidth = std::max(w >> level, 1u);
                const auto mipHeight = std::max(h >> level, 1u);
                const auto rowPitch = m_outputTexture.get()->pitch(level);

                char refFilePath[QRE_MAX_PATH];
                qre::PrintfSafe(refFilePath, "%s/%s/level_%02d.%s", QHWI_TEST_REF_DATA_DIR, m_name, level, getStorageExtension(bytesPerChannel).c_str());

                qhwi::ImageData refImageData;
                if (qhwi::ext::ImageUtil::readImage(refFilePath, refImageData, numChannels, bytesPerChannel) == qhwi::ext::ImageUtil::ResultCode::eFailed)
                {
                    std::cerr << "Failed to read ref image from " << refFilePath << std::endl;
                    result = false;
                    offset += mipHeight * rowPitch;
                    continue;
                }

                // Due to accumulated floating point precision error at conversion of textures, we need a slightly higher threshold.
                bool imageCompareRet;
                if (m_colorAttachmentFormat == qhwi::Format::eR8Unorm || m_colorAttachmentFormat == qhwi::Format::eRg8Unorm || m_colorAttachmentFormat == qhwi::Format::eRgba8Unorm)
                {
                    imageCompareRet = test_util::imageCompare<uint8_t>(data + offset,
                                                                       static_cast<int>(mipWidth),
                                                                       static_cast<int>(mipHeight),
                                                                       refImageData.data,
                                                                       static_cast<int>(rowPitch),
                                                                       static_cast<int>(qhwi::FormatUtil::getRowPitch(refImageData.getFormat(), refImageData.w)),
                                                                       1,
                                                                       true,
                                                                       m_colorAttachmentFormat,
                                                                       static_cast<uint8_t>(2));
                }
                else
                {
                    imageCompareRet = test_util::imageCompare<float>(data + offset,
                                                                     static_cast<int>(mipWidth),
                                                                     static_cast<int>(mipHeight),
                                                                     refImageData.data,
                                                                     static_cast<int>(rowPitch),
                                                                     static_cast<int>(qhwi::FormatUtil::getRowPitch(refImageData.getFormat(), refImageData.w)),
                                                                     1,
                                                                     true,
                                                                     m_colorAttachmentFormat,
                                                                     2.0f / 255.0f);
                }
                if (!imageCompareRet)
                {
                    // imageCompare only specifies coordinates of failed pixel, not the level, so adding additional info:
                    std::cerr << "    Mipmap level " << level << " verification failed.\n\n";
                    result = false;
                }

                offset += mipHeight * rowPitch;
            }
        }

        return result ? qhwi::ResultCode::eSuccess : qhwi::ResultCode::eErrorOther;
    }

    test_util::Texture1 m_patternTexture;
    test_util::Texture1 m_outputTexture;
    test_util::DrawTextureTask2D m_drawTextureTask;
    test_util::DrawTextureTask2D m_blitTask;
    test_util::SampleTextureCubeTask m_sampleCubeTask;
    bool m_generateMipmaps;
    std::pair<unsigned int, unsigned int> m_kernelSize;
    unsigned int m_mipmapFilter;
    qhwi::TextureType m_textureType;
    qre::RefCountPtr<qhwi::GenerateMipmapsOp> m_mipmapGenerator;

    static constexpr uint32_t s_numGroupSizeX = 8, s_numGroupSizeY = 8;
};

#define CREATE_MULTI_PASS_MIPMAP_TEST(name, format, kernelWidth, kernelHeight, mipmapFilter)                                                                                       \
    UnitTest* create_test_mipmap_multi_pass_##name(qhwi::GraphicsBackend backend, UnitTest::Config config)                                                                         \
    {                                                                                                                                                                              \
        return new TestMipmap(backend, config, qhwi::TextureType::e2D, qhwi::Format::format, kernelWidth, kernelHeight, mipmapFilter, "test_mipmap_multi_pass_" #name);            \
    }
// linear, 2x2 kernel tests
CREATE_MULTI_PASS_MIPMAP_TEST(r8unorm_2x2_linear, eR8Unorm, 2, 2, 1)
CREATE_MULTI_PASS_MIPMAP_TEST(rg8unorm_2x2_linear, eRg8Unorm, 2, 2, 1)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_2x2_linear, eRgba8Unorm, 2, 2, 1)
CREATE_MULTI_PASS_MIPMAP_TEST(r32float_2x2_linear, eR32Float, 2, 2, 1)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba32float_2x2_linear, eRgba32Float, 2, 2, 1)

// nearest tests
CREATE_MULTI_PASS_MIPMAP_TEST(r8unorm_2x2_nearest, eR8Unorm, 2, 2, 0)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_2x2_nearest, eRgba8Unorm, 2, 2, 0)

// min tests
CREATE_MULTI_PASS_MIPMAP_TEST(r8unorm_2x2_min, eR8Unorm, 2, 2, 2)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_2x2_min, eRgba8Unorm, 2, 2, 2)
CREATE_MULTI_PASS_MIPMAP_TEST(r32float_2x2_min, eR32Float, 2, 2, 2)

// max tests
CREATE_MULTI_PASS_MIPMAP_TEST(r8unorm_2x2_max, eR8Unorm, 2, 2, 3)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_2x2_max, eRgba8Unorm, 2, 2, 3)
CREATE_MULTI_PASS_MIPMAP_TEST(r32float_2x2_max, eR32Float, 2, 2, 3)

// alternate kernel size tests
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_3x3_linear, eRgba8Unorm, 3, 3, 1)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_5x2_linear, eRgba8Unorm, 5, 2, 1)
CREATE_MULTI_PASS_MIPMAP_TEST(rgba8unorm_3x6_linear, eRgba8Unorm, 3, 6, 1)

#define CREATE_SINGLE_PASS_MIPMAP_TEST(name, format)                                                                                                                               \
    UnitTest* create_test_mipmap_single_pass_##name(qhwi::GraphicsBackend backend, UnitTest::Config config)                                                                        \
    {                                                                                                                                                                              \
        config.w = 512;                                                                                                                                                            \
        config.h = 512;                                                                                                                                                            \
        return new TestMipmap(backend, config, qhwi::TextureType::e2D, qhwi::Format::format, 2, 2, 1, "test_mipmap_single_pass_" #name);                                           \
    }

CREATE_SINGLE_PASS_MIPMAP_TEST(r8unorm, eR8Unorm)
CREATE_SINGLE_PASS_MIPMAP_TEST(rg8unorm, eRg8Unorm)
CREATE_SINGLE_PASS_MIPMAP_TEST(rgba8unorm, eRgba8Unorm)
CREATE_SINGLE_PASS_MIPMAP_TEST(r32float, eR32Float)
CREATE_SINGLE_PASS_MIPMAP_TEST(rgba32float, eRgba32Float)

UnitTest* create_test_mipmap_cube(qhwi::GraphicsBackend backend, UnitTest::Config config)
{
    return new TestMipmap(backend, config, qhwi::TextureType::eCube, qhwi::Format::eRgba8Unorm, 2, 2, 1, "test_mipmap_cube");
}