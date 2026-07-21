//
//
//  vk_custom_resolve.hlsl
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(uint VertexId : SV_VertexID)
{
    VSOutput result;
    // Full-screen triangle strip quad
    const float2 Pos[] = { float2(-1, -1), float2(-1, 1), float2(1, -1), float2(1, 1) };
    result.pos = float4(Pos[VertexId], 0.0f, 1.0f);
    return result;
}

#ifdef SPIRV
[[vk::input_attachment_index(0)]]
SubpassInputMS<float4> msaaColor : register(t0); // no SamplerState needed
#else
Texture2D    resolvedColor : register(t0);
SamplerState resolvedSampler : register(s0);
#endif

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Sample the resolved texture and convert to grayscale
    // using standard luminance weights (ITU-R BT.709).
#ifdef SPIRV
    float4 sum = 0;
    [unroll] for (int i = 0; i < 4; i++)
        sum += msaaColor.SubpassLoad(i);
    float4 resolved = sum / 4.0;
#else
    float4 resolved = resolvedColor.Sample(resolvedSampler, input.uv);
#endif
    float luma = dot(resolved.rgb, float3(0.2126, 0.7152, 0.0722));
    return float4(luma, luma, luma, 1.0);
}
