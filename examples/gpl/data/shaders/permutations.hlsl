//
//
//  permutations.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

// This shader tests the effect of gpl with a large number shaders created
// by permutating a simple vertex/fragment shader pair.
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

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = mul(cbIn.modelViewProj, input.pos);
#ifdef PERM_0
    result.pos *= 0.9;
#endif
#ifdef PERM_1
    result.pos.x *= 0.8;
#endif
#ifdef PERM_2
    result.pos.y *= 0.7;
#endif
#ifdef PERM_3
    result.pos.z *= 0.6;
#endif
#ifdef PERM_4
    result.pos *= 0.5;
#endif
#ifdef PERM_5
    result.pos.x *= 0.4;
#endif
#ifdef PERM_6
    result.pos.y *= 0.3;
#endif
#ifdef PERM_7
    result.pos.z *= 0.2;
#endif
#ifdef PERM_8
    result.pos *= 0.1;
#endif
#ifdef PERM_9
    result.pos.x *= 0.01;
#endif
    result.color = input.color;

    return result;
}

float4 PSMain(VSOutput input)
    : SV_TARGET
{
#ifdef PERM_0
    input.color *= 0.6;
#endif
#ifdef PERM_1
    input.color  = 0.4;
#endif
#ifdef PERM_2
    input.color *= 0.2;
#endif
#ifdef PERM_3
    input.color *= 0.1;
#endif
#ifdef PERM_4
    input.color *= 0.05;
#endif
#ifdef PERM_5
    input.color = input.color.bgra;
#endif
#ifdef PERM_6
    input.color = input.color.brga;
#endif
#ifdef PERM_7
    input.color = input.color.grba;
#endif
#ifdef PERM_8
    input.color = input.color.gbra;
#endif
#ifdef PERM_9
    input.color = input.color.rrba;
#endif

    return input.color;
}
