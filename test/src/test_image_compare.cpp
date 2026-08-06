//
//
//  test_mipmap.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "unit_test.h"
#include "test_util.h"

#include <array>
#include <random>

static unsigned int getRandomIndex(const unsigned int max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, max);
    return dist(gen);
}

constexpr unsigned int g_testImageWidth = 6;
constexpr unsigned int g_testImageHeight = 5;

constexpr std::array<std::array<uint8_t, 4>, g_testImageWidth * g_testImageHeight> g_refData_uint8_t = {
    { { 57, 12, 140, 125 },   { 114, 71, 52, 44 },    { 216, 16, 15, 47 },    { 111, 119, 13, 101 },  { 214, 112, 229, 142 }, { 3, 81, 216, 142 },
      { 186, 127, 186, 221 }, { 183, 192, 204, 183 }, { 36, 174, 90, 186 },   { 69, 208, 189, 107 },  { 112, 87, 230, 246 },  { 244, 214, 207, 244 },
      { 183, 111, 71, 144 },  { 71, 48, 128, 228 },   { 180, 219, 112, 87 },  { 70, 130, 49, 52 },    { 44, 216, 16, 15 },    { 47, 111, 119, 13 },
      { 101, 214, 112, 229 }, { 142, 3, 81, 216 },    { 142, 186, 127, 186 }, { 221, 183, 192, 204 }, { 183, 36, 174, 90 },   { 186, 69, 208, 189 },
      { 107, 112, 87, 230 },  { 246, 244, 214, 207 }, { 244, 183, 111, 71 },  { 144, 71, 48, 128 },   { 228, 180, 219, 112 }, { 87, 70, 130, 49 } }
};

constexpr std::array<std::array<float, 4>, g_testImageWidth * g_testImageHeight> g_refData_float = { {
    { 0.109f, 0.056f, 0.787f, 0.314f }, { 0.820f, 0.582f, 0.700f, 0.035f }, { 0.625f, 0.992f, 0.399f, 0.045f }, { 0.515f, 0.798f, 0.129f, 0.746f },
    { 0.602f, 0.095f, 0.266f, 0.338f }, { 0.285f, 0.537f, 0.259f, 0.077f }, { 0.140f, 0.929f, 0.110f, 0.121f }, { 0.972f, 0.955f, 0.250f, 0.476f },
    { 0.580f, 0.893f, 0.948f, 0.533f }, { 0.210f, 0.342f, 0.873f, 0.603f }, { 0.060f, 0.317f, 0.383f, 0.519f }, { 0.739f, 0.769f, 0.962f, 0.539f },
    { 0.387f, 0.237f, 0.816f, 0.953f }, { 0.139f, 0.360f, 0.512f, 0.465f }, { 0.853f, 0.773f, 0.496f, 0.166f }, { 0.032f, 0.618f, 0.695f, 0.457f },
    { 0.815f, 0.710f, 0.862f, 0.413f }, { 0.160f, 0.951f, 0.396f, 0.473f }, { 0.424f, 0.103f, 0.712f, 0.569f }, { 0.418f, 0.839f, 0.968f, 0.169f },
    { 0.264f, 0.501f, 0.273f, 0.619f }, { 0.177f, 0.463f, 0.234f, 0.386f }, { 0.480f, 0.330f, 0.279f, 0.410f }, { 0.925f, 0.966f, 0.466f, 0.419f },
    { 0.564f, 0.959f, 0.544f, 0.751f }, { 0.717f, 0.894f, 0.442f, 0.872f }, { 0.575f, 0.106f, 0.068f, 0.732f }, { 0.165f, 0.158f, 0.092f, 0.871f },
    { 0.877f, 0.300f, 0.601f, 0.238f }, { 0.716f, 0.621f, 0.051f, 0.942f },
} };

template <typename T, const unsigned int numChannels>
constexpr std::vector<T> getFilteredTestData()
{
    std::array<std::array<T, 4>, g_testImageWidth * g_testImageHeight> refData{};
    if constexpr (std::is_same_v<T, uint8_t>)
    {
        refData = g_refData_uint8_t;
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        refData = g_refData_float;
    }

    std::vector<T> filteredData;
    filteredData.resize(numChannels * g_testImageWidth * g_testImageHeight);
    for (unsigned int i = 0; i < g_testImageWidth * g_testImageHeight; i++)
    {
        const auto& refColor = refData[i];
        for (unsigned int j = 0; j < numChannels; j++)
        {
            filteredData[i * numChannels + j] = refColor[j];
        }
    }
    return filteredData;
}

template <qhwi::Format F>
struct FormatTraits;

template <>
struct FormatTraits<qhwi::Format::eR8Unorm>
{
    using Type = uint8_t;
    static constexpr unsigned int numChannels = 1;
    static constexpr Type epsilon = 2;
};

template <>
struct FormatTraits<qhwi::Format::eRg8Unorm>
{
    using Type = uint8_t;
    static constexpr unsigned int numChannels = 2;
    static constexpr Type epsilon = 2;
};

template <>
struct FormatTraits<qhwi::Format::eRgba8Unorm>
{
    using Type = uint8_t;
    static constexpr unsigned int numChannels = 4;
    static constexpr Type epsilon = 2;
};

template <>
struct FormatTraits<qhwi::Format::eR32Float>
{
    using Type = float;
    static constexpr unsigned int numChannels = 1;
    static constexpr Type epsilon = 1.0f / 255.0f;
};

template <>
struct FormatTraits<qhwi::Format::eRg32Float>
{
    using Type = float;
    static constexpr unsigned int numChannels = 2;
    static constexpr Type epsilon = 1.0f / 255.0f;
};

template <>
struct FormatTraits<qhwi::Format::eRgba32Float>
{
    using Type = float;
    static constexpr unsigned int numChannels = 4;
    static constexpr Type epsilon = 1.0f / 255.0f;
};

template <qhwi::Format F>
std::vector<typename FormatTraits<F>::Type> getRefImage()
{
    return getFilteredTestData<typename FormatTraits<F>::Type, FormatTraits<F>::numChannels>();
}

template <qhwi::Format F>
class TestImageCompare final : public UnitTest
{
public:
    TestImageCompare(qhwi::GraphicsBackend graphicsBackend, Config config, const char* testName)
        : UnitTest(graphicsBackend, config, testName)
    {
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        auto simRefData = getRefImage<F>();
        auto simSrcData = getRefImage<F>();

        const int pitch = qhwi::FormatUtil::getRowPitch(F, g_testImageWidth);

        if (!test_util::imageCompare(simSrcData.data(), g_testImageWidth, g_testImageHeight, simRefData.data(), pitch, pitch, 1, true, F))
        {
            return qhwi::ResultCode::eErrorOther;
        }

        // Corrupt a random pixel of src data to trigger failure:
        const unsigned int numChannels = FormatTraits<F>::numChannels;
        const unsigned int randomIndex = getRandomIndex(numChannels * g_testImageWidth * g_testImageHeight - 2 * numChannels);
        for (unsigned int i = 0; i < numChannels; i++)
        {
            simSrcData[randomIndex + i] += FormatTraits<F>::epsilon * 2;
        }

        // Expect Failure
        if (test_util::imageCompare(simSrcData.data(), g_testImageWidth, g_testImageHeight, simRefData.data(), pitch, pitch, 0, false, F))
        {
            return qhwi::ResultCode::eErrorOther;
        }

        return qhwi::ResultCode::eSuccess;
    }

protected:
    qhwi::ResultCode init() override
    {
        m_isInitialized = true;
        return qhwi::ResultCode::eSuccess;
    }
};

#define CREATE_IMAGE_COMPARE_TEST(format)                                                                                                                                          \
    UnitTest* create_test_image_compare_##format(qhwi::GraphicsBackend backend, UnitTest::Config config)                                                                           \
    {                                                                                                                                                                              \
        config.outputType = UnitTest::OutputType::eBuffer;                                                                                                                         \
        return new TestImageCompare<qhwi::Format::format>(backend, config, "test_image_compare_" #format);                                                                         \
    }

CREATE_IMAGE_COMPARE_TEST(eR8Unorm)
CREATE_IMAGE_COMPARE_TEST(eRg8Unorm)
CREATE_IMAGE_COMPARE_TEST(eRgba8Unorm)
CREATE_IMAGE_COMPARE_TEST(eR32Float)
CREATE_IMAGE_COMPARE_TEST(eRg32Float)
CREATE_IMAGE_COMPARE_TEST(eRgba32Float)
