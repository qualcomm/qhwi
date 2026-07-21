//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct ComputeWriteTask1_BufferData { uint2 i; };
RWStructuredBuffer<ComputeWriteTask1_BufferData> ComputeWriteTask1_BufferOut : register(u0);
#define ComputeWriteTask1_NumThreadsPerThreadGroup 4, 1, 1

[numthreads(ComputeWriteTask1_NumThreadsPerThreadGroup)]
void ComputeWriteTask1(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ComputeWriteTask1_BufferOut[dispatchThreadID.x].i = uint2(0, dispatchThreadID.x + 1);
}
