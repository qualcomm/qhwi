#version 310 es

precision highp float;
precision highp sampler;

layout (set = 0, binding = 0) uniform texture2D tex;
layout (set = 0, binding = 2) uniform sampler   tex_sampler;
layout (location = 0) in mediump vec2 texcoord;
layout (location = 1) out lowp vec4 oColour;

void main (void)
{
   oColour = texture(sampler2D(tex, tex_sampler),  texcoord, 1.0);
}