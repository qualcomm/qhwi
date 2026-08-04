//
//
//  testDrawIndirect_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct DrawArg
{
    uint32_t vertexCountPerInstance;
    uint32_t instanceCount;
    uint32_t firstVertexLocation;
    uint32_t firstInstanceLocation;
};

struct CB
{
    uint32_t numVertices;
    uint32_t numInstances;
};

ConstantBuffer<CB> constantBuffer : register(b0);
RWStructuredBuffer<DrawArg> drawArg : register(u0);

[numThreads(4, 1, 1)]
 void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    DrawArg arg;

    arg.vertexCountPerInstance = constantBuffer.numVertices;
    arg.instanceCount          = constantBuffer.numInstances;
    arg.firstVertexLocation    = 0;
    arg.firstInstanceLocation  = dispatchThreadID.x * constantBuffer.numInstances;

    drawArg[dispatchThreadID.x] = arg;
}
