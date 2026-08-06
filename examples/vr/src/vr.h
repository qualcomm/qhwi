//
//
//  vr.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#if defined(QHWI_PLATFORM_ANDROID)
#    define XR_USE_PLATFORM_ANDROID
#elif defined(QHWI_PLATFORM_WINDOWS)
#    define XR_USE_PLATFORM_WINDOWS
#endif
#ifdef QHWI_BACKEND_VULKAN
#    include <qhwi/qhwi_vk.h>
#    define XR_USE_GRAPHICS_API_VULKAN
#endif
#ifdef QHWI_BACKEND_D3D12
#    include <qhwi/qhwi_d3d12.h>
#    define XR_USE_GRAPHICS_API_D3D12
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace qhwi
{
#ifdef QHWI_BACKEND_VULKAN
class VrVkSwapchain : public qhwi::ext::VkSwapchain
{
public:
    struct Description : public qhwi::ext::VkSwapchain::Description
    {
        constexpr Description& setColorFormat(vk::Format v)
        {
            colorFormat = v;
            return *this;
        }

        vk::Format colorFormat{};

        constexpr Description& setDepthStencilFormat(vk::Format v)
        {
            depthStencilFormat = v;
            return *this;
        }

        vk::Format depthStencilFormat{};

        constexpr Description& setSurfaceWidth(uint32_t v)
        {
            surfaceWidth = v;
            return *this;
        }

        uint32_t surfaceWidth = 0;

        constexpr Description& setSurfaceHeight(uint32_t v)
        {
            surfaceHeight = v;
            return *this;
        }

        uint32_t surfaceHeight = 0;
    };

    VrVkSwapchain(XrSession xrSession, VkDevice* device, const Description& desc);
    ~VrVkSwapchain();
    void acquireNextImage() override;
    void releaseImage();

    XrSwapchain colorSwapchain() { return m_xrColorSwapchain; }

    Texture* getCurrentColorImage() const { return getCurrentImage(); }

    Texture* getCurrentDepthStencilImage() const { return m_depthStencilSwapchainTextures[m_currentImageIndex]; }

protected:
    void createXrSwapchain(uint32_t numImages, bool vsync);
    void createXrSwapchainImages(uint32_t numImages, const char* name, bool autoBarrier);

private:
    uint32_t m_surfaceWidth = 0, m_surfaceHeight = 0;
    vk::Format m_depthStencilFormat{};
    XrSwapchain m_xrColorSwapchain{}, m_xrDepthStencilSwapchain{};
    XrSession m_xrSession{};
    std::vector<XrSwapchainImageVulkanKHR> m_xrColorSwapchainImages;
    std::vector<XrSwapchainImageVulkanKHR> m_xrDepthStencilSwapchainImages;
    qre::RefCountPtr<qhwi::VkTexture> m_depthStencilSwapchainTextures[MaxSwapchainImages];
};

using VrVkSwapchainDesc = VrVkSwapchain::Description;
#endif
#ifdef QHWI_BACKEND_D3D12
class VrD3d12Swapchain : public qhwi::ext::DxgiSwapchain
{
public:
    struct Description : public qhwi::ext::DxgiSwapchain::Description
    {
        constexpr Description& setColorFormat(DXGI_FORMAT v)
        {
            colorFormat = v;
            return *this;
        }

        DXGI_FORMAT colorFormat{};

        constexpr Description& setDepthStencilFormat(DXGI_FORMAT v)
        {
            depthStencilFormat = v;
            return *this;
        }

        DXGI_FORMAT depthStencilFormat{};

        constexpr Description& setSurfaceWidth(uint32_t v)
        {
            surfaceWidth = v;
            return *this;
        }

        uint32_t surfaceWidth = 0;

        constexpr Description& setSurfaceHeight(uint32_t v)
        {
            surfaceHeight = v;
            return *this;
        }

        uint32_t surfaceHeight = 0;
    };

    VrD3d12Swapchain(XrSession xrSession, D3dDevice* device, const Description& desc);
    ~VrD3d12Swapchain();
    void acquireNextImage() override;
    void releaseImage();

    XrSwapchain colorSwapchain() { return m_xrColorSwapchain; }

    Texture* getCurrentColorImage() const { return getCurrentImage(); }

    Texture* getCurrentDepthStencilImage() const { return m_depthStencilSwapchainTextures[m_currentImageIndex]; }

protected:
    void createXrSwapchain(uint32_t numImages, bool vsync);
    void createXrSwapchainImages(uint32_t numImages, const char* name, bool autoBarrier);

private:
    uint32_t m_surfaceWidth = 0, m_surfaceHeight = 0;
    DXGI_FORMAT m_depthStencilFormat{};
    XrSwapchain m_xrColorSwapchain{}, m_xrDepthStencilSwapchain{};
    XrSession m_xrSession{};
    std::vector<XrSwapchainImageD3D12KHR> m_xrColorSwapchainImages;
    std::vector<XrSwapchainImageD3D12KHR> m_xrDepthStencilSwapchainImages;
    qre::RefCountPtr<qhwi::D3dTexture> m_depthStencilSwapchainTextures[MaxSwapchainImages];
};

using VrD3d12SwapchainDesc = VrD3d12Swapchain::Description;
#endif
class VrExample
{
public:
    virtual ~VrExample();
#ifdef QHWI_PLATFORM_ANDROID
    void init(android_app* app);
#endif
    struct Config
    {
        uint32_t numFrames = 0;
        bool debug = true;
        GraphicsBackend backend;
    };

    virtual void init(const Config& config) = 0;
    void run();
    void destroy();

protected:
    static constexpr Format ColorFormat = Format::eRgba8Unorm;
    static constexpr Format DepthStencilFormat = Format::eD32Float;
    void initXrLoader();
    void createXrInstance(GraphicsBackend graphicsBackend);
    void createXrDebugMessenger();
    void getXrSystem();
    void getXrViewConfigurationViews();
    void handleInput();
#ifdef QHWI_PLATFORM_ANDROID
    static void handleCommandCb(android_app* app, int32_t cmd);
    void handleCommand(int32_t cmd);
#endif
    void createScene();
    void createXrReferenceSpace();
    void createCommandLists();
    void createFrameFences();

    struct RenderLayerInfo
    {
        XrTime predictedDisplayTime = 0;
        std::vector<XrCompositionLayerBaseHeader*> xrLayers;
        XrCompositionLayerProjection xrLayerProjection = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        std::vector<XrCompositionLayerProjectionView> xrLayerProjectionViews;
    };

    bool renderLayer(RenderLayerInfo& renderLayerInfo);
    void renderFrame();
    void beginRenderPass(CommandList* cmdList);
    void recordCommandList(CommandList* commandList);
    void endRenderPass(CommandList* cmdList);
    void beginTransfer();
    void endTransfer();

    int nextCommandListIndex() { return (m_currentCommandListIndex + 1) % m_numCommandLists; }

    uint32_t viewWidth() const { return m_xrViewConfigurationViews[0].recommendedImageRectWidth; }

    uint32_t viewHeight() const { return m_xrViewConfigurationViews[0].recommendedImageRectHeight; }

    void handleSystemEvents();
    void handleXrEvents();

    static XrBool32 xrMessageCb(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                XrDebugUtilsMessageTypeFlagsEXT messageType,
                                const XrDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                void* pUserData);
    qre::RefCountPtr<qhwi::Context> m_ctx;
    qre::RefCountPtr<qhwi::Device> m_device;
    qre::RefCountPtr<qhwi::CommandQueue> m_primaryCommandQueue;
#ifdef QHWI_PLATFORM_ANDROID
    android_app* m_app = nullptr;
    ANativeWindow* m_nativeWindow = nullptr;
#endif
    struct State
    {
        bool resumed = false;
        bool sceneCreated = false;
        bool sessionRunning = false;
        bool appRunning = false;
        XrSessionState xrSessionState = XR_SESSION_STATE_UNKNOWN;
    };

    State m_state;
    uint64_t m_frameIndex = 1;
    double m_displayTime = 0.0;
    int m_currentCommandListIndex = -1;
    uint32_t m_numCommandLists = 0;
    qhwi::CommandList* m_currentCommandList = nullptr;
    static constexpr int NumSwapchainImages = 3;
    qre::RefCountPtr<qhwi::Fence> m_frameFences[NumSwapchainImages];
    qre::RefCountPtr<qhwi::CommandList> m_commandLists[NumSwapchainImages];
    qre::RefCountPtr<qhwi::CommandList> m_copyCommandList;
    test_util::DrawCubeTask m_drawCubeTask;
    float m_rot[3] = { 0.0f };

    XrInstance m_xrInstance{};
    XrDebugUtilsMessengerEXT m_xrDebugMessenger{};
    XrSession m_xrSession{};
    XrSystemId m_xrSystemId;
    std::vector<XrViewConfigurationView> m_xrViewConfigurationViews;
    XrSpace m_xrLocalSpace{};

    virtual XrSwapchain colorSwapchain() = 0;
    virtual Texture* getCurrentSwapchainColorImage() = 0;
    virtual Texture* getCurrentSwapchainDepthStencilImage() = 0;
    virtual void releaseSwapchainImage() = 0;

    std::unique_ptr<ext::Swapchain> m_swapchain;
    Config m_cfg;
};

#ifdef QHWI_BACKEND_VULKAN
class VrVkExample : public VrExample
{
public:
    void init(const Config& config) override;

protected:
    void createSwapchain();
    void createXrSession();
    void loadPFNXrFunctions();

    XrSwapchain colorSwapchain() override { return vrVkSwapchain()->colorSwapchain(); }

    Texture* getCurrentSwapchainColorImage() override { return vrVkSwapchain()->getCurrentColorImage(); }

    Texture* getCurrentSwapchainDepthStencilImage() override { return vrVkSwapchain()->getCurrentDepthStencilImage(); }

    void releaseSwapchainImage() override { vrVkSwapchain()->releaseImage(); }

    VrVkSwapchain* vrVkSwapchain() { return static_cast<VrVkSwapchain*>(m_swapchain.get()); }

private:
    std::vector<std::string> getXrVulkanInstanceExtensions();
    std::vector<std::string> getXrVulkanDeviceExtensions();
    XrGraphicsBindingVulkan2KHR m_xrGraphicsBindingVk{};
    PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;
    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
};
#endif
#ifdef QHWI_BACKEND_D3D12
class VrD3d12Example : public VrExample
{
public:
    void init(const Config& config) override;

protected:
    void createSwapchain();
    void createXrSession();
    void loadPFNXrFunctions();

    XrSwapchain colorSwapchain() override { return vrD3d12Swapchain()->colorSwapchain(); }

    Texture* getCurrentSwapchainColorImage() override { return vrD3d12Swapchain()->getCurrentColorImage(); }

    Texture* getCurrentSwapchainDepthStencilImage() override { return vrD3d12Swapchain()->getCurrentDepthStencilImage(); }

    void releaseSwapchainImage() override { vrD3d12Swapchain()->releaseImage(); }

    VrD3d12Swapchain* vrD3d12Swapchain() { return static_cast<VrD3d12Swapchain*>(m_swapchain.get()); }

private:
    PFN_xrGetD3D12GraphicsRequirementsKHR xrGetD3D12GraphicsRequirementsKHR = nullptr;
    XrGraphicsBindingD3D12KHR m_xrGraphicsBindingD3d12{};
};
#endif
} // namespace qhwi
