//
//
//  qhwi_util.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_UTIL_H__
#define __QHWI_UTIL_H__

#include "qre/image.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#    include <Windows.h>
#else
#    include <dlfcn.h>
#endif
#include <qhwi/qhwi_enums.h>
#include <qhwi/qhwi_log_util.h>
#ifdef QHWI_PLATFORM_ANDROID
#    include <android/asset_manager.h>
#endif
#include <qhwi/qhwi.h>

namespace qhwi
{
struct FileData
{
    ~FileData()
    {
        if (data)
        {
            free(data);
        }
    }

    void* data = nullptr;
    size_t size = 0;
};

namespace FileUtil
{
enum class ResultCode : uint8_t
{
    eSuccess,
    eFileNotFound
};
enum class Flag : uint8_t
{
    eQuiet = 1
};
typedef uint8_t Flags;
ResultCode readFile(const char* path, FileData& data, Flags flags = 0);
ResultCode writeFile(const char* path, void* data, int size, Flags flags = 0);
#ifdef QHWI_PLATFORM_ANDROID
void setAndroidAssetManager(AAssetManager* aAssetManager);
ResultCode readAndroidAsset(const char* path, FileData& data, Flags flags = 0);
#endif
} // namespace FileUtil

struct ImageData
{
    ImageData() {}

    ImageData(int w, int h, int numChannels = 4, int bytesPerChannel = 1)
        : w(w)
        , h(h)
        , numChannels(numChannels)
        , bytesPerChannel(bytesPerChannel)
    {
        size = w * h * numChannels * bytesPerChannel;
        data = malloc(size);
    }

    ~ImageData()
    {
        if (data)
        {
            free(data);
        }
    }

    Format getFormat() const
    {
        switch (numChannels)
        {
        case 1:
        {
            if (bytesPerChannel == 1)
            {
                return Format::eR8Unorm;
            }
            if (bytesPerChannel == 4)
            {
                return Format::eR32Float;
            }

            return Format::eUnknown;
        }
        case 2:
        {
            if (bytesPerChannel == 1)
            {
                return Format::eRg8Unorm;
            }
            if (bytesPerChannel == 4)
            {
                return Format::eRg32Float;
            }

            return Format::eUnknown;
        }
        case 4:
        {
            if (bytesPerChannel == 1)
            {
                return Format::eRgba8Unorm;
            }
            if (bytesPerChannel == 4)
            {
                return Format::eRgba32Float;
            }

            return Format::eUnknown;
        }
        default:
            return Format::eUnknown;
        }
    }

    int w = 0, h = 0, numChannels = 4, bytesPerChannel = 1;
    void* data = nullptr;
    int size = 0;
};

namespace FormatUtil
{
struct FormatInfo
{
    Format format;
    const char* name;
    uint8_t bytesPerBlock;
    uint8_t blockWidth;
    uint8_t blockHeight;
    FormatKind kind;
    bool hasRed     : 1;
    bool hasGreen   : 1;
    bool hasBlue    : 1;
    bool hasAlpha   : 1;
    bool hasDepth   : 1;
    bool hasStencil : 1;
    bool isSigned   : 1;
    bool isSRGB     : 1;
};

extern const FormatInfo c_FormatInfo[uint32_t(qhwi::Format::eCount)];
uint32_t getRowPitch(Format format, uint32_t width);
uint32_t getDepthPitch(Format format, uint32_t width, uint32_t height);

constexpr bool isDepthStencilFormat(Format format)
{
    return ((format == Format::eD32Float) || (format == Format::eD24UnormS8Uint) || (format == Format::eD32FloatS8Uint) || (format == Format::eD16Unorm));
}

constexpr const FormatInfo& getFormatInfo(Format format)
{
    const FormatInfo& info = c_FormatInfo[uint32_t(format)];
    QHWI_ASSERT_MSG(info.format == format, "Entry from FormatInfo lookup table doesn't match format");
    return info;
}

constexpr bool isCompressedFormat(Format format)
{
    const FormatInfo& info = getFormatInfo(format);

    return info.blockWidth != 1 || info.blockHeight != 1;
}

constexpr qre::ImageFormat getQreImageFormat(const Format format)
{
    switch (format)
    {
    case Format::eR8Unorm:
        return qre::ImageFormat::ImageFormat_R8Unorm;
    case Format::eRg8Unorm:
        return qre::ImageFormat::ImageFormat_Rg8Unorm;
    case Format::eRgba8Unorm:
        return qre::ImageFormat::ImageFormat_Rgba8Unorm;
    case Format::eR32Float:
        return qre::ImageFormat::ImageFormat_R32Float;
    case Format::eRg32Float:
        return qre::ImageFormat::ImageFormat_Rg32Float;
    case Format::eRgba32Float:
        return qre::ImageFormat::ImageFormat_Rgba32Float;
    default:
        return qre::ImageFormat::ImageFormat_count;
    }
}

constexpr unsigned int getNumChannels(const Format format)
{
    switch (format)
    {
    case Format::eR8Unorm:
    case Format::eR8Int:
    case Format::eR8Uint:
    case Format::eR16Unorm:
    case Format::eR16Int:
    case Format::eR16Uint:
    case Format::eR16Float:
    case Format::eR32Int:
    case Format::eR32Uint:
    case Format::eR32Float:
    case Format::eR8Snorm:
    case Format::eR16Snorm:
    case Format::eBc4Unorm:
    case Format::eBc4Snorm:
    case Format::eEacR11Unorm:
    case Format::eD16Unorm:
    case Format::eD32Float:
        return 1;
        break;
    case Format::eRg8Unorm:
    case Format::eRg8Int:
    case Format::eRg8Uint:
    case Format::eRg16Unorm:
    case Format::eRg16Int:
    case Format::eRg16Uint:
    case Format::eRg16Float:
    case Format::eRg32Int:
    case Format::eRg32Uint:
    case Format::eRg32Float:
    case Format::eRg8Snorm:
    case Format::eRg16Snorm:
    case Format::eBc5Unorm:
    case Format::eBc5Snorm:
    case Format::eEacRg11Unorm:
    case Format::eD24UnormS8Uint:
    case Format::eD32FloatS8Uint:
    case Format::eRg16S10_5_NV:
        return 2;
        break;
    case Format::eRgb32Float:
    case Format::eRgb9E5Float:
    case Format::eRg11B10Float:
    case Format::eBc1Unorm:
    case Format::eBc1UnormSrgb:
    case Format::eBc6HUFloat:
    case Format::eBc6HSFloat:
    case Format::eEtc2RgbUnorm:
    case Format::eEtc2RgbSrgb:
        return 3;
        break;
    case Format::eRgba8Unorm:
    case Format::eRgba8Int:
    case Format::eRgba8Uint:
    case Format::eRgba16Unorm:
    case Format::eRgba16Int:
    case Format::eRgba16Uint:
    case Format::eRgba16Float:
    case Format::eRgba32Int:
    case Format::eRgba32Uint:
    case Format::eRgba32Float:
    case Format::eRgba8Snorm:
    case Format::eRgba16Snorm:
    case Format::eBgra8Unorm:
    case Format::eRgba8Srgb:
    case Format::eBgra8Srgb:
    case Format::eRgb10A2Unorm:
    case Format::eBc2Unorm:
    case Format::eBc2UnormSrgb:
    case Format::eBc3Unorm:
    case Format::eBc3UnormSrgb:
    case Format::eBc7Unorm:
    case Format::eBc7UnormSrgb:
    case Format::eEtc2RgbaUnorm:
    case Format::eEtc2RgbaSrgb:
    case Format::eAstc4x4Unorm:
    case Format::eAstc4x4Srgb:
    case Format::eAstc5x4Unorm:
    case Format::eAstc5x4Srgb:
    case Format::eAstc5x5Unorm:
    case Format::eAstc5x5Srgb:
    case Format::eAstc6x5Unorm:
    case Format::eAstc6x5Srgb:
    case Format::eAstc6x6Unorm:
    case Format::eAstc6x6Srgb:
    case Format::eAstc8x5Unorm:
    case Format::eAstc8x5Srgb:
    case Format::eAstc8x6Unorm:
    case Format::eAstc8x6Srgb:
    case Format::eAstc8x8Unorm:
    case Format::eAstc8x8Srgb:
    case Format::eAstc10x5Unorm:
    case Format::eAstc10x5Srgb:
    case Format::eAstc10x6Unorm:
    case Format::eAstc10x6Srgb:
    case Format::eAstc10x8Unorm:
    case Format::eAstc10x8Srgb:
    case Format::eAstc10x10Unorm:
    case Format::eAstc10x10Srgb:
    case Format::eAstc12x10Unorm:
    case Format::eAstc12x10Srgb:
    case Format::eAstc12x12Unorm:
    case Format::eAstc12x12Srgb:
        return 4;
        break;
    default:
        return 0;
    }
}

}; // namespace FormatUtil

namespace TextureUtil
{
struct TextureSubresourceInfo
{
    uint32_t mipWidth = 0, mipHeight = 0, mipDepth = 0;
    uint32_t numBlocksX = 0, numBlocksY = 0;
    uint32_t deviceRowPitch = 0, deviceMemSize = 0;
};

extern TextureSubresourceInfo getSubresourceInfo(uint32_t w, uint32_t h, uint32_t d, qhwi::Format format, uint32_t mipLevel);

inline TextureSubresourceInfo getSubresourceInfo(const qhwi::Texture* texture, uint32_t mipLevel)
{
    const auto desc = texture->desc();
    return getSubresourceInfo(desc->w, desc->h, desc->d, desc->format, mipLevel);
}
} // namespace TextureUtil

namespace ext
{
namespace ImageUtil
{
enum class ResultCode : uint8_t
{
    eSuccess,
    eFailed
};
ResultCode readImage(const char* path, ImageData& data, const unsigned int numChannels = 4, const unsigned int bytesPerChannel = 1);
ResultCode readImage(const FileData& byteCode, ImageData& imageData, const unsigned int numChannels = 4, const unsigned int bytesPerChannel = 1);

struct WriteImageParams
{
    int w = 0;
    int h = 0;
    qhwi::Format format = qhwi::Format::eRgba8Unorm;
    int numChannels = 0;
    void* data = nullptr;
    int pitch = 0;
};

ResultCode writePng(const char* path, const WriteImageParams& params);
} // namespace ImageUtil
}; // namespace ext

namespace Algorithm
{
template <typename T0, typename T1>
static bool contains(const T0& s, const T1& t)
{
    return std::find(s.begin(), s.end(), t) != s.end();
}
} // namespace Algorithm

namespace System
{
struct UniqueModuleHandle
{
#ifdef _WIN32
    typedef HMODULE handle_t;
#else
    typedef void* handle_t;
#endif
    ~UniqueModuleHandle()
    {
        if (handle)
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
    }

    handle_t handle = nullptr;
};
}; // namespace System

inline bool streq(const char* s0, const char* s1)
{
    return 0 == strcmp(s0, s1);
}

template <typename T>
static bool isEqual(T* a, T* b, uint32_t count)
{
    T* aPtr = a;
    T* bPtr = b;
    for (uint32_t j = 0; j < count; j++)
    {
        if (*a++ != *b++)
        {
            return false;
        }
    }
    return true;
}

static const char* GraphicsBackendMap[] = { "auto", "d3d12", "vk", "mtl" };
const int GraphicsBackendMapSize = sizeof(GraphicsBackendMap) / sizeof(GraphicsBackendMap[0]);

inline bool fromString(const char* s, GraphicsBackend& b)
{
    for (int j = 0; j < GraphicsBackendMapSize; j++)
    {
        if (streq(s, GraphicsBackendMap[j]))
        {
            b = GraphicsBackend(j);
            return true;
        }
    }
    return false;
}

inline const char* toString(GraphicsBackend b)
{
    return GraphicsBackendMap[int(b)];
}
} // namespace qhwi
#endif
