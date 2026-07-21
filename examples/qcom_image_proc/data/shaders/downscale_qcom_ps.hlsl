//
//
//  downscale_qcom_ps.hlsl
//
//  Copyright (C) 2021-2024 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

#define CapabilityTextureSampleWeightedQCOM 4484
#define OpImageSampleWeightedQCOM 4480
#define DecorationWeightTextureQCOM 4487

// Allow dxc to create spir-v for this extension
[[vk::ext_capability(CapabilityTextureSampleWeightedQCOM)]]
[[vk::ext_extension("SPV_QCOM_image_processing")]]
[[vk::ext_instruction(OpImageSampleWeightedQCOM)]]
float4 textureWeightedQCOM(Texture2D tex, float2 P, Texture2DArray weight);

Texture2D inputTex : register(t0);
sampler inputTexSampler : register(s0);

[[vk::ext_decorate(DecorationWeightTextureQCOM)]]
Texture2DArray kernelTex : register(t1);
sampler kernelTexSampler : register(s1);

float4 PSMain(VSOutput input) : SV_TARGET
{
    return textureWeightedQCOM(inputTex, input.uv, kernelTex);
}
