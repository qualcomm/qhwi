//
//
//  testMultipleRenderTargets1_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION0;
};

cbuffer cbIn : register(b0)
{
    float4 color;
}

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 color : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = float4(input.pos, 0.5f, 1.0f);
    result.color = color;
    return result;
}
