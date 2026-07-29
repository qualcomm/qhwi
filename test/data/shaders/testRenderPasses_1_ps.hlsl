//
//
//  testRenderPasses_1_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifdef SPIRV
#define SUBPASS_INPUT(binding, name) \
[[vk::input_attachment_index(binding)]] SubpassInput name
#define SUBPASS_LOAD(name) \
name.SubpassLoad()
#else
#define SUBPASS_INPUT(binding, name) \
Texture2D name : register(t##binding)
#define SUBPASS_LOAD(name) \
name[int2(input.pos.xy)]
#endif

struct VSOutput
{
    float4 pos : SV_POSITION;
};

SUBPASS_INPUT(0, color0);
SUBPASS_INPUT(1, color1);
SUBPASS_INPUT(2, color2);
SUBPASS_INPUT(3, color3);

float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(
        SUBPASS_LOAD(color0).r,
        SUBPASS_LOAD(color1).r,
        SUBPASS_LOAD(color2).r,
        SUBPASS_LOAD(color3).r
    );
}