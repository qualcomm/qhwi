# Tile Shading Shaders

GLSL shaders for the qcom_tile_shading example. Shaders are automatically compiled during the CMake build process using `glslc` from the Vulkan SDK.

## Manual Compilation

To compile shaders manually:

```powershell
.\compile.ps1
```

## Shader Files

**GLSL (Active):**
- `vert1.glsl`, `vert2.glsl` - Vertex shaders
- `frag1.glsl`, `frag2.glsl` - Fragment shaders  
- `compute1.glsl`, `compute2.glsl` - Compute shaders

**HLSL (Reference only):**
- Corresponding `.hlsl` files kept for future use when DXC supports VK_QCOM_tile_shading
