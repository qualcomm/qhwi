//
//
//  testMeshletPipeline_ms.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#define NUM_PRIMITIVES 4
#define NUM_VERTICES NUM_PRIMITIVES * 3

struct MeshPayload
{
    float vertexOffsetY;
};

struct VertexPosColor
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};

StructuredBuffer<VertexPosColor> vertexPosColBuffer : register(t0);
ByteAddressBuffer indicesBuffer : register(t1);

[numThreads(NUM_PRIMITIVES, 1, 1)]

[outputtopology("triangle")]

void MSMain(
    out indices uint3 primitiveIndices[NUM_PRIMITIVES],
    out vertices VertexPosColor primitiveVertices[NUM_VERTICES],
    in payload MeshPayload meshPayload,
    uint threadId : SV_DispatchThreadID)
{
    SetMeshOutputCounts(NUM_VERTICES, NUM_PRIMITIVES);

     VertexPosColor vert1 = vertexPosColBuffer[0];
     VertexPosColor vert2 = vertexPosColBuffer[1];
     VertexPosColor vert3 = vertexPosColBuffer[2];

     vert1.pos.x += 0.5 * threadId;
     vert2.pos.x += 0.5 * threadId;
     vert3.pos.x += 0.5 * threadId;

     vert1.pos.y += meshPayload.vertexOffsetY;
     vert2.pos.y += meshPayload.vertexOffsetY;
     vert3.pos.y += meshPayload.vertexOffsetY;

    if (threadId < NUM_PRIMITIVES)
    {
        primitiveVertices[threadId * 3 + 0] = vert1;
        primitiveVertices[threadId * 3 + 1] = vert2;
        primitiveVertices[threadId * 3 + 2] = vert3;
    }

    if (threadId < NUM_PRIMITIVES)
    {
        uint packedIndices = indicesBuffer.Load(0);
        uint indexOffsetX = 3 * threadId;
        primitiveIndices[threadId] = uint3((packedIndices         & 0xff) + indexOffsetX,
                                           ((packedIndices >> 8)  & 0xff) + indexOffsetX,
                                           ((packedIndices >> 16) & 0xff) + indexOffsetX);
    }
}
