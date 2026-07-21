//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct ComputeWriteTask0_BufferData { uint v; };
StructuredBuffer<ComputeWriteTask0_BufferData> ComputeReadTask0_BufferIn : register(t0);
RWStructuredBuffer<ComputeWriteTask0_BufferData> ComputeReadTask0_BufferOut : register(u0);

#define ComputeReadTask0_NumThreadsPerThreadGroup 1024, 1, 1

[numthreads(ComputeReadTask0_NumThreadsPerThreadGroup)]
void ComputeReadTask0(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    ComputeReadTask0_BufferOut[dispatchThreadID.x].v = ComputeReadTask0_BufferIn[dispatchThreadID.x].v;
}
