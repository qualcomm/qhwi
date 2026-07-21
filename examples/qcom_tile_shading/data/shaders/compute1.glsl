#version 460
#extension GL_QCOM_tile_shading : enable

layout(std140, binding = 0) buffer SSBOOut {
   vec4 DataOut[ ];
};

layout(set = 0, binding = 1, tile_attachmentQCOM, rgba32f) uniform highp image2D color0;

layout(set = 0, binding = 2, tile_attachmentQCOM) buffer TransientOut {
    vec4 dataTransient[ ];
};

layout(set = 0, binding = 3, tile_attachmentQCOM, rgba32f) uniform highp image2D color1;

layout (shading_rate_xQCOM = 1, shading_rate_yQCOM = 1, shading_rate_zQCOM = 1) in;

void main()
{
    ivec2 pixelLocation = ivec2(gl_TileOffsetQCOM.xy + gl_GlobalInvocationID.xy);

    vec4 finalColor = vec4(0);
    int offset = 2;
    for(int x = -offset; x <= offset; x++) {
        for(int y = -offset; y <= offset; y++) {
            vec4 pixelColor = imageLoad(color0, pixelLocation + ivec2(x,y));
            finalColor += pixelColor / ((offset * 2 + 1)*(offset * 2 + 1));
        }
    }

    imageStore(color1, pixelLocation, finalColor);

    // The transient buffer size is sizeof(float4)
    dataTransient[0] = vec4(gl_TileDimensionQCOM.x,
                            gl_TileDimensionQCOM.y,
                            gl_TileOffsetQCOM.x,
                            gl_TileOffsetQCOM.y);
}