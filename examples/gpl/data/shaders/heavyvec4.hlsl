//
//
// heavyVec4.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

// This shader tests the effect of gpl and the shader compiler with a large number
// of vec4 calculations.
struct VSInput
{
    float4 pos : POSITION0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
};

struct CBInput
{
    float4x4 modelViewProj;
};

[[vk::push_constant]] ConstantBuffer<CBInput> cbIn : register(b0);

#define ARRAY_SIZE 512
VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = mul(cbIn.modelViewProj, input.pos);

    float4 f[ARRAY_SIZE];
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
        f[i] = float4(1.0, 2.0, 3.0, 4.0);
        if (i > 0)
        {
            f[i] += f[i - 1];
        }
    }
    result.color = input.color + f[ARRAY_SIZE - 1];

    return result;
}

float4 PSMain(VSOutput input)
    : SV_TARGET
{
    return input.color;
}
