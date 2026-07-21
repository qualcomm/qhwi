//
//
//  vectorAddition_cs.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
struct BufferType
{
    int value;
};

StructuredBuffer<BufferType> BufferIn1 : register(t0);
StructuredBuffer<BufferType> BufferIn2 : register(t1);
RWStructuredBuffer<BufferType> BufferOut : register(u0);

[numthreads(16, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    BufferOut[dispatchThreadID.x].value = BufferIn1[dispatchThreadID.x].value + BufferIn2[dispatchThreadID.x].value;
}
