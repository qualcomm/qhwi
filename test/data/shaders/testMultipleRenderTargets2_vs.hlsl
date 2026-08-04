//
//
//  testMultipleRenderTargets2_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION0;
    float2 uvw : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uvw : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos   = float4(input.pos.x, input.pos.y, 0.5f, 1.0);
    result.uvw = input.uvw;
    return result;
}
