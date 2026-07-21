//
//
//  qhwi_d3d12_enums_internal.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_D3D12_ENUMS_INTERNAL_H__
#define __QHWI_D3D12_ENUMS_INTERNAL_H__

#include <d3d12.h>

#define QHWI_DEFINE_DXGI_FORMATS(s, t0, t1)                                                                                                                                        \
    CASE(Format::eR##s##t0, DXGI_FORMAT_R##s##_##t1);                                                                                                                              \
    CASE(Format::eRg##s##t0, DXGI_FORMAT_R##s##G##s##_##t1);                                                                                                                       \
    CASE(Format::eRgba##s##t0, DXGI_FORMAT_R##s##G##s##B##s##A##s##_##t1)

#define QHWI_DXGI_FORMAT_MAP()                                                                                                                                                     \
    QHWI_DEFINE_DXGI_FORMATS(8, Unorm, UNORM);                                                                                                                                     \
    QHWI_DEFINE_DXGI_FORMATS(8, Int, SINT);                                                                                                                                        \
    QHWI_DEFINE_DXGI_FORMATS(8, Uint, UINT);                                                                                                                                       \
    QHWI_DEFINE_DXGI_FORMATS(16, Unorm, UNORM);                                                                                                                                    \
    QHWI_DEFINE_DXGI_FORMATS(16, Int, SINT);                                                                                                                                       \
    QHWI_DEFINE_DXGI_FORMATS(16, Uint, UINT);                                                                                                                                      \
    QHWI_DEFINE_DXGI_FORMATS(16, Float, FLOAT);                                                                                                                                    \
    QHWI_DEFINE_DXGI_FORMATS(32, Int, SINT);                                                                                                                                       \
    QHWI_DEFINE_DXGI_FORMATS(32, Uint, UINT);                                                                                                                                      \
    QHWI_DEFINE_DXGI_FORMATS(32, Float, FLOAT);                                                                                                                                    \
    QHWI_DEFINE_DXGI_FORMATS(8, Snorm, SNORM);                                                                                                                                     \
    QHWI_DEFINE_DXGI_FORMATS(16, Snorm, SNORM);                                                                                                                                    \
    CASE(Format::eRgb32Float, DXGI_FORMAT_R32G32B32_FLOAT);                                                                                                                        \
    CASE(Format::eBgra8Unorm, DXGI_FORMAT_B8G8R8A8_UNORM);                                                                                                                         \
    CASE(Format::eRgba8Srgb, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);                                                                                                                     \
    CASE(Format::eBgra8Srgb, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);                                                                                                                     \
    CASE(Format::eRgb9E5Float, DXGI_FORMAT_R9G9B9E5_SHAREDEXP);                                                                                                                    \
    CASE(Format::eRgb10A2Unorm, DXGI_FORMAT_R10G10B10A2_UNORM);                                                                                                                    \
    CASE(Format::eRg11B10Float, DXGI_FORMAT_R11G11B10_FLOAT);                                                                                                                      \
    CASE(Format::eD16Unorm, DXGI_FORMAT_D16_UNORM);                                                                                                                                \
    CASE(Format::eD24UnormS8Uint, DXGI_FORMAT_D24_UNORM_S8_UINT);                                                                                                                  \
    CASE(Format::eD32Float, DXGI_FORMAT_D32_FLOAT);                                                                                                                                \
    CASE(Format::eD32FloatS8Uint, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);                                                                                                               \
    CASE(Format::eBc1Unorm, DXGI_FORMAT_BC1_UNORM);                                                                                                                                \
    CASE(Format::eBc1UnormSrgb, DXGI_FORMAT_BC1_UNORM_SRGB);                                                                                                                       \
    CASE(Format::eBc2Unorm, DXGI_FORMAT_BC2_UNORM);                                                                                                                                \
    CASE(Format::eBc2UnormSrgb, DXGI_FORMAT_BC2_UNORM_SRGB);                                                                                                                       \
    CASE(Format::eBc3Unorm, DXGI_FORMAT_BC3_UNORM);                                                                                                                                \
    CASE(Format::eBc3UnormSrgb, DXGI_FORMAT_BC3_UNORM_SRGB);                                                                                                                       \
    CASE(Format::eBc4Unorm, DXGI_FORMAT_BC4_UNORM);                                                                                                                                \
    CASE(Format::eBc4Snorm, DXGI_FORMAT_BC4_SNORM);                                                                                                                                \
    CASE(Format::eBc5Unorm, DXGI_FORMAT_BC5_UNORM);                                                                                                                                \
    CASE(Format::eBc5Snorm, DXGI_FORMAT_BC5_SNORM);                                                                                                                                \
    CASE(Format::eBc6HUFloat, DXGI_FORMAT_BC6H_UF16);                                                                                                                              \
    CASE(Format::eBc6HSFloat, DXGI_FORMAT_BC6H_SF16);                                                                                                                              \
    CASE(Format::eBc7Unorm, DXGI_FORMAT_BC7_UNORM);                                                                                                                                \
    CASE(Format::eBc7UnormSrgb, DXGI_FORMAT_BC7_UNORM_SRGB);                                                                                                                       \
    CASE(Format::eUnknown, DXGI_FORMAT_UNKNOWN);
#endif
