//
//
//  downscale_qcom_ps.glsl
//
//  Copyright (C) 2021-2024 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#version 450

#extension GL_QCOM_image_processing : require
#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

// Inputs and outputs for fragment shader
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 fragColor;

#define S_OFFSET 128

// Texture and sampler inputs
layout(set = 0, binding = 0) uniform highp texture2D inputTex;
layout(set = 0, binding = S_OFFSET + 0) uniform highp sampler samplerLinear;

layout(set = 0, binding = 1) uniform highp texture2DArray kernelTex;
layout(set = 0, binding = S_OFFSET + 1) uniform highp sampler samplerKernel;

void main()
{
    fragColor = textureWeightedQCOM(sampler2D(inputTex, samplerLinear), uv, sampler2DArray(kernelTex, samplerKernel));
}
