//
//
//  testRenderToCubemap_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION;
};

struct VSOutput
{
    float4 pos      : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos   = float4(input.pos, 0.5f, 1.0f);

    return result;
}
