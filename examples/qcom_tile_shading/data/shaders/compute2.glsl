#version 460
#extension GL_QCOM_tile_shading : enable

layout(std140, binding = 0) buffer SSBOOut {
   vec4 DataOut[ ];
};
layout(set = 0, binding = 1, tile_attachmentQCOM, rgba32f) uniform highp image2D color0;

layout(set = 0, binding = 2, tile_attachmentQCOM) buffer TransientOut {
    vec4 dataTransient[ ];
};

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    DataOut[0] = dataTransient[0];
}