//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

RWTexture2D<float4> ComputeReadTask1_TextureIn : register(u0);
RWTexture2D<float4> ComputeReadTask1_TextureOut : register(u1);
#define ComputeReadTask1_NumThreadsPerThreadGroup 32, 32, 1

[numthreads(ComputeReadTask1_NumThreadsPerThreadGroup)]
void ComputeReadTask1(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint x = dispatchThreadID.x % 32, y = dispatchThreadID.y % 32;
    float4 rgba = ComputeReadTask1_TextureIn[dispatchThreadID.xy];
    ComputeReadTask1_TextureOut[dispatchThreadID.xy] = float4(1.0f - rgba[0], 1.0f - rgba[1], 1.0f - rgba[2], 1.0f);
}