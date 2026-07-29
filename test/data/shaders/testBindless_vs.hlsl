//
//
//  testBindless_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos       : POSITION0;
    int textureIndex : TEXINDEX0;
};

struct VSOutput
{
    float4 pos       : SV_POSITION;
    int textureIndex : TEXINDEX0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput r;
    r.pos = float4(input.pos, 0.5f, 1.0f);
    r.textureIndex = input.textureIndex;
    return r;
}
