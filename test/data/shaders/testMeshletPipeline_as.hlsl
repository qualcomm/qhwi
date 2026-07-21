//
//
//  testMeshletPipeline_as.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct MeshPayload
{
    float vertexOffsetY;
};

groupshared MeshPayload payload;

[numThreads(1, 1, 1)]

void ASMain(uint groupId : SV_GroupID)
{
    payload.vertexOffsetY = -0.5 * groupId;
    DispatchMesh(1, 1, 1, payload);
}
