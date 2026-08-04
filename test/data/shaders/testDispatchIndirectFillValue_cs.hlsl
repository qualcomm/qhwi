//
//
//  testDispatchIndirectFillValue_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

RWStructuredBuffer<uint> outBuffer : register(u0);

struct cBuffer
{
    uint referenceValue;
};

[[vk::push_constant]] ConstantBuffer<cBuffer> cb : register(b0);

[numThreads(4, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    outBuffer[dispatchThreadID] = cb.referenceValue;
}
