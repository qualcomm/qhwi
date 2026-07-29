//
//
//  compute1.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

[[vk::tile_attachment_qcom]]
[[vk::binding(1)]]
RWTexture2D<float4> color0;

struct TransientOutS
{
    float4 dataTransient;
};

[[vk::tile_attachment_qcom]]
[[vk::binding(2)]]
uniform RWStructuredBuffer<TransientOutS> TransientOut;

[[vk::tile_attachment_qcom]]
[[vk::binding(3)]]
RWTexture2D<float4> color1;

[[vk::tile_shading_rate_qcom(1, 1, 1)]]
[numthreads(1, 1, 1)] //ignored
void main(
     // Arbitrary refers to semantics that have no special meaning
    [[vk::builtin("TileOffsetQCOM")]] uint2 gl_TileOffsetQCOM : Arbitrary,
    [[vk::builtin("TileDimensionQCOM")]] uint3 gl_TileDimensionQCOM : Arbitrary,
    [[vk::builtin("TileApronSizeQCOM")]] uint2 gl_TileApronSizeQCOM : Arbitrary,
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID
)
{
    int2 pixelLocation = int2(gl_TileOffsetQCOM.xy + gl_GlobalInvocationID.xy);

    float4 finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int offset = 2;
    for(int x = -offset; x <= offset; x++)
    {
         for(int y = -offset; y <= offset; y++)
         {
            float4 pixelColor = color0.Load(pixelLocation + uint2(x,y));
            finalColor += pixelColor / ((offset * 2 + 1)*(offset * 2 + 1));
         }
    }
    color1[pixelLocation] = finalColor;
    
    TransientOut[0].dataTransient = float4(gl_TileDimensionQCOM.x,
                            gl_TileDimensionQCOM.y,
                            gl_TileOffsetQCOM.x,
                            gl_TileOffsetQCOM.y);    
}