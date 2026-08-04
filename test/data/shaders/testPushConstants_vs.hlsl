//
//
//  testPushConstants_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION0;
};

struct CbIn
{
    float4 color;
};
[[vk::push_constant]] ConstantBuffer<CbIn> cbIn : register(b0);

struct VSOutput
{
    float4 pos      : SV_POSITION;
    float4 color    : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos   = float4(input.pos, 0.5f, 1.0f);
    result.color = cbIn.color;
    return result;
}
