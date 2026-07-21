#
#  compileShaders.py
#  QHWI
#
#  Copyright (C) 2023-2024 Qualcomm Technologies, Inc.
#  SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple

sys.path.append('../../../examples/common/scripts')
import shaderUtil
from shaderUtil import *

def main():
    init()
    Opts = shaderUtil.Opts
    enableSER = Opts.enableSER if hasattr(Opts, 'enableSER') else False
    compileShaders('testUtilDrawColor')
    compileShaders('testUtilDrawColor1')
    compileShaders('testUtilDrawImage')
    compileShaders('testUtilDrawTriangle')
    compileShaders('testComputePipeline', [ computeShaderDesc() ])
    compileShaders('testUavBuffer', [ computeShaderDesc() ])
    compileShaders('testBindless')
    compileShaders('testPushConstants')
    compileShaders('testDepthStencilBuffer')
    compileShaders('testTexture3d', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('testTexture2dArray', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('testTextureCube', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('testDrawIndirect', [ vertexShaderDesc(), pixelShaderDesc(), computeShaderDesc() ])
    compileShaders('testDrawIndexedIndirect', [ computeShaderDesc() ])
    compileShaders('testDispatchIndirectFillArg', [ computeShaderDesc() ])
    compileShaders('testDispatchIndirectFillValue', [ computeShaderDesc() ])
    compileShaders('testMeshletPipeline', [ amplificationShaderDesc(), meshShaderDesc(), pixelShaderDesc() ])
    compileShaders('testBarriers',
    [
        computeShaderDesc('_0.hlsl', 'ComputeWriteTask0'),
        computeShaderDesc('_1.hlsl', 'ComputeReadTask0'),
        computeShaderDesc('_2.hlsl', 'ComputeWriteTask1'),
        vertexShaderDesc('_3.hlsl', 'DrawReadTask0_VS_Main'),
        pixelShaderDesc('_3.hlsl', 'DrawReadTask0_PS_Main'),
        computeShaderDesc('_4.hlsl', 'ComputeWriteTask2'),
        vertexShaderDesc('_5.hlsl', 'DrawSampleTask0_VS_Main'),
        pixelShaderDesc('_5.hlsl', 'DrawSampleTask0_PS_Main'),
        computeShaderDesc('_6.hlsl', 'ComputeReadTask1'),
        computeShaderDesc('_7.hlsl', 'ComputeWriteTask3')
    ])
    compileShaders('testRayTracing', [ computeShaderDesc()])
    compileShaders('testByteAddressBuffer', [ computeShaderDesc() ])
    compileShaders('testRwByteAddressBuffer', [ computeShaderDesc() ])
    compileShaders('testMultipleRenderTargets1')
    compileShaders('testMultipleRenderTargets2')
    compileShaders('testRenderToCubemap')
    compileShaders('sampleTextureCube')
    if Opts.enableVulkanBackend:
        compileShadersSpv('testSpecializationConstants', [ vertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('testTypedBuffer', [ computeShaderDesc() ])
    compileShaders('testMultiView', [ multiViewVertexShaderDesc('.hlsl'), pixelShaderDesc('.hlsl') ])
    compileShaders('testRenderPasses_0', [ vertexShaderDesc('_vs.hlsl'), pixelShaderDesc('_ps.hlsl') ])
    compileShaders('testRenderPasses_1', [ vertexShaderDesc('_vs.hlsl'), pixelShaderDesc('_ps.hlsl') ])
    compileShaders('testRayPipeline',
    [
        computeShaderDesc('.hlsl', 'RayGeneration_Main', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DRAYGEN_SHADER'),
        computeShaderDesc('.hlsl', 'Miss_Main', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DMISS_SHADER'),
        computeShaderDesc('.hlsl', 'ClosestHit_Main', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DHIT_SHADER')
    ])
    compileShaders('testRayPipeline_sbt',
    [
        computeShaderDesc('.hlsl', 'RayGeneration_Main', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DRAYGEN_SHADER'),
        computeShaderDesc('.hlsl', 'Miss_Main', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DMISS_SHADER'),
        computeShaderDesc('.hlsl', 'ClosestHit_Red', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DHIT_SHADER'),
        computeShaderDesc('.hlsl', 'ClosestHit_Blue', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DHIT_SHADER')
    ])
    compileShaders('testRayPipeline_normal',
    [
        computeShaderDesc('.hlsl', 'RayGeneration_Depth', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DRAYGEN_SHADER'),
        computeShaderDesc('.hlsl', 'Miss_Depth', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DMISS_SHADER'),
        computeShaderDesc('.hlsl', 'ClosestHit_Normal', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DHIT_SHADER'),
        computeShaderDesc('.hlsl', 'ClosestHit_RayDepth', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DHIT_SHADER'),
        computeShaderDesc('.hlsl', 'ClosestHit_Color', 'lib_6_4', '-fspv-target-env=vulkan1.1spirv1.4 -DHIT_SHADER')
    ])
    # Compile SER shaders only when QHWI_ENABLE_SER is enabled
    if enableSER:
        compileShaders('testRayPipeline_normal_SER',
        [
            computeShaderDesc('.hlsl', 'RayGeneration_Depth', 'lib_6_9', '-fspv-target-env=vulkan1.3 -DUSE_SER -DRAYGEN_SHADER -fspv-extension=SPV_KHR_ray_tracing -fspv-extension=SPV_NV_shader_subgroup_partitioned'),
            computeShaderDesc('.hlsl', 'Miss_Depth', 'lib_6_9', '-fspv-target-env=vulkan1.3 -DUSE_SER -DMISS_SHADER -fspv-extension=SPV_KHR_ray_tracing -fspv-extension=SPV_NV_shader_subgroup_partitioned'),
            computeShaderDesc('.hlsl', 'ClosestHit_Normal', 'lib_6_9', '-fspv-target-env=vulkan1.3 -DUSE_SER -DHIT_SHADER -fspv-extension=SPV_KHR_ray_tracing -fspv-extension=SPV_NV_shader_subgroup_partitioned'),
            computeShaderDesc('.hlsl', 'ClosestHit_RayDepth', 'lib_6_9', '-fspv-target-env=vulkan1.3 -DUSE_SER -DHIT_SHADER -fspv-extension=SPV_KHR_ray_tracing -fspv-extension=SPV_NV_shader_subgroup_partitioned'),
            computeShaderDesc('.hlsl', 'ClosestHit_Color', 'lib_6_9', '-fspv-target-env=vulkan1.3 -DUSE_SER -DHIT_SHADER -fspv-extension=SPV_KHR_ray_tracing -fspv-extension=SPV_NV_shader_subgroup_partitioned')
        ])
main()
