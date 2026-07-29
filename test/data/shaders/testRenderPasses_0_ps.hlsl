//
//
//  testRenderPasses_0_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos   : SV_POSITION;
    float4 color : TEXCOORD0;
};

struct PSOutput
{
    float4 color0 : SV_TARGET0;
    float4 color1 : SV_TARGET1;
    float4 color2 : SV_TARGET2;
    float4 color3 : SV_TARGET3;
};

PSOutput PSMain(VSOutput input)
{
    float4 c = input.color;
    PSOutput output;
    output.color0 = float4(c.r, 0.0, 0.0, 0.0);
    output.color1 = float4(c.g, 0.0, 0.0, 0.0);
    output.color2 = float4(c.b, 0.0, 0.0, 0.0);
    output.color3 = float4(c.a, 0.0, 0.0, 0.0);
    return output;
}