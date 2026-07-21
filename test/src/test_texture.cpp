//
//
//  test_texture.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

static const uint32_t g_srcRgba8UnormData[] = { 0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF, 0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF,
                                                0xFF888888, 0xFF8888FF, 0xFF88FF88, 0xFF88FFFF, 0xFFFF8888, 0xFFFF88FF, 0xFFFFFF88, 0xFFFFFFFF };

static const float4 g_srcRgba32FloatData[] = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f },
                                               { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },
                                               { 0.5f, 0.5f, 0.5f, 1.0f }, { 1.0f, 0.5f, 0.5f, 1.0f }, { 0.5f, 1.0f, 0.5f, 1.0f }, { 1.0f, 1.0f, 0.5f, 1.0f },
                                               { 0.5f, 0.5f, 1.0f, 1.0f }, { 1.0f, 0.5f, 1.0f, 1.0f }, { 0.5f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } };

static const half4 g_srcRgba16FloatData[] = { { 0, 0, 0, 15360 },
                                              { 15360, 0, 0, 15360 },
                                              { 0, 15360, 0, 15360 },
                                              { 15360, 15360, 0, 15360 },
                                              { 0, 0, 15360, 15360 },
                                              { 15360, 0, 15360, 15360 },
                                              { 0, 15360, 15360, 15360 },
                                              { 15360, 15360, 15360, 15360 },
                                              { 14336, 14336, 14336, 15360 },
                                              { 15360, 14336, 14336, 15360 },
                                              { 14336, 15360, 14336, 15360 },
                                              { 15360, 15360, 14336, 15360 },
                                              { 14336, 14336, 15360, 15360 },
                                              { 15360, 14336, 15360, 15360 },
                                              { 14336, 15360, 15360, 15360 },
                                              { 15360, 15360, 15360, 15360 } };

static const float2 g_srcRg32FloatData[] = { { 0.0f, 0.0f }, { 0.0f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.5f }, { 0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.7f, 0.7f },
                                             { 0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.7f, 0.7f }, { 0.0f, 0.5f }, { 1.0f, 1.0f }, { 0.7f, 0.7f }, { 0.0f, 0.5f }, { 0.5f, 0.0f } };

static const half2 g_srcRg16FloatData[] = { { 0, 0 },         { 0, 14336 },     { 14336, 0 }, { 15360, 15360 }, { 0, 14336 },     { 14336, 0 },
                                            { 15360, 15360 }, { 14746, 14746 }, { 14336, 0 }, { 15360, 15360 }, { 14746, 14746 }, { 0, 14336 },
                                            { 15360, 15360 }, { 14746, 14746 }, { 0, 14336 }, { 14336, 0 } };

#define RGBA10A2(rgba8)                                                                                                                                                            \
    (uint32_t((rgba8 & 0xff) * 1023.0 / 255.0) & 0x3ff) | ((uint32_t(((rgba8 >> 8) & 0xff) * 1023.0 / 255.0) & 0x3ff) << 10) |                                                     \
        ((uint32_t(((rgba8 >> 16) & 0xff) * 1023.0 / 255.0) & 0x3ff) << 20) | ((uint32_t(((rgba8 >> 24) & 0xff) * 1023.0 / 3.0) & 0x3) << 30)

static const uint32_t g_srcRgb10A2UnormData[] = { RGBA10A2(0xFF000000), RGBA10A2(0xFF0000FF), RGBA10A2(0xFF00FF00), RGBA10A2(0xFF00FFFF),
                                                  RGBA10A2(0xFFFF0000), RGBA10A2(0xFFFF00FF), RGBA10A2(0xFFFFFF00), RGBA10A2(0xFFFFFFFF),
                                                  RGBA10A2(0xFF888888), RGBA10A2(0xFF8888FF), RGBA10A2(0xFF88FF88), RGBA10A2(0xFF88FFFF),
                                                  RGBA10A2(0xFFFF8888), RGBA10A2(0xFFFF88FF), RGBA10A2(0xFFFFFF88), RGBA10A2(0xFFFFFFFF) };

static const uint32_t g_srcRgba8UnormLayersData[] = { 0xFF111111, 0xFF222222, 0xFF333333, 0xFF444444, 0xFF555555, 0xFF666666, 0xFF777777, 0xFF888888,
                                                      0xFF999999, 0xFFAAAAAA, 0xFFBBBBBB, 0xFFCCCCCC, 0xFFDDDDDD, 0xFFEEEEEE, 0xFFFFFFFF, 0xFFEEEEEE,

                                                      0xFFFF0000, 0xFFFF1111, 0xFFFF2222, 0xFFFF3333, 0xFFFF4444, 0xFFFF5555, 0xFFFF6666, 0xFFFF7777,
                                                      0xFFFF8888, 0xFFFF9999, 0xFFFFAAAA, 0xFFFFBBBB, 0xFFFFCCCC, 0xFFFFDDDD, 0xFFFFEEEE, 0xFFFFFFFF,

                                                      0xFF222222, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF, 0xFFFF0000, 0xFFFF00FF, 0xFFFFEE00, 0xFFEE88FF,
                                                      0xFF888888, 0xFF8888FF, 0xFF88FF88, 0xFF88FFFF, 0xFFFF8888, 0xFFFF88FF, 0xFFFFFF88, 0xFFEEEEEE };

static const uint32_t g_srcRgba8UnormCubeLayersData[] = {
    0xFF111111, 0xFF222222, 0xFF333333, 0xFF444444, 0xFF555555, 0xFF666666, 0xFF777777, 0xFF888888,
    0xFF999999, 0xFFAAAAAA, 0xFFBBBBBB, 0xFFCCCCCC, 0xFFDDDDDD, 0xFFEEEEEE, 0xFFFFFFFF, 0xFFEEEEEE,

    0xFFFF0000, 0xFFFF1111, 0xFFFF2222, 0xFFFF3333, 0xFFFF4444, 0xFFFF5555, 0xFFFF6666, 0xFFFF7777,
    0xFFFF8888, 0xFFFF9999, 0xFFFFAAAA, 0xFFFFBBBB, 0xFFFFCCCC, 0xFFFFDDDD, 0xFFFFEEEE, 0xFFFFFFFF,

    0xFF222222, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF, 0xFFFF0000, 0xFFFF00FF, 0xFFFFEE00, 0xFFEE88FF,
    0xFF888888, 0xFF8888FF, 0xFF88FF88, 0xFF88FFFF, 0xFFFF8888, 0xFFFF88FF, 0xFFFFFF88, 0xFFEEEEEE,

    0xFF111111, 0xFF222222, 0xFF333333, 0xFF444444, 0xFF555555, 0xFF666666, 0xFF777777, 0xFF888888,
    0xFF999999, 0xFFAAAAAA, 0xFFBBBBBB, 0xFFCCCCCC, 0xFFDDDDDD, 0xFFEEEEEE, 0xFFFFFFFF, 0xFFEEEEEE,

    0xFFFF0000, 0xFFFF1111, 0xFFFF2222, 0xFFFF3333, 0xFFFF4444, 0xFFFF5555, 0xFFFF6666, 0xFFFF7777,
    0xFFFF8888, 0xFFFF9999, 0xFFFFAAAA, 0xFFFFBBBB, 0xFFFFCCCC, 0xFFFFDDDD, 0xFFFFEEEE, 0xFFFFFFFF,

    0xFF222222, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF, 0xFFFF0000, 0xFFFF00FF, 0xFFFFEE00, 0xFFEE88FF,
    0xFF888888, 0xFF8888FF, 0xFF88FF88, 0xFF88FFFF, 0xFFFF8888, 0xFFFF88FF, 0xFFFFFF88, 0xFFEEEEEE,
};

class TestTexture2D : public UnitTest
{
public:
    TestTexture2D(GraphicsBackend graphicsBackend, Config config, const char* testName, Format format, const void* srcData)
        : UnitTest(graphicsBackend, config, testName)
    {
        m_pSrcData = srcData;
        m_format = format;
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        const uint32_t w = 4, h = 4;

        m_texture.init(m_device.Get(),
                       { .heapFlags = {},
                         .w = w,
                         .h = h,
                         .d = 1,
                         .mipLevels = 1,
                         .arrayLayers = 1,
                         .numPlanes = 1,
                         .numSamples = SampleCount::e1,
                         .format = m_format,
                         .rSwizzle = {},
                         .gSwizzle = ComponentSwizzle::eG,
                         .bSwizzle = ComponentSwizzle::eB,
                         .aSwizzle = ComponentSwizzle::eA,
                         .usage = TextureUsage_Sampled,
                         .type = TextureType::e2D,
                         .name = "texture",
                         .automaticBarriers = true },
                       m_pSrcData);
        m_drawTextureTask.init({ .device = m_device.Get(),
                                 .texture = &m_texture,
                                 .colorAttachmentFormat = m_colorAttachmentFormat,
                                 .depthStencilFormat = m_depthStencilAttachmentFormat,
                                 .subresource = {} });

        beginTransfer();
        m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawTextureTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    Texture1 m_texture;
    DrawTextureTask2D m_drawTextureTask;
    const void* m_pSrcData;
    Format m_format;
};

class TestTexture3D : public UnitTest
{
public:
    TestTexture3D(GraphicsBackend graphicsBackend, Config config, const char* testName, Format format, TextureType textureType, const void* srcData)
        : UnitTest(graphicsBackend, config, testName)
    {
        m_config.depthStencilType = DepthStencilType::eDepthOnly;
        m_pSrcData = srcData;
        m_format = format;
        m_textureType = textureType;
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        const uint32_t w = 4, h = 4, depthOrArrayLayers = 3;
        uint32_t d = 1;
        uint32_t arrayLayers = 1;

        const char* vertexShader;
        const char* pixelShader;

        switch (m_textureType)
        {
        case TextureType::e2DArray:
            arrayLayers = depthOrArrayLayers;
            vertexShader = "testTexture2dArray";
            pixelShader = "testTexture2dArray";
            break;
        case TextureType::e3D:
            d = depthOrArrayLayers;
            vertexShader = "testTexture3d";
            pixelShader = "testTexture3d";
            break;
        case TextureType::eCube:
            arrayLayers = 6;
            vertexShader = "testTextureCube";
            pixelShader = "testTextureCube";
            break;
        default:
            QHWI_FATAL("Invalid texture type");
            break;
        }

        m_texture.init(m_device.Get(),
                       { .heapFlags = {},
                         .w = w,
                         .h = h,
                         .d = d,
                         .mipLevels = 1,
                         .arrayLayers = arrayLayers,
                         .numPlanes = 1,
                         .numSamples = SampleCount::e1,
                         .format = m_format,
                         .rSwizzle = {},
                         .gSwizzle = ComponentSwizzle::eG,
                         .bSwizzle = ComponentSwizzle::eB,
                         .aSwizzle = ComponentSwizzle::eA,
                         .usage = TextureUsage_Sampled,
                         .type = m_textureType,
                         .name = "texture",
                         .automaticBarriers = true },
                       m_pSrcData);

        float3 vertexPos[] = { // Front face
                               { -0.25f, -0.25f, 0.45f },
                               { -0.25f, 0.25f, 0.45f },
                               { 0.25f, 0.25f, 0.45f },
                               { 0.25f, -0.25f, 0.45f },

                               // Top face
                               { -0.25f, 0.25f, 0.45f },
                               { -0.25f, 0.25f, 1.000005f },
                               { 0.25f, 0.25f, 1.000005f },
                               { 0.25f, 0.25f, 0.45f },

                               // Right face
                               { 0.25f, -0.25f, 0.45f },
                               { 0.25f, 0.25f, 0.45f },
                               { 0.25f, 0.25f, 1.000005f },
                               { 0.25f, -0.25f, 1.000005f },

                               // Left face
                               { -0.25f, -0.25f, 1.000005f },
                               { -0.25f, 0.25f, 1.000005f },
                               { -0.25f, 0.25f, 0.45f },
                               { -0.25f, -0.25f, 0.45f },

                               // Bottom face
                               { -0.25f, -0.25f, 1.000005f },
                               { -0.25f, -0.25f, 0.45f },
                               { 0.25f, -0.25f, 0.45f },
                               { 0.25f, -0.25f, 1.000005f },

                               // Back face
                               { 0.25f, -0.25f, 1.000005f },
                               { 0.25f, 0.25f, 1.000005f },
                               { -0.25f, 0.25f, 1.000005f },
                               { -0.25f, -0.25f, 1.000005f }
        };

        uint16_t indices[] = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23 };

        float textureW0;
        float textureW1;
        float textureW2;
        float textureW3;
        float textureW4;
        float textureW5;

        float2 topLeft;
        float2 topRight;
        float2 bottomLeft;
        float2 bottomRight;

        switch (m_textureType)
        {
        case TextureType::e2DArray:
            textureW0 = 0.0;
            textureW1 = 1.0;
            textureW2 = 2.0;
            textureW3 = 0.0;
            textureW4 = 0.0;
            textureW5 = 0.0;

            topLeft = { 0.0, 1.0 };
            topRight = { 1.0, 1.0 };
            bottomLeft = { 0.0, 0.0 };
            bottomRight = { 1.0, 0.0 };
            break;

        case TextureType::e3D:
            textureW0 = 0.0;
            textureW1 = 0.5;
            textureW2 = 1.0;
            textureW3 = 0.0;
            textureW4 = 0.0;
            textureW5 = 0.0;

            topLeft = { 0.0, 1.0 };
            topRight = { 1.0, 1.0 };
            bottomLeft = { 0.0, 0.0 };
            bottomRight = { 1.0, 0.0 };
            break;

        case TextureType::eCube:
            textureW0 = 1.0;
            textureW1 = 1.0;
            textureW2 = 1.0;
            textureW3 = 0.0;
            textureW4 = 0.0;
            textureW5 = 0.0;

            topLeft = { -1.0, 1.0 };
            topRight = { 1.0, 1.0 };
            bottomLeft = { -1.0, -1.0 };
            bottomRight = { 1.0, -1.0 };
            break;

        default:
            QHWI_FATAL("Invalid texture type");
            break;
        }

        float3 uvData[] = { { bottomLeft.x, bottomLeft.y, textureW2 },   { bottomRight.x, bottomRight.y, textureW2 }, { topRight.x, topRight.y, textureW2 },
                            { topLeft.x, topLeft.y, textureW2 },         { bottomLeft.x, bottomLeft.y, textureW3 },   { bottomRight.x, bottomRight.y, textureW3 },
                            { topRight.x, topRight.y, textureW3 },       { topLeft.x, topLeft.y, textureW3 },         { bottomLeft.x, bottomLeft.y, textureW1 },
                            { bottomRight.x, bottomRight.y, textureW1 }, { topRight.x, topRight.y, textureW1 },       { topLeft.x, topLeft.y, textureW1 },
                            { bottomLeft.x, bottomLeft.y, textureW4 },   { bottomRight.x, bottomRight.y, textureW4 }, { topRight.x, topRight.y, textureW4 },
                            { topLeft.x, topLeft.y, textureW4 },         { bottomLeft.x, bottomLeft.y, textureW0 },   { bottomRight.x, bottomRight.y, textureW0 },
                            { topRight.x, topRight.y, textureW0 },       { topLeft.x, topLeft.y, textureW0 },         { bottomLeft.x, bottomLeft.y, textureW5 },
                            { bottomRight.x, bottomRight.y, textureW5 }, { topRight.x, topRight.y, textureW5 },       { topLeft.x, topLeft.y, textureW5 } };

        float4x4 mat(std::array<float, 16>({ 0.7071068f,
                                             -0.5000000f,
                                             -0.5000000f,
                                             0.0f,
                                             0.7071068f,
                                             0.5000000f,
                                             0.5000000f,
                                             0.0f,
                                             -0.0000000f,
                                             -0.7071068f,
                                             0.7071068f,
                                             0.0f,
                                             0.0000000f,
                                             0.0000000f,
                                             0.0000000f,
                                             1.0f }));

        m_drawTextureTask.init({ .device = m_device.Get(),
                                 .texture = &m_texture,
                                 .posData = vertexPos,
                                 .numVerts = sizeof(vertexPos) / sizeof(vertexPos[0]),
                                 .indicesData = indices,
                                 .numIndices = sizeof(indices) / sizeof(indices[0]),
                                 .uvwData = uvData,
                                 .samplerDesc = { .minFilter = {},
                                                  .magFilter = {},
                                                  .mipFilter = {},
                                                  .addressModeU = SamplerAddressMode::eClampToEdge,
                                                  .addressModeV = SamplerAddressMode::eClampToEdge,
                                                  .addressModeW = SamplerAddressMode::eClampToEdge,
                                                  .mipLodBias = 0,
                                                  .minLod = 0,
                                                  .maxLod = 0,
                                                  .maxAnisotropy = 0,
                                                  .flags = {},
                                                  .reductionMode = {},
                                                  .compareOp = {} },
                                 .transform = &mat,
                                 .vertexShader = vertexShader,
                                 .pixelShader = pixelShader,
                                 .colorAttachmentFormat = m_colorAttachmentFormat,
                                 .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawTextureTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    Texture1 m_texture;
    DrawTextureTask3D m_drawTextureTask;
    const void* m_pSrcData;
    Format m_format;
    TextureType m_textureType;
};

#define CREATE_TEXTURE_2D_TEST(name, fmt, data)                                                                                                                                    \
    UnitTest* create_test_texture_##name(GraphicsBackend backend, UnitTest::Config config)                                                                                         \
    {                                                                                                                                                                              \
        return new TestTexture2D(backend, config, "test_texture_" #name, Format::fmt, data);                                                                                       \
    }

CREATE_TEXTURE_2D_TEST(2d, eRgba8Unorm, g_srcRgba8UnormData)
CREATE_TEXTURE_2D_TEST(format_rgba8srgb, eRgba8Srgb, g_srcRgba8UnormData)
CREATE_TEXTURE_2D_TEST(format_rgba32float, eRgba32Float, g_srcRgba32FloatData)
CREATE_TEXTURE_2D_TEST(format_rg32float, eRg32Float, g_srcRg32FloatData)
CREATE_TEXTURE_2D_TEST(format_rgb10a2unorm, eRgb10A2Unorm, g_srcRgb10A2UnormData)
CREATE_TEXTURE_2D_TEST(format_rgba16float, eRgba16Float, g_srcRgba16FloatData)
CREATE_TEXTURE_2D_TEST(format_rg16float, eRg16Float, g_srcRg16FloatData)

#define CREATE_TEXTURE_3D_TEST(name, fmt, type, data)                                                                                                                              \
    UnitTest* create_test_texture_##name(GraphicsBackend backend, UnitTest::Config config)                                                                                         \
    {                                                                                                                                                                              \
        return new TestTexture3D(backend, config, "test_texture_" #name, Format::fmt, TextureType::type, data);                                                                    \
    }

CREATE_TEXTURE_3D_TEST(3d, eRgba8Unorm, e3D, g_srcRgba8UnormLayersData)
CREATE_TEXTURE_3D_TEST(2darray, eRgba8Unorm, e2DArray, g_srcRgba8UnormLayersData)
CREATE_TEXTURE_3D_TEST(cube, eRgba8Unorm, eCube, g_srcRgba8UnormCubeLayersData)