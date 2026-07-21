spirv-cross --hlsl ..\..\android\app\src\main\assets\shaders\vert1.glsl.spv -V --shader-model 50 > vert1.hlsl
spirv-cross --hlsl ..\..\android\app\src\main\assets\shaders\frag1.glsl.spv -V --shader-model 50 > frag1.hlsl
spirv-cross --hlsl ..\..\android\app\src\main\assets\shaders\vert2.glsl.spv -V --shader-model 50 > vert2.hlsl
spirv-cross --hlsl ..\..\android\app\src\main\assets\shaders\frag2.glsl.spv -V --shader-model 50 > frag2.hlsl