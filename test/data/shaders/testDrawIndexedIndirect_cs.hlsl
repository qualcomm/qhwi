//
//
//  testDrawIndexedIndirect_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct DrawIndexedArg
{
    uint32_t indexCountPerInstance;
    uint32_t instanceCount;
    uint32_t firstIndexLocation;
    int32_t  baseVertexLocation;
    uint32_t firstInstanceLocation;
};

struct CB
{
    uint32_t numIndices;
    uint32_t numInstances;
};

ConstantBuffer<CB> constantBuffer : register(b0);
RWStructuredBuffer<DrawIndexedArg> drawIndexedArg : register(u0);

[numThreads(4, 1, 1)]
 void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
     DrawIndexedArg arg;

     arg.indexCountPerInstance = constantBuffer.numIndices;
     arg.instanceCount         = constantBuffer.numInstances;
     arg.firstIndexLocation    = 0;
     arg.baseVertexLocation    = 0;
     arg.firstInstanceLocation = dispatchThreadID.x * constantBuffer.numInstances;

     drawIndexedArg[dispatchThreadID.x] = arg;
}
