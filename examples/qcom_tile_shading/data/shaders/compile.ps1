# Manual shader compilation script for qcom_tile_shading example
# Note: Shaders are automatically compiled as part of the CMake build process.
# This script is provided for manual compilation if needed.
#
# Uses standard glslangValidator from Vulkan SDK (GLSL only)
# HLSL support will be enabled once DXC properly supports VK_QCOM_tile_shading extension

$OUTPUT_DIR="$PSScriptRoot/../../android/app/src/main/assets/shaders"
mkdir -Force -p $OUTPUT_DIR

# Use glslangValidator from Vulkan SDK
$GLSLANG = "glslangValidator"
if ($env:VULKAN_SDK) {
    $GLSLANG = "$env:VULKAN_SDK/Bin/glslangValidator.exe"
}

Write-Host "Compiling GLSL shaders with glslangValidator..."

# Compile vertex shaders
& $GLSLANG -V -S vert -o "$OUTPUT_DIR/vert1.glsl.spv" ./vert1.glsl
& $GLSLANG -V -S vert -o "$OUTPUT_DIR/vert2.glsl.spv" ./vert2.glsl

# Compile fragment shaders
& $GLSLANG -V -S frag -o "$OUTPUT_DIR/frag1.glsl.spv" ./frag1.glsl
& $GLSLANG -V -S frag -o "$OUTPUT_DIR/frag2.glsl.spv" ./frag2.glsl

# Compile compute shaders
& $GLSLANG -V -S comp -o "$OUTPUT_DIR/compute1.glsl.spv" ./compute1.glsl
& $GLSLANG -V -S comp -o "$OUTPUT_DIR/compute2.glsl.spv" ./compute2.glsl

Write-Host "Shader compilation complete!"
Write-Host "Output directory: $OUTPUT_DIR"
