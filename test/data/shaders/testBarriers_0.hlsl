//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct ComputeWriteTask0_BufferData { uint v; };
RWStructuredBuffer<ComputeWriteTask0_BufferData> ComputeWriteTask0_BufferOut : register(u0);
#define ComputeWriteTask0_NumThreadsPerThreadGroup 1024, 1, 1

[numthreads(ComputeWriteTask0_NumThreadsPerThreadGroup)]
void ComputeWriteTask0(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ComputeWriteTask0_BufferOut[dispatchThreadID.x].v = dispatchThreadID.x;
}
