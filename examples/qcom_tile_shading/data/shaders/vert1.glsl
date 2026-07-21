#version 310 es

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 in_texcoord;
layout (location = 0) out vec2 texcoord;

void main()
{
    gl_Position = position;
    texcoord    = in_texcoord;
}