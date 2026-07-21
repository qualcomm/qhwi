//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

RWTexture2D<float4> ComputeWriteTask2_TextureOut : register(u0);
#define ComputeWriteTask2_NumThreadsPerThreadGroup 32, 32, 1

[numthreads(ComputeWriteTask2_NumThreadsPerThreadGroup)]
void ComputeWriteTask2(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    static const uint3 Colors[] =
    {
        uint3(128, 255, 255), uint3(0, 128, 255), uint3(255, 128, 192), uint3(255, 128, 255),
        uint3(0, 128, 192), uint3(128, 128, 192), uint3(255, 0, 255), uint3(255, 128, 64),
        uint3(0, 128, 128), uint3(0, 64, 128), uint3(128, 0, 64), uint3(255, 128, 0),
        uint3(128, 128, 0), uint3(128, 128, 128), uint3(64, 128, 128), uint3(255, 128, 128)
    };
    uint x = dispatchThreadID.x % 4, y = dispatchThreadID.y % 4;
    uint3 rgb = Colors[y * 4 + x];
    ComputeWriteTask2_TextureOut[dispatchThreadID.xy] = float4(rgb / 255.0f, 1.0f);
}
