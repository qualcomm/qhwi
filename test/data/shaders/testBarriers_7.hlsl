//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct ComputeWriteTask3_BufferData { uint v; };
RWStructuredBuffer<ComputeWriteTask3_BufferData> ComputeWriteTask3_Buffer : register(u0);
#define ComputeWriteTask3_NumThreadsPerThreadGroup 1, 1, 1

[numthreads(ComputeWriteTask3_NumThreadsPerThreadGroup)]
void ComputeWriteTask3(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    for (uint j = 0; j < 16; j++)
    {
        ComputeWriteTask3_Buffer[j].v *= 3;
    }
}
