//
//
//  testByteAddressBuffer_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

ByteAddressBuffer  inBuffer  : register(t0);
RWStructuredBuffer<uint4> outBuffer : register(u0);

[numThreads(1, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    outBuffer[dispatchThreadID] = inBuffer.Load4(dispatchThreadID * sizeof(uint4));
}
