//
//
//  qhwi_vk_enums_internal.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_VK_ENUMS_INTERNAL_H__
#define __QHWI_VK_ENUMS_INTERNAL_H__

#include <vulkan/vulkan.hpp>

#define QHWI_DEFINE_VK_FORMATS(s, t0, t1)                                                                                                                                          \
    CASE(Format::eR##s##t0, vk::Format::eR##s##t1);                                                                                                                                \
    CASE(Format::eRg##s##t0, vk::Format::eR##s##G##s##t1);                                                                                                                         \
    CASE(Format::eRgba##s##t0, vk::Format::eR##s##G##s##B##s##A##s##t1)

#define QHWI_VK_FORMAT_MAP()                                                                                                                                                       \
    QHWI_DEFINE_VK_FORMATS(8, Unorm, Unorm);                                                                                                                                       \
    QHWI_DEFINE_VK_FORMATS(8, Int, Sint);                                                                                                                                          \
    QHWI_DEFINE_VK_FORMATS(8, Uint, Uint);                                                                                                                                         \
    QHWI_DEFINE_VK_FORMATS(16, Unorm, Unorm);                                                                                                                                      \
    QHWI_DEFINE_VK_FORMATS(16, Int, Sint);                                                                                                                                         \
    QHWI_DEFINE_VK_FORMATS(16, Uint, Uint);                                                                                                                                        \
    QHWI_DEFINE_VK_FORMATS(16, Float, Sfloat);                                                                                                                                     \
    QHWI_DEFINE_VK_FORMATS(32, Int, Sint);                                                                                                                                         \
    QHWI_DEFINE_VK_FORMATS(32, Uint, Uint);                                                                                                                                        \
    QHWI_DEFINE_VK_FORMATS(32, Float, Sfloat);                                                                                                                                     \
    QHWI_DEFINE_VK_FORMATS(8, Snorm, Snorm);                                                                                                                                       \
    QHWI_DEFINE_VK_FORMATS(16, Snorm, Snorm);                                                                                                                                      \
    CASE(Format::eRgb32Float, vk::Format::eR32G32B32Sfloat);                                                                                                                       \
    CASE(Format::eBgra8Unorm, vk::Format::eB8G8R8A8Unorm);                                                                                                                         \
    CASE(Format::eRgba8Srgb, vk::Format::eR8G8B8A8Srgb);                                                                                                                           \
    CASE(Format::eBgra8Srgb, vk::Format::eB8G8R8A8Srgb);                                                                                                                           \
    CASE(Format::eRgb9E5Float, vk::Format::eE5B9G9R9UfloatPack32);                                                                                                                 \
    CASE(Format::eRgb10A2Unorm, vk::Format::eA2B10G10R10UnormPack32);                                                                                                              \
    CASE(Format::eRg11B10Float, vk::Format::eB10G11R11UfloatPack32);                                                                                                               \
    CASE(Format::eD16Unorm, vk::Format::eD16Unorm);                                                                                                                                \
    CASE(Format::eD24UnormS8Uint, vk::Format::eD24UnormS8Uint);                                                                                                                    \
    CASE(Format::eD32Float, vk::Format::eD32Sfloat);                                                                                                                               \
    CASE(Format::eD32FloatS8Uint, vk::Format::eD32SfloatS8Uint);                                                                                                                   \
    CASE(Format::eBc1Unorm, vk::Format::eBc1RgbUnormBlock);                                                                                                                        \
    CASE(Format::eBc1UnormSrgb, vk::Format::eBc1RgbSrgbBlock);                                                                                                                     \
    CASE(Format::eBc2Unorm, vk::Format::eBc2UnormBlock);                                                                                                                           \
    CASE(Format::eBc2UnormSrgb, vk::Format::eBc2SrgbBlock);                                                                                                                        \
    CASE(Format::eBc3Unorm, vk::Format::eBc3UnormBlock);                                                                                                                           \
    CASE(Format::eBc3UnormSrgb, vk::Format::eBc3SrgbBlock);                                                                                                                        \
    CASE(Format::eBc4Unorm, vk::Format::eBc4UnormBlock);                                                                                                                           \
    CASE(Format::eBc4Snorm, vk::Format::eBc4SnormBlock);                                                                                                                           \
    CASE(Format::eBc5Unorm, vk::Format::eBc5UnormBlock);                                                                                                                           \
    CASE(Format::eBc5Snorm, vk::Format::eBc5SnormBlock);                                                                                                                           \
    CASE(Format::eBc6HUFloat, vk::Format::eBc6HUfloatBlock);                                                                                                                       \
    CASE(Format::eBc6HSFloat, vk::Format::eBc6HSfloatBlock);                                                                                                                       \
    CASE(Format::eBc7Unorm, vk::Format::eBc7UnormBlock);                                                                                                                           \
    CASE(Format::eBc7UnormSrgb, vk::Format::eBc7SrgbBlock);                                                                                                                        \
    CASE(Format::eEtc2RgbUnorm, vk::Format::eEtc2R8G8B8UnormBlock);                                                                                                                \
    CASE(Format::eEtc2RgbSrgb, vk::Format::eEtc2R8G8B8SrgbBlock);                                                                                                                  \
    CASE(Format::eEtc2RgbaUnorm, vk::Format::eEtc2R8G8B8A8UnormBlock);                                                                                                             \
    CASE(Format::eEtc2RgbaSrgb, vk::Format::eEtc2R8G8B8A8SrgbBlock);                                                                                                               \
    CASE(Format::eEacR11Unorm, vk::Format::eEacR11UnormBlock);                                                                                                                     \
    CASE(Format::eEacRg11Unorm, vk::Format::eEacR11G11UnormBlock);                                                                                                                 \
    CASE(Format::eAstc4x4Unorm, vk::Format::eAstc4x4UnormBlock);                                                                                                                   \
    CASE(Format::eAstc4x4Srgb, vk::Format::eAstc4x4SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc5x4Unorm, vk::Format::eAstc5x4UnormBlock);                                                                                                                   \
    CASE(Format::eAstc5x4Srgb, vk::Format::eAstc5x4SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc5x5Unorm, vk::Format::eAstc5x5UnormBlock);                                                                                                                   \
    CASE(Format::eAstc5x5Srgb, vk::Format::eAstc5x5SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc6x5Unorm, vk::Format::eAstc6x5UnormBlock);                                                                                                                   \
    CASE(Format::eAstc6x5Srgb, vk::Format::eAstc6x5SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc6x6Unorm, vk::Format::eAstc6x6UnormBlock);                                                                                                                   \
    CASE(Format::eAstc6x6Srgb, vk::Format::eAstc6x6SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc8x5Unorm, vk::Format::eAstc8x5UnormBlock);                                                                                                                   \
    CASE(Format::eAstc8x5Srgb, vk::Format::eAstc8x5SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc8x6Unorm, vk::Format::eAstc8x6UnormBlock);                                                                                                                   \
    CASE(Format::eAstc8x6Srgb, vk::Format::eAstc8x6SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc8x8Unorm, vk::Format::eAstc8x8UnormBlock);                                                                                                                   \
    CASE(Format::eAstc8x8Srgb, vk::Format::eAstc8x8SrgbBlock);                                                                                                                     \
    CASE(Format::eAstc10x5Unorm, vk::Format::eAstc10x5UnormBlock);                                                                                                                 \
    CASE(Format::eAstc10x5Srgb, vk::Format::eAstc10x5SrgbBlock);                                                                                                                   \
    CASE(Format::eAstc10x6Unorm, vk::Format::eAstc10x6UnormBlock);                                                                                                                 \
    CASE(Format::eAstc10x6Srgb, vk::Format::eAstc10x6SrgbBlock);                                                                                                                   \
    CASE(Format::eAstc10x8Unorm, vk::Format::eAstc10x8UnormBlock);                                                                                                                 \
    CASE(Format::eAstc10x8Srgb, vk::Format::eAstc10x8SrgbBlock);                                                                                                                   \
    CASE(Format::eAstc10x10Unorm, vk::Format::eAstc10x10UnormBlock);                                                                                                               \
    CASE(Format::eAstc10x10Srgb, vk::Format::eAstc10x10SrgbBlock);                                                                                                                 \
    CASE(Format::eAstc12x10Unorm, vk::Format::eAstc12x10UnormBlock);                                                                                                               \
    CASE(Format::eAstc12x10Srgb, vk::Format::eAstc12x10SrgbBlock);                                                                                                                 \
    CASE(Format::eAstc12x12Unorm, vk::Format::eAstc12x12UnormBlock);                                                                                                               \
    CASE(Format::eAstc12x12Srgb, vk::Format::eAstc12x12SrgbBlock);                                                                                                                 \
    CASE(Format::eRg16S10_5_NV, vk::Format::eR16G16S105NV);
#endif
