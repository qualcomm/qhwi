# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-07-16

Initial public release.

### Added

- Graphics and compute pipeline support over Vulkan and D3D12 backends
- Device enumeration and feature querying
- Mesh and amplification shader support
- Ray queries and ray tracing acceleration structures
- Render-to-texture and multiple render target support
- Multisampling (MSAA)
- 2D, 3D, 2D array, and cubemap texture support
- Swapchain and window creation
- Bindless rendering model
- GPU profiling via timer queries
- Explicit and automatic resource barriers
- Single-pass mipmap generation
- HDR rendering
- MultiView and VR rendering support
- Specialization constants (Vulkan)
- D3D12 enhanced barriers
- Qualcomm Vulkan extensions: `VK_QCOM_IMAGE_PROCESSING`, `VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS`, `VK_QCOM_MULTIVIEW_PER_VIEW_RENDER_AREAS`, `VK_QCOM_TILE_PROPERTIES`, `VK_QCOM_TILE_SHADING`, `VK_QCOM_TILE_MEMORY_HEAP`, `VK_QCOM_RENDER_PASS_SHADER_RESOLVE`
- Hardware-accelerated optical flow via `VK_NV_OPTICAL_FLOW`
- Example applications: `gpl`, `qcom_image_proc`, `qcom_multiview`, `qcom_tile_shading`, `qcom_tile_memory_heap`, `vk_custom_resolve`, `vr`
- Unit test suite covering both graphics backends

[Unreleased]: https://github.com/qualcomm/qhwi/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/qualcomm/qhwi/releases/tag/v1.0.0
