//
//
//  qhwi_util.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_util.h>
#include <qhwi/qhwi_log_util.h>
#include <qhwi/qhwi.h>
#include <qre/memory/align.h>
#include <qre/image.h>
#include <fstream>

using namespace qhwi;
using namespace qhwi::ext;

#define FORMAT_INFO(s) qhwi::Format::e##s, #s

const qhwi::FormatUtil::FormatInfo qhwi::FormatUtil::c_FormatInfo[uint32_t(qhwi::Format::eCount)] = {
    //format  name                    bytes blkw blkh kind                           red    green  blue   alpha  depth  stencl signed srgb
    { FORMAT_INFO(Unknown), 0, 0, 0, qhwi::FormatKind::eInteger, false, false, false, false, false, false, false, false },
    { FORMAT_INFO(R8Unorm), 1, 1, 1, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg8Unorm), 2, 1, 1, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba8Unorm), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(R8Int), 1, 1, 1, qhwi::FormatKind::eInteger, true, false, false, false, false, false, true, false },
    { FORMAT_INFO(Rg8Int), 2, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, true, false },
    { FORMAT_INFO(Rgba8Int), 4, 1, 1, qhwi::FormatKind::eInteger, true, true, true, true, false, false, true, false },
    { FORMAT_INFO(R8Uint), 1, 1, 1, qhwi::FormatKind::eInteger, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg8Uint), 2, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba8Uint), 4, 1, 1, qhwi::FormatKind::eInteger, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(R16Unorm), 2, 1, 1, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg16Unorm), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba16Unorm), 8, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(R16Int), 2, 1, 1, qhwi::FormatKind::eInteger, true, false, false, false, false, false, true, false },
    { FORMAT_INFO(Rg16Int), 4, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, true, false },
    { FORMAT_INFO(Rgba16Int), 8, 1, 1, qhwi::FormatKind::eInteger, true, true, true, true, false, false, true, false },
    { FORMAT_INFO(R16Uint), 2, 1, 1, qhwi::FormatKind::eInteger, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg16Uint), 4, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba16Uint), 8, 1, 1, qhwi::FormatKind::eInteger, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(R16Float), 2, 1, 1, qhwi::FormatKind::eFloat, true, false, false, false, false, false, true, false },
    { FORMAT_INFO(Rg16Float), 4, 1, 1, qhwi::FormatKind::eFloat, true, true, false, false, false, false, true, false },
    { FORMAT_INFO(Rgba16Float), 8, 1, 1, qhwi::FormatKind::eFloat, true, true, true, true, false, false, true, false },
    { FORMAT_INFO(R32Int), 4, 1, 1, qhwi::FormatKind::eInteger, true, false, false, false, false, false, true, false },
    { FORMAT_INFO(Rg32Int), 8, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, true, false },
    { FORMAT_INFO(Rgba32Int), 16, 1, 1, qhwi::FormatKind::eInteger, true, true, true, true, false, false, true, false },
    { FORMAT_INFO(R32Uint), 4, 1, 1, qhwi::FormatKind::eInteger, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg32Uint), 8, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba32Uint), 16, 1, 1, qhwi::FormatKind::eInteger, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(R32Float), 4, 1, 1, qhwi::FormatKind::eFloat, true, false, false, false, false, false, true, false },
    { FORMAT_INFO(Rg32Float), 8, 1, 1, qhwi::FormatKind::eFloat, true, true, false, false, false, false, true, false },
    { FORMAT_INFO(Rgba32Float), 16, 1, 1, qhwi::FormatKind::eFloat, true, true, true, true, false, false, true, false },
    { FORMAT_INFO(R8Snorm), 1, 1, 1, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg8Snorm), 2, 1, 1, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba8Snorm), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(R16Snorm), 2, 1, 1, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Rg16Snorm), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Rgba16Snorm), 8, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Rgb32Float), 12, 1, 1, qhwi::FormatKind::eFloat, true, true, true, false, false, false, true, false },
    { FORMAT_INFO(Bgra8Unorm), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Rgba8Srgb), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Bgra8Srgb), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Rgb9E5Float), 4, 1, 1, qhwi::FormatKind::eFloat, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Rgb10A2Unorm), 4, 1, 1, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Rg11B10Float), 4, 1, 1, qhwi::FormatKind::eFloat, true, true, true, false, false, false, false, false },
    { FORMAT_INFO(Bc1Unorm), 8, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Bc1UnormSrgb), 8, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Bc2Unorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Bc2UnormSrgb), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Bc3Unorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Bc3UnormSrgb), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Bc4Unorm), 8, 4, 4, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Bc4Snorm), 8, 4, 4, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(Bc5Unorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Bc5Snorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Bc6HUFloat), 16, 4, 4, qhwi::FormatKind::eFloat, true, true, true, false, false, false, false, false },
    { FORMAT_INFO(Bc6HSFloat), 16, 4, 4, qhwi::FormatKind::eFloat, true, true, true, false, false, false, true, false },
    { FORMAT_INFO(Bc7Unorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Bc7UnormSrgb), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Etc2RgbUnorm), 8, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, false, false, false, false, false },
    { FORMAT_INFO(Etc2RgbSrgb), 8, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, false, false, false, false, true },
    { FORMAT_INFO(Etc2RgbaUnorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Etc2RgbaSrgb), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(EacR11Unorm), 8, 4, 4, qhwi::FormatKind::eNormalized, true, false, false, false, false, false, false, false },
    { FORMAT_INFO(EacRg11Unorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, false, false, false, false, false, false },
    { FORMAT_INFO(Astc4x4Unorm), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc4x4Srgb), 16, 4, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc5x4Unorm), 16, 5, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc5x4Srgb), 16, 5, 4, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc5x5Unorm), 16, 5, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc5x5Srgb), 16, 5, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc6x5Unorm), 16, 6, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc6x5Srgb), 16, 6, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc6x6Unorm), 16, 6, 6, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc6x6Srgb), 16, 6, 6, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc8x5Unorm), 16, 8, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc8x5Srgb), 16, 8, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc8x6Unorm), 16, 8, 6, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc8x6Srgb), 16, 8, 6, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc8x8Unorm), 16, 8, 8, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc8x8Srgb), 16, 8, 8, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc10x5Unorm), 16, 10, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc10x5Srgb), 16, 10, 5, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc10x6Unorm), 16, 10, 6, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc10x6Srgb), 16, 10, 6, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc10x8Unorm), 16, 10, 8, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc10x8Srgb), 16, 10, 8, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc10x10Unorm), 16, 10, 10, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc10x10Srgb), 16, 10, 10, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc12x10Unorm), 16, 12, 10, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc12x10Srgb), 16, 12, 10, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(Astc12x12Unorm), 16, 12, 12, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, false },
    { FORMAT_INFO(Astc12x12Srgb), 16, 12, 12, qhwi::FormatKind::eNormalized, true, true, true, true, false, false, false, true },
    { FORMAT_INFO(D16Unorm), 2, 1, 1, qhwi::FormatKind::eDepthStencil, false, false, false, false, true, false, false, false },
    { FORMAT_INFO(D24UnormS8Uint), 4, 1, 1, qhwi::FormatKind::eDepthStencil, false, false, false, false, true, true, false, false },
    { FORMAT_INFO(D32Float), 4, 1, 1, qhwi::FormatKind::eDepthStencil, false, false, false, false, true, false, false, false },
    { FORMAT_INFO(D32FloatS8Uint), 8, 1, 1, qhwi::FormatKind::eDepthStencil, false, false, false, false, true, true, false, false },
    { FORMAT_INFO(Rg16S10_5_NV), 4, 1, 1, qhwi::FormatKind::eInteger, true, true, false, false, false, false, true, false }
};

qhwi::FileUtil::ResultCode qhwi::FileUtil::readFile(const char* path, FileData& fileData, Flags flags)
{
    std::ifstream in(path, std::ios::binary | std::ios::in | std::ios::ate);
    if (!in.is_open())
    {
        if (!(flags & uint8_t(Flag::eQuiet)))
        {
            QHWI_ERR("cannot open file: %s", path);
        }
        return ResultCode::eFileNotFound;
    }
    fileData.size = in.tellg();
    fileData.data = malloc(fileData.size);
    in.seekg(0, std::ios::beg);
    in.read((char*)fileData.data, fileData.size);
    in.close();
    return ResultCode::eSuccess;
}

#ifdef QHWI_PLATFORM_ANDROID
static AAssetManager* s_AAssetManager = nullptr;

void qhwi::FileUtil::setAndroidAssetManager(AAssetManager* aAssetManager)
{
    s_AAssetManager = aAssetManager;
}

qhwi::FileUtil::ResultCode qhwi::FileUtil::readAndroidAsset(const char* path, FileData& fileData, Flags flags)
{
    QHWI_ASSERT(s_AAssetManager);
    AAsset* asset = AAssetManager_open(s_AAssetManager, path, AASSET_MODE_BUFFER);
    if (!asset)
    {
        if (!(flags & uint8_t(Flag::eQuiet)))
        {
            QHWI_ERR("cannot open file: %s", path);
        }
        return ResultCode::eFileNotFound;
    }
    fileData.size = AAsset_getLength(asset);
    fileData.data = malloc(fileData.size);
    AAsset_read(asset, fileData.data, fileData.size);
    AAsset_close(asset);
    return ResultCode::eSuccess;
}
#endif

qhwi::FileUtil::ResultCode qhwi::FileUtil::writeFile(const char* path, void* data, int size, Flags flags)
{
    std::ofstream out(path, std::ios::binary | std::ios::out);
    if (!out.is_open())
    {
        if (!(flags & uint8_t(Flag::eQuiet)))
        {
            QHWI_ERR("cannot open file: %s", path);
        }
        return ResultCode::eFileNotFound;
    }
    out.write((char*)data, size);
    out.close();
    return ResultCode::eSuccess;
}

uint32_t qhwi::FormatUtil::getRowPitch(qhwi::Format format, uint32_t width)
{
    const FormatInfo& formatInfo = c_FormatInfo[uint32_t(format)];
    return (qre::AlignUp(width, formatInfo.blockWidth) / formatInfo.blockWidth) * formatInfo.bytesPerBlock;
}

uint32_t qhwi::FormatUtil::getDepthPitch(qhwi::Format format, uint32_t width, uint32_t height)
{
    const FormatInfo& formatInfo = c_FormatInfo[uint32_t(format)];
    return (qre::AlignUp(height, formatInfo.blockHeight) / formatInfo.blockHeight) * getRowPitch(format, width);
}

qhwi::TextureUtil::TextureSubresourceInfo qhwi::TextureUtil::getSubresourceInfo(uint32_t w, uint32_t h, uint32_t d, Format format, uint32_t mipLevel)
{
    TextureSubresourceInfo s;
    const auto& formatInfo = FormatUtil::getFormatInfo(format);
    s.mipWidth = std::max(w >> mipLevel, 1u);
    s.mipHeight = std::max(h >> mipLevel, 1u);
    s.mipDepth = std::max(d >> mipLevel, 1u);
    s.numBlocksX = (s.mipWidth + formatInfo.blockWidth - 1) / formatInfo.blockWidth;
    s.numBlocksY = (s.mipHeight + formatInfo.blockHeight - 1) / formatInfo.blockHeight;
    s.deviceRowPitch = s.numBlocksX * formatInfo.bytesPerBlock;
    s.deviceMemSize = s.numBlocksY * s.deviceRowPitch * s.mipDepth;
    return s;
}

ImageUtil::ResultCode ImageUtil::readImage(const char* path, ImageData& imageData, const unsigned int numChannels, const unsigned int bytesPerChannel)
{
    qre::ImageDesc desc;
    imageData.data = qre::ImageLoad(&desc, path, numChannels);
    if (!imageData.data)
    {
        return ResultCode::eFailed;
    }
    imageData.w = desc.width;
    imageData.h = desc.height;
    imageData.numChannels = numChannels;
    imageData.bytesPerChannel = bytesPerChannel;
    imageData.size = imageData.w * imageData.h * imageData.numChannels * imageData.bytesPerChannel;

    return ResultCode::eSuccess;
}

ImageUtil::ResultCode ImageUtil::readImage(const FileData& byteCode, ImageData& imageData, const unsigned int numChannels, const unsigned int bytesPerChannel)
{
    qre::ImageDesc desc;
    imageData.data = qre::ImageLoadFromMemory(&desc, byteCode.data, byteCode.size, numChannels);
    if (!imageData.data)
    {
        return ResultCode::eFailed;
    }
    imageData.w = desc.width;
    imageData.h = desc.height;
    imageData.bytesPerChannel = bytesPerChannel;
    imageData.numChannels = numChannels;
    imageData.size = imageData.w * imageData.h * imageData.numChannels * imageData.bytesPerChannel;
    return ResultCode::eSuccess;
}

#include <iostream>
#include <filesystem>

static ImageUtil::ResultCode writeImage(const char* path, const ImageUtil::WriteImageParams& p)
{
    const qre::ImageDesc desc = { .type = qre::ImageType_2D,
                                  .format = FormatUtil::getQreImageFormat(p.format),
                                  .width = static_cast<uint32_t>(p.w),
                                  .height = static_cast<uint32_t>(p.h),
                                  .depth = 1,
                                  .levelCount = 1,
                                  .sampleCount = 1,
                                  .arrayCount = 1 };

    // create directory if needed
    if (const std::filesystem::path dirPath = std::filesystem::path(path).parent_path(); !dirPath.empty() && !std::filesystem::exists(dirPath))
    {
        try
        {
            std::filesystem::create_directories(dirPath);
        }
        catch (...)
        {
            return ImageUtil::ResultCode::eSuccess;
        }
    }

    if (!qre::ImageStore(desc, p.data, path, nullptr, p.pitch))
    {
        return ImageUtil::ResultCode::eFailed;
    }
    return ImageUtil::ResultCode::eSuccess;
}

ImageUtil::ResultCode ImageUtil::writePng(const char* path, const WriteImageParams& p)
{
    return writeImage(path, p);
}
