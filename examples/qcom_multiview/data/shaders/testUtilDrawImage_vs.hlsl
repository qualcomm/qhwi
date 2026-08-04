//
//
//  testUtilDrawImage_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION0;
    float2 uv  : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos   = float4(input.pos, 0.5f, 1.0f);
    result.uv    = input.uv;
    return result;
}
