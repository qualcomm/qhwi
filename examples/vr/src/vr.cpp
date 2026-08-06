//
//
//  vr.cpp
//  QHWI
// Based on reference code from: https://github.com/KhronosGroup/OpenXR-Tutorials
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <qhwi/qhwi_android_window.h>
#endif
#include <fstream>
#include <sstream>
#include <qre/text/textutil.h>
#include <test_util.h>
#include "vr.h"
#include <cxxopts.hpp>

using namespace qhwi;
using namespace qre::math;

static Quatf toQre(const XrQuaternionf& q)
{
    return { q.x, q.y, q.z, q.w };
};

static Vec3f toQre(const XrVector3f& v)
{
    return { v.x, v.y, v.z };
};

static constexpr float Pi = float(M_PI);
static constexpr float ToRad = Pi / 180.0f;

static void toXrProj(float* q, float near, float far)
{
    q[5] *= -1.0f;
    q[8] *= -1.0f;
    q[10] *= far / near;
    q[11] *= -1.0f;
    q[14] *= -1.0f;
}

#define XR_V(fn)                                                                                                                                                                   \
    {                                                                                                                                                                              \
        XrResult r = (fn);                                                                                                                                                         \
        if (!XR_SUCCEEDED(r))                                                                                                                                                      \
        {                                                                                                                                                                          \
            QHWI_FATAL("function: %s, result: %d", #fn, int(r));                                                                                                                   \
        }                                                                                                                                                                          \
    }

static std::vector<std::string> split(const std::string& str)
{
    std::stringstream ss(str.data());
    std::vector<std::string> v;
    std::string s;
    while (std::getline(ss, s, ' '))
    {
        v.push_back(s);
    }
    return v;
}

VrExample::~VrExample()
{
    if (m_primaryCommandQueue)
    {
        m_primaryCommandQueue->waitForIdle();
    }
}

void VrExample::initXrLoader()
{
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    XR_V(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR));
    if (!xrInitializeLoaderKHR)
    {
        return;
    }
#if defined(QHWI_PLATFORM_ANDROID)
    XrLoaderInitInfoAndroidKHR s{ XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
    s.applicationVM = m_app->activity->vm;
    s.applicationContext = m_app->activity->clazz;
    XR_V(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&s));
#endif
}

void VrExample::createXrInstance(GraphicsBackend graphicsBackend)
{
    XrApplicationInfo appInfo;
    qre::PrintfSafe(appInfo.applicationName, "qhwi_example_vr");
    appInfo.applicationVersion = 1;
    qre::PrintfSafe(appInfo.engineName, "qhwi");
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;

    qre::StaticVector<const char*, 2> instanceExtensions = { XR_EXT_DEBUG_UTILS_EXTENSION_NAME };
#ifdef QHWI_BACKEND_VULKAN
    if (graphicsBackend == qhwi::GraphicsBackend::eVulkan)
    {
        instanceExtensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
    }
#endif
#ifdef QHWI_BACKEND_D3D12
    if (graphicsBackend == qhwi::GraphicsBackend::eDirectX12)
    {
        instanceExtensions.push_back(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
    }
#endif

    XrInstanceCreateInfo s{ XR_TYPE_INSTANCE_CREATE_INFO };
    s.createFlags = 0;
    s.applicationInfo = appInfo;
    s.enabledApiLayerCount = 0;
    s.enabledApiLayerNames = nullptr;
    s.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    s.enabledExtensionNames = instanceExtensions.data();
    XR_V(xrCreateInstance(&s, &m_xrInstance));
}

void VrExample::getXrSystem()
{
    XrSystemGetInfo s{ XR_TYPE_SYSTEM_GET_INFO };
    s.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XR_V(xrGetSystem(m_xrInstance, &s, &m_xrSystemId));
}
#ifdef QHWI_BACKEND_VULKAN
void VrVkExample::createXrSession()
{
    auto vkDevice = static_cast<qhwi::VkDevice*>(m_device.Get());
    auto vkQueue = static_cast<qhwi::VkCommandQueue*>(m_primaryCommandQueue.Get());
    const auto& vkQueueDesc = vkQueue->vkDesc()->vk;

    auto& b = m_xrGraphicsBindingVk;
    b.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
    b.instance = vkDevice->getInstance();
    b.physicalDevice = vkDevice->getPhysicalDevice();
    b.device = vkDevice->get();
    b.queueFamilyIndex = vkQueueDesc.queueFamilyIndex;
    b.queueIndex = vkQueueDesc.queueIndex;

    XrSessionCreateInfo s{ XR_TYPE_SESSION_CREATE_INFO };
    s.next = &b;
    s.createFlags = 0;
    s.systemId = m_xrSystemId;
    XR_V(xrCreateSession(m_xrInstance, &s, &m_xrSession));
}
#endif

#ifdef QHWI_BACKEND_D3D12
void VrD3d12Example::createXrSession()
{
    auto d3dDevice = static_cast<qhwi::D3dDevice*>(m_device.Get());
    auto d3dQueue = static_cast<qhwi::D3dCommandQueue*>(m_primaryCommandQueue.Get());

    auto& b = m_xrGraphicsBindingD3d12;
    b.type = XR_TYPE_GRAPHICS_BINDING_D3D12_KHR;
    b.device = d3dDevice->get();
    b.queue = d3dQueue->get();

    XrSessionCreateInfo s{ XR_TYPE_SESSION_CREATE_INFO };
    s.next = &b;
    s.createFlags = 0;
    s.systemId = m_xrSystemId;
    XR_V(xrCreateSession(m_xrInstance, &s, &m_xrSession));
}
#endif

XrBool32 VrExample::xrMessageCb(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                XrDebugUtilsMessageTypeFlagsEXT messageType,
                                const XrDebugUtilsMessengerCallbackDataEXT* data,
                                void* pUserData)
{
    std::stringstream debugMessage;
    debugMessage << "[" << data->messageId << "][" << (data->functionName ? data->functionName : "") << "] : " << (data->message ? data->message : "");
    if (messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        QHWI_LOGW("%s", debugMessage.str().c_str());
    }
    else
    {
        QHWI_LOGV("%s", debugMessage.str().c_str());
    }
    return XR_FALSE;
}

void VrExample::createXrDebugMessenger()
{
    XrDebugUtilsMessengerCreateInfoEXT s{ XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    s.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    s.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                     XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    s.userCallback = (PFN_xrDebugUtilsMessengerCallbackEXT)xrMessageCb;
    s.userData = nullptr;

    // Load xrCreateDebugUtilsMessengerEXT() function pointer as it is not default loaded by the OpenXR loader.
    PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT;
    XR_V(xrGetInstanceProcAddr(m_xrInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&xrCreateDebugUtilsMessengerEXT));

    // Finally create and return the XrDebugUtilsMessengerEXT.
    XR_V(xrCreateDebugUtilsMessengerEXT(m_xrInstance, &s, &m_xrDebugMessenger));
}

void VrExample::getXrViewConfigurationViews()
{
    constexpr XrViewConfigurationType ViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    uint32_t viewConfigurationViewCount = 0;
    XR_V(xrEnumerateViewConfigurationViews(m_xrInstance, m_xrSystemId, ViewConfigType, 0, &viewConfigurationViewCount, nullptr));
    m_xrViewConfigurationViews.resize(viewConfigurationViewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    XR_V(xrEnumerateViewConfigurationViews(m_xrInstance, m_xrSystemId, ViewConfigType, viewConfigurationViewCount, &viewConfigurationViewCount, m_xrViewConfigurationViews.data()));
}

#ifdef QHWI_BACKEND_VULKAN
void VrVkExample::loadPFNXrFunctions()
{
    XR_V(xrGetInstanceProcAddr(m_xrInstance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsRequirementsKHR));
    XR_V(xrGetInstanceProcAddr(m_xrInstance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&xrGetVulkanInstanceExtensionsKHR));
    XR_V(xrGetInstanceProcAddr(m_xrInstance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)&xrGetVulkanDeviceExtensionsKHR));
    XR_V(xrGetInstanceProcAddr(m_xrInstance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDeviceKHR));
}

std::vector<std::string> VrVkExample::getXrVulkanInstanceExtensions()
{
    uint32_t size = 0;
    std::string data;
    XR_V(xrGetVulkanInstanceExtensionsKHR(m_xrInstance, m_xrSystemId, 0, &size, nullptr));
    data.resize(size);
    XR_V(xrGetVulkanInstanceExtensionsKHR(m_xrInstance, m_xrSystemId, size, &size, data.data()));
    return split(data);
}

std::vector<std::string> VrVkExample::getXrVulkanDeviceExtensions()
{
    uint32_t size = 0;
    std::string data;
    XR_V(xrGetVulkanDeviceExtensionsKHR(m_xrInstance, m_xrSystemId, 0, &size, nullptr));
    data.resize(size);
    XR_V(xrGetVulkanDeviceExtensionsKHR(m_xrInstance, m_xrSystemId, size, &size, data.data()));
    return split(data);
}
#endif

#ifdef QHWI_BACKEND_D3D12
void VrD3d12Example::loadPFNXrFunctions()
{
    XR_V(xrGetInstanceProcAddr(m_xrInstance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetD3D12GraphicsRequirementsKHR));
}
#endif

void VrExample::createXrReferenceSpace()
{
    XrReferenceSpaceCreateInfo s{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    s.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    s.poseInReferenceSpace = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };
    XR_V(xrCreateReferenceSpace(m_xrSession, &s, &m_xrLocalSpace));
}

#ifdef QHWI_BACKEND_VULKAN
void VrVkExample::init(const Config& config)
{
    m_cfg = config;
    QHWI_LOGV("Create XR Instance");
    createXrInstance(GraphicsBackend::eVulkan);
    createXrDebugMessenger();
    getXrSystem();

    loadPFNXrFunctions();

    QHWI_LOGV("get XR Vulkan graphics requirements");
    XrGraphicsRequirementsVulkanKHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
    XR_V(xrGetVulkanGraphicsRequirementsKHR(m_xrInstance, m_xrSystemId, &graphicsRequirements));
    QHWI_LOGV("minAPIVersion: %llu", graphicsRequirements.minApiVersionSupported);

    QHWI_LOGV("get required XR Vulkan extensions");
    auto vkInstanceExtensions = getXrVulkanInstanceExtensions();
    QHWI_LOGV("required XR Vulkan instance extensions:");
    uint32_t j = 0;
    for (const auto& s : vkInstanceExtensions)
    {
        QHWI_LOGV("[%d]: %s", j++, s.c_str());
    }
    auto vkDeviceExtensions = getXrVulkanDeviceExtensions();
    QHWI_LOGV("required XR Vulkan device extensions:");
    j = 0;
    for (const auto& s : vkDeviceExtensions)
    {
        QHWI_LOGV("[%d]: %s", j++, s.c_str());
    }

    QHWI_LOGV("Create qhwi context and VkInstance");
    m_ctx = qhwi::createContext(qhwi::ContextDesc()
#    ifndef NDEBUG
                                    .setDebug(config.debug)
#    endif
                                    .setGraphicsBackend(qhwi::GraphicsBackend::eVulkan));

    qhwi::VkContext* vkCtx = static_cast<qhwi::VkContext*>(m_ctx.Get());
    auto vkInstance = vkCtx->instance();

    QHWI_LOGV("get XR Vulkan graphics device");
    ::VkPhysicalDevice xrVkPhysicalDevice{};
    XR_V(xrGetVulkanGraphicsDeviceKHR(m_xrInstance, m_xrSystemId, vkInstance, &xrVkPhysicalDevice));

    QHWI_LOGV("create VkDevice");
    m_device = m_ctx->createDevice();
    auto vkDevice = static_cast<qhwi::VkDevice*>(m_device.Get());
    // use vulkan coordinate system (origin top left)
    vkDevice->setFlipViewportY(false);

    QHWI_LOGV("create VkQueue");
    m_primaryCommandQueue = m_device->createCommandQueue();

    QHWI_LOGV("create XR Session");
    createXrSession();

    getXrViewConfigurationViews();

    QHWI_LOGV("create XR reference space");
    createXrReferenceSpace();

    QHWI_LOGV("create swapchain");
    createSwapchain();
    m_numCommandLists = NumSwapchainImages;
    QHWI_LOGV("create commandlists");
    createCommandLists();
    QHWI_LOGV("create frame fences");
    createFrameFences();
}
#endif

#ifdef QHWI_BACKEND_D3D12
void VrD3d12Example::init(const Config& config)
{
    m_cfg = config;
    QHWI_LOGV("Create XR Instance");
    createXrInstance(GraphicsBackend::eDirectX12);
    createXrDebugMessenger();
    getXrSystem();

    loadPFNXrFunctions();

    QHWI_LOGV("get XR D3D12 graphics requirements");
    XrGraphicsRequirementsD3D12KHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
    XR_V(xrGetD3D12GraphicsRequirementsKHR(m_xrInstance, m_xrSystemId, &graphicsRequirements));
    QHWI_LOGV("minFeatureLevel: %llu", graphicsRequirements.minFeatureLevel);

    // TODO: choose the D3d12 device that OpenXR wants

    QHWI_LOGV("Create qhwi context");
    m_ctx = qhwi::createContext(qhwi::ContextDesc()
#    ifndef NDEBUG
                                    .setDebug(config.debug)
#    endif
                                    .setGraphicsBackend(qhwi::GraphicsBackend::eDirectX12));

    QHWI_LOGV("create D3dDevice");
    m_device = m_ctx->createDevice(qhwi::DeviceDesc().SetEnableD3dEnhancedBarriers(false));

    QHWI_LOGV("create D3dQueue");
    m_primaryCommandQueue = m_device->createCommandQueue();

    QHWI_LOGV("create XR Session");
    createXrSession();

    getXrViewConfigurationViews();

    QHWI_LOGV("create XR reference space");
    createXrReferenceSpace();

    QHWI_LOGV("create swapchain");
    createSwapchain();
    m_numCommandLists = NumSwapchainImages;
    QHWI_LOGV("create commandlists");
    createCommandLists();
    QHWI_LOGV("create frame fences");
    createFrameFences();
}
#endif

void VrExample::createCommandLists()
{
    for (uint32_t j = 0; j < m_numCommandLists; j++)
    {
        char name[16];
        qre::PrintfSafe(name, "commandList[%d]", j);
        m_commandLists[j] = m_device->createCommandList(m_primaryCommandQueue.Get(), CommandListDesc().setName(name).setSupportsAutomaticBarriers(false));
    }
}

void VrExample::createFrameFences()
{
    for (uint32_t j = 0; j < m_numCommandLists; j++)
    {
        m_frameFences[j] = m_device->createFence(FenceState_Signaled);
    }
}

#ifdef QHWI_BACKEND_VULKAN
VrVkSwapchain::VrVkSwapchain(XrSession xrSession, VkDevice* device, const Description& desc)
    : VkSwapchain()
    , m_xrSession(xrSession)
{
    m_device = device;
    QHWI_ASSERT(m_device);
    QHWI_ASSERT(m_device->supportsMultiView());

    m_surfaceWidth = desc.surfaceWidth;
    m_surfaceHeight = desc.surfaceHeight;
    m_colorFormat = desc.colorFormat;
    m_depthStencilFormat = desc.depthStencilFormat;

    createXrSwapchain(desc.numImages, desc.vsync);

    createXrSwapchainImages(desc.numImages, desc.name, desc.automaticBarriers);

    createSemaphores();
}

VrVkSwapchain::~VrVkSwapchain()
{
    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        m_colorSwapchainTextures[j] = nullptr;
        m_depthStencilSwapchainTextures[j] = nullptr;
    }
    m_xrColorSwapchainImages.clear();
    m_xrDepthStencilSwapchainImages.clear();
    XR_V(xrDestroySwapchain(m_xrColorSwapchain));
    XR_V(xrDestroySwapchain(m_xrDepthStencilSwapchain));
}

void VrVkSwapchain::createXrSwapchainImages(uint32_t numImages, const char* namePrefix, bool autoBarrier)
{
    uint32_t numColorSwapchainImages = 0;
    XR_V(xrEnumerateSwapchainImages(m_xrColorSwapchain, 0, &numColorSwapchainImages, nullptr));
    QHWI_LOGV("num color swapchain images requested: %d allocated: %d", numImages, numColorSwapchainImages);
    if (numColorSwapchainImages > MaxSwapchainImages)
    {
        QHWI_FATAL("# swapchain images (%d) > max swapchain images (%d)", numColorSwapchainImages, MaxSwapchainImages);
    }
    m_xrColorSwapchainImages.resize(numColorSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
    XR_V(xrEnumerateSwapchainImages(m_xrColorSwapchain, numColorSwapchainImages, &numColorSwapchainImages, (XrSwapchainImageBaseHeader*)m_xrColorSwapchainImages.data()));
    m_numSwapchainImages = numColorSwapchainImages;

    uint32_t numDepthStencilSwapchainImages = 0;
    XR_V(xrEnumerateSwapchainImages(m_xrDepthStencilSwapchain, 0, &numDepthStencilSwapchainImages, nullptr));
    QHWI_LOGV("num depth swapchain images requested: %d allocated: %d", numImages, numDepthStencilSwapchainImages);
    if (numDepthStencilSwapchainImages > MaxSwapchainImages)
    {
        QHWI_FATAL("# swapchain images (%d) > max swapchain images (%d)", numDepthStencilSwapchainImages, MaxSwapchainImages);
    }
    m_xrDepthStencilSwapchainImages.resize(numDepthStencilSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
    XR_V(xrEnumerateSwapchainImages(m_xrDepthStencilSwapchain,
                                    numDepthStencilSwapchainImages,
                                    &numDepthStencilSwapchainImages,
                                    (XrSwapchainImageBaseHeader*)m_xrDepthStencilSwapchainImages.data()));
    QHWI_ASSERT(numDepthStencilSwapchainImages == numColorSwapchainImages);

    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        char name[MaxStringSize];
        qre::PrintfSafe(name, "%s_color_%d", (const char*)namePrefix, j);

        auto textureDesc = qhwi::Texture::Description()
                               .setFormat(qhwi::qhwi(m_colorFormat))
                               .setType(qhwi::TextureType::e2DArray)
                               .setWidth(m_surfaceWidth)
                               .setHeight(m_surfaceHeight)
                               .setMipLevels(1)
                               .setArrayLayers(2)
                               .setUsage(TextureUsage_ColorAttachment)
                               .setName(name)
                               .setAutomaticBarriers(autoBarrier);

        m_colorSwapchainTextures[j] = m_device->createTextureFromVkImage(textureDesc, m_xrColorSwapchainImages[j].image).CastTo<VkTexture>();

        qre::PrintfSafe(name, "%s_depth_stencil_%d", (const char*)namePrefix, j);

        textureDesc.setFormat(qhwi::qhwi(m_depthStencilFormat)).setUsage(TextureUsage_DepthStencilAttachment).setName(name);
        m_depthStencilSwapchainTextures[j] = m_device->createTextureFromVkImage(textureDesc, m_xrDepthStencilSwapchainImages[j].image).CastTo<VkTexture>();
    }
}

void VrVkSwapchain::createXrSwapchain(uint32_t numImages, bool vsync)
{
    // Color
    XrSwapchainCreateInfo s{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    s.createFlags = 0;
    s.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    s.format = (VkFormat)m_colorFormat;
    s.sampleCount = 1;
    s.width = m_surfaceWidth;
    s.height = m_surfaceHeight;
    s.faceCount = 1;
    s.arraySize = numImages;
    s.mipCount = 1;
    XR_V(xrCreateSwapchain(m_xrSession, &s, &m_xrColorSwapchain));

    // Depth/Stencil
    s.createFlags = 0;
    s.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    s.format = (VkFormat)m_depthStencilFormat;
    s.sampleCount = 1;
    s.width = m_surfaceWidth;
    s.height = m_surfaceHeight;
    s.faceCount = 1;
    s.arraySize = numImages;
    s.mipCount = 1;
    XR_V(xrCreateSwapchain(m_xrSession, &s, &m_xrDepthStencilSwapchain));
}

void VrVkSwapchain::acquireNextImage()
{
    uint32_t colorImageIndex = 0;
    uint32_t depthStencilImageIndex = 0;
    XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XR_V(xrAcquireSwapchainImage(m_xrColorSwapchain, &acquireInfo, &colorImageIndex));
    XR_V(xrAcquireSwapchainImage(m_xrDepthStencilSwapchain, &acquireInfo, &depthStencilImageIndex));
    QHWI_ASSERT(colorImageIndex == depthStencilImageIndex);
    m_currentImageIndex = colorImageIndex;

    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    XR_V(xrWaitSwapchainImage(m_xrColorSwapchain, &waitInfo));
    XR_V(xrWaitSwapchainImage(m_xrDepthStencilSwapchain, &waitInfo));
}

void VrVkSwapchain::releaseImage()
{
    XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XR_V(xrReleaseSwapchainImage(m_xrColorSwapchain, &releaseInfo));
    XR_V(xrReleaseSwapchainImage(m_xrDepthStencilSwapchain, &releaseInfo));
}

void VrVkExample::createSwapchain()
{
    QHWI_ASSERT(m_device->supportsMultiView());
    VrVkSwapchainDesc desc{};
    desc.setColorFormat(vk(ColorFormat))
        .setDepthStencilFormat(vk(DepthStencilFormat))
        .setSurfaceWidth((uint32_t)viewWidth())
        .setSurfaceHeight((uint32_t)viewHeight())
        .setNumImages(NumSwapchainImages)
        .setName("VrVkSwapchain")
        .setAutomaticBarriers(false);
    m_swapchain.reset(new VrVkSwapchain(m_xrSession, static_cast<qhwi::VkDevice*>(m_device.Get()), desc));
}

#endif

#ifdef QHWI_BACKEND_D3D12
VrD3d12Swapchain::VrD3d12Swapchain(XrSession xrSession, D3dDevice* device, const Description& desc)
    : DxgiSwapchain()
    , m_xrSession(xrSession)
{
    m_device = device;
    QHWI_ASSERT(m_device);
    QHWI_ASSERT(m_device->supportsMultiView());

    m_surfaceWidth = desc.surfaceWidth;
    m_surfaceHeight = desc.surfaceHeight;
    m_colorFormat = desc.colorFormat;
    m_depthStencilFormat = desc.depthStencilFormat;

    createXrSwapchain(desc.numImages, desc.vsync);

    createXrSwapchainImages(desc.numImages, desc.name, desc.automaticBarriers);
}

VrD3d12Swapchain::~VrD3d12Swapchain()
{
    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        m_colorSwapchainTextures[j] = nullptr;
        m_depthStencilSwapchainTextures[j] = nullptr;
    }
    m_xrColorSwapchainImages.clear();
    m_xrDepthStencilSwapchainImages.clear();
    XR_V(xrDestroySwapchain(m_xrColorSwapchain));
    XR_V(xrDestroySwapchain(m_xrDepthStencilSwapchain));
}

void VrD3d12Swapchain::createXrSwapchainImages(uint32_t numImages, const char* namePrefix, bool autoBarrier)
{
    uint32_t numColorSwapchainImages = 0;
    XR_V(xrEnumerateSwapchainImages(m_xrColorSwapchain, 0, &numColorSwapchainImages, nullptr));
    QHWI_LOGV("num color swapchain images requested: %d allocated: %d", numImages, numColorSwapchainImages);
    if (numColorSwapchainImages > MaxSwapchainImages)
    {
        QHWI_FATAL("# swapchain images (%d) > max swapchain images (%d)", numColorSwapchainImages, MaxSwapchainImages);
    }
    m_xrColorSwapchainImages.resize(numColorSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
    XR_V(xrEnumerateSwapchainImages(m_xrColorSwapchain, numColorSwapchainImages, &numColorSwapchainImages, (XrSwapchainImageBaseHeader*)m_xrColorSwapchainImages.data()));
    m_numSwapchainImages = numColorSwapchainImages;

    uint32_t numDepthStencilSwapchainImages = 0;
    XR_V(xrEnumerateSwapchainImages(m_xrDepthStencilSwapchain, 0, &numDepthStencilSwapchainImages, nullptr));
    QHWI_LOGV("num depth swapchain images requested: %d allocated: %d", numImages, numDepthStencilSwapchainImages);
    if (numDepthStencilSwapchainImages > MaxSwapchainImages)
    {
        QHWI_FATAL("# swapchain images (%d) > max swapchain images (%d)", numDepthStencilSwapchainImages, MaxSwapchainImages);
    }
    m_xrDepthStencilSwapchainImages.resize(numDepthStencilSwapchainImages, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
    XR_V(xrEnumerateSwapchainImages(m_xrDepthStencilSwapchain,
                                    numDepthStencilSwapchainImages,
                                    &numDepthStencilSwapchainImages,
                                    (XrSwapchainImageBaseHeader*)m_xrDepthStencilSwapchainImages.data()));
    QHWI_ASSERT(numDepthStencilSwapchainImages == numColorSwapchainImages);

    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        char name[MaxStringSize];
        qre::PrintfSafe(name, "%s_color_%d", (const char*)namePrefix, j);

        auto textureDesc = qhwi::Texture::Description()
                               .setFormat(qhwi::qhwi(m_colorFormat))
                               .setType(qhwi::TextureType::e2DArray)
                               .setWidth(m_surfaceWidth)
                               .setHeight(m_surfaceHeight)
                               .setMipLevels(1)
                               .setArrayLayers(2)
                               .setUsage(TextureUsage_ColorAttachment)
                               .setName(name)
                               .setAutomaticBarriers(autoBarrier);

        m_colorSwapchainTextures[j] = m_device->createTextureFromD3d12Resource(textureDesc, m_xrColorSwapchainImages[j].texture).CastTo<D3dTexture>();

        qre::PrintfSafe(name, "%s_depth_stencil_%d", (const char*)namePrefix, j);

        textureDesc.setFormat(qhwi::qhwi(m_depthStencilFormat)).setUsage(TextureUsage_DepthStencilAttachment).setName(name);
        m_depthStencilSwapchainTextures[j] = m_device->createTextureFromD3d12Resource(textureDesc, m_xrDepthStencilSwapchainImages[j].texture).CastTo<D3dTexture>();
    }
}

void VrD3d12Swapchain::createXrSwapchain(uint32_t numImages, bool vsync)
{
    // Color
    XrSwapchainCreateInfo s{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    s.createFlags = 0;
    s.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    s.format = (DXGI_FORMAT)m_colorFormat;
    s.sampleCount = 1;
    s.width = m_surfaceWidth;
    s.height = m_surfaceHeight;
    s.faceCount = 1;
    s.arraySize = numImages;
    s.mipCount = 1;
    XR_V(xrCreateSwapchain(m_xrSession, &s, &m_xrColorSwapchain));

    // Depth/Stencil
    s.createFlags = 0;
    s.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    s.format = (DXGI_FORMAT)m_depthStencilFormat;
    s.sampleCount = 1;
    s.width = m_surfaceWidth;
    s.height = m_surfaceHeight;
    s.faceCount = 1;
    s.arraySize = numImages;
    s.mipCount = 1;
    XR_V(xrCreateSwapchain(m_xrSession, &s, &m_xrDepthStencilSwapchain));
}

void VrD3d12Swapchain::acquireNextImage()
{
    uint32_t colorImageIndex = 0;
    uint32_t depthStencilImageIndex = 0;
    XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XR_V(xrAcquireSwapchainImage(m_xrColorSwapchain, &acquireInfo, &colorImageIndex));
    XR_V(xrAcquireSwapchainImage(m_xrDepthStencilSwapchain, &acquireInfo, &depthStencilImageIndex));
    QHWI_ASSERT(colorImageIndex == depthStencilImageIndex);
    m_currentImageIndex = colorImageIndex;

    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    XR_V(xrWaitSwapchainImage(m_xrColorSwapchain, &waitInfo));
    XR_V(xrWaitSwapchainImage(m_xrDepthStencilSwapchain, &waitInfo));
}

void VrD3d12Swapchain::releaseImage()
{
    XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XR_V(xrReleaseSwapchainImage(m_xrColorSwapchain, &releaseInfo));
    XR_V(xrReleaseSwapchainImage(m_xrDepthStencilSwapchain, &releaseInfo));
}

void VrD3d12Example::createSwapchain()
{
    QHWI_ASSERT(m_device->supportsMultiView());
    VrD3d12SwapchainDesc desc{};
    desc.setColorFormat(d3d(ColorFormat))
        .setDepthStencilFormat(d3d(DepthStencilFormat))
        .setSurfaceWidth((uint32_t)viewWidth())
        .setSurfaceHeight((uint32_t)viewHeight())
        .setNumImages(NumSwapchainImages)
        .setName("VrD3d12Swapchain")
        .setAutomaticBarriers(false);
    m_swapchain.reset(new VrD3d12Swapchain(m_xrSession, static_cast<qhwi::D3dDevice*>(m_device.Get()), desc));
}

#endif

#ifdef QHWI_PLATFORM_ANDROID
void VrExample::handleCommandCb(struct android_app* app, int32_t cmd)
{
    ((VrExample*)app->userData)->handleCommand(cmd);
}

void VrExample::handleCommand(int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_RESUME:
    {
        QHWI_LOGV("Resume");
        m_state.resumed = true;
        break;
    }
    case APP_CMD_PAUSE:
    {
        QHWI_LOGV("Pause");
        m_state.resumed = false;
        break;
    }
    case APP_CMD_DESTROY:
    {
        QHWI_LOGV("Destroy");
        m_nativeWindow = nullptr;
        break;
    }
    case APP_CMD_INIT_WINDOW:
    {
        QHWI_LOGV("Surface Created");
        m_nativeWindow = m_app->window;
        break;
    }
    case APP_CMD_TERM_WINDOW:
    {
        QHWI_LOGV("Surface Destroyed");
        m_nativeWindow = nullptr;
        break;
    }
    }
}

void VrExample::handleSystemEvents()
{
    if (m_app->destroyRequested)
    {
        m_state.appRunning = false;
        return;
    }
    while (true)
    {
        android_poll_source* source = nullptr;
        int events = 0;
        const int timeoutMilliseconds = (!m_state.resumed && !m_state.sessionRunning && !m_app->destroyRequested) ? -1 : 0;
        if (ALooper_pollOnce(timeoutMilliseconds, nullptr, &events, (void**)&source) < 0)
        {
            break;
        }
        if (source)
        {
            source->process(m_app, source);
        }
    }
}
#else
void VrExample::handleSystemEvents() {}
#endif

void VrExample::run()
{
    m_state.appRunning = true;
    QHWI_LOGV("run");
    while (m_state.appRunning)
    {
        handleSystemEvents();
        handleXrEvents();
        handleInput();
        if (!m_state.sceneCreated)
        {
            QHWI_LOGV("create scene");
            createScene();
        }
        if (m_state.sessionRunning)
        {
            renderFrame();
            m_frameIndex++;
        }
    }
}

void VrExample::beginTransfer()
{
    if (!m_copyCommandList || !m_copyCommandList->isReusable())
    {
        m_copyCommandList = m_device->createCommandList(m_primaryCommandQueue.Get(), { .name = "copyCommandList", .supportsAutomaticBarriers = false });
    }
    m_copyCommandList->begin();
    m_copyCommandList->beginTransferPass();
}

void VrExample::endTransfer()
{
    m_copyCommandList->endTransferPass();
    m_copyCommandList->end();
    qhwi::CommandList* cmdLists[] = { m_copyCommandList.Get() };
    m_primaryCommandQueue->executeCommandLists(1, cmdLists, nullptr);
    m_primaryCommandQueue->waitForIdle();
}

void VrExample::createScene()
{
    m_drawCubeTask.init({ .device = m_device.Get(),
                          .colorAttachmentFormat = ColorFormat,
                          .depthStencilFormat = DepthStencilFormat,
                          .rasterizerState = { .frontFaceCounterClockwise = true },
                          .multiViewState = { .numViews = 2 } });

    beginTransfer();
    m_drawCubeTask.uploadStagingBuffers(m_copyCommandList.Get());
    endTransfer();
    m_state.sceneCreated = true;
}

bool VrExample::renderLayer(RenderLayerInfo& renderLayerInfo)
{
    std::vector<XrView> xrViews(m_xrViewConfigurationViews.size(), { XR_TYPE_VIEW });

    XrViewState viewState{ XR_TYPE_VIEW_STATE };
    XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
    viewLocateInfo.space = m_xrLocalSpace;
    uint32_t numViews = 0;
    XR_V(xrLocateViews(m_xrSession, &viewLocateInfo, &viewState, static_cast<uint32_t>(xrViews.size()), &numViews, xrViews.data()));

    renderLayerInfo.xrLayerProjectionViews.resize(numViews, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });

    for (uint32_t j = 0; j < numViews; j++)
    {
        auto& layerProjView = renderLayerInfo.xrLayerProjectionViews[j];
        layerProjView = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
        layerProjView.pose = xrViews[j].pose;
        layerProjView.fov = xrViews[j].fov;
        layerProjView.subImage.swapchain = colorSwapchain();
        layerProjView.subImage.imageRect.offset.x = 0;
        layerProjView.subImage.imageRect.offset.y = 0;
        layerProjView.subImage.imageRect.extent.width = static_cast<int32_t>(viewWidth());
        layerProjView.subImage.imageRect.extent.height = static_cast<int32_t>(viewHeight());
        layerProjView.subImage.imageArrayIndex = j;
    }

    Fence* frameFence = m_frameFences[nextCommandListIndex()].Get();
    QHWI_ASSERT(frameFence);
    frameFence->wait();
    frameFence->reset();
    m_swapchain->acquireNextImage();
    m_currentCommandListIndex = (m_currentCommandListIndex + 1) % NumSwapchainImages;
    auto& commandList = m_commandLists[m_currentCommandListIndex];
    QHWI_ASSERT(commandList);
    if (!commandList || !commandList->isReusable())
    {
        commandList = m_device->createCommandList(m_primaryCommandQueue.Get(), CommandListDesc().setSupportsAutomaticBarriers(false));
    }
    m_currentCommandList = commandList.Get();
    // When application calls xrAcquireSwapchainImage, the XR runtime provides provides the color attachment
    // in state RENDER_TARGET and the depth stencil attachment in state DEPTH_WRITE

    using CBInput = test_util::DrawColor1Task::CBInput;
    CBInput cb;
    GraphicsBackend gfxBackend = m_device->getGraphicsBackend();

    for (uint32_t j = 0; j < numViews; j++)
    {
        const auto& xrView = xrViews[j];

        // Calculate projection matrix
        constexpr float nearZ = 0.05f;
        constexpr float farZ = 100.0f;
        float left = tanf(xrView.fov.angleLeft) * nearZ, right = tanf(xrView.fov.angleRight) * nearZ, bottom = tanf(xrView.fov.angleDown) * nearZ,
              top = tanf(xrView.fov.angleUp) * nearZ;
        Mat4f proj = (gfxBackend == GraphicsBackend::eDirectX12) ? Mat4f::Perspective(left, right, top, bottom, nearZ, farZ).first :
                                                                   Mat4f::Perspective(left, right, bottom, top, nearZ, farZ).first;
        // convert to XR coordinate system (right hand rule, normal depth)
        toXrProj(&proj.m[0][0], nearZ, farZ);

        // Calculate view matrix
        Quatf orientation = toQre(xrView.pose.orientation);
        Vec3f position = toQre(xrView.pose.position);
        Mat4f view(Affine3f::ScaleRotationTranslation({ 1.0f, 1.0f, 1.0f }, orientation, position));
        view = view.Inverse();
        Mat4f viewProj = proj * view;

        // Calculate model matrix
        Quatf qRot = Quatf::FromEuler(m_rot[0], m_rot[1], m_rot[2]);
        Mat4f model(Affine3f::Translation(0.0f, -1.0f, -2.0f) * Affine3f::Rotation(qRot));

        Mat4f modelViewProj(viewProj * model);
        memcpy(cb.modelViewProj[j].v, modelViewProj.m, sizeof(modelViewProj.m));
    }
    m_rot[0] = fmodf(m_rot[0] + 1.0f * ToRad, 2.0f * Pi);
    m_rot[1] = fmodf(m_rot[0] + 2.0f * ToRad, 2.0f * Pi);
    m_rot[2] = fmodf(m_rot[0] + 3.0f * ToRad, 2.0f * Pi);
    m_drawCubeTask.updateConstants(cb);
    recordCommandList(m_currentCommandList);

    CommandList* commandLists[] = { m_currentCommandList };
    m_primaryCommandQueue->executeCommandLists(1, commandLists, frameFence);

    releaseSwapchainImage();

    renderLayerInfo.xrLayerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    renderLayerInfo.xrLayerProjection.space = m_xrLocalSpace;
    renderLayerInfo.xrLayerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.xrLayerProjectionViews.size());
    renderLayerInfo.xrLayerProjection.views = renderLayerInfo.xrLayerProjectionViews.data();

    return true;
}

void VrExample::renderFrame()
{
    XrFrameState frameState{ XR_TYPE_FRAME_STATE };
    XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
    XR_V(xrWaitFrame(m_xrSession, &frameWaitInfo, &frameState));

    XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
    XR_V(xrBeginFrame(m_xrSession, &frameBeginInfo));

    RenderLayerInfo renderLayerInfo;
    renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

    auto sessionState = m_state.xrSessionState;
    bool sessionActive = (sessionState == XR_SESSION_STATE_SYNCHRONIZED || sessionState == XR_SESSION_STATE_VISIBLE || sessionState == XR_SESSION_STATE_FOCUSED);

    if (sessionActive && frameState.shouldRender)
    {
        if (renderLayer(renderLayerInfo))
        {
            renderLayerInfo.xrLayers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&renderLayerInfo.xrLayerProjection));
        }
    }
    XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.xrLayers.size());
    frameEndInfo.layers = renderLayerInfo.xrLayers.data();
    XR_V(xrEndFrame(m_xrSession, &frameEndInfo));
}

void VrExample::beginRenderPass(qhwi::CommandList* cmdList)
{
    uint32_t w = (uint32_t)viewWidth(), h = (uint32_t)viewHeight();
    ColorAttachmentDescription colorAttachmentDesc = {
        .loadOp = AttachmentLoadOp::eClear,
        .storeOp = AttachmentStoreOp::eStore,
    };
    colorAttachmentDesc.setTexture(getCurrentSwapchainColorImage(), EntireResource);
    DepthStencilAttachmentDescription depthStencilAttachmentDesc = {
        .depthLoadOp = AttachmentLoadOp::eClear,
        .depthStoreOp = AttachmentStoreOp::eDontCare,
        .stencilLoadOp = AttachmentLoadOp::eDontCare,
        .stencilStoreOp = AttachmentStoreOp::eDontCare,
    };

    depthStencilAttachmentDesc.setTexture(getCurrentSwapchainDepthStencilImage(), EntireResource);

    MultiViewState multiViewState = { .numViews = 2 };
    cmdList->beginRenderPass(1, &colorAttachmentDesc, &depthStencilAttachmentDesc, &multiViewState);
    cmdList->setViewport({ .w = float(w), .h = float(h) });
    cmdList->setScissor({ .w = w, .h = h });
}

void VrExample::endRenderPass(qhwi::CommandList* cmdList)
{
    cmdList->endRenderPass();
    cmdList->addTextureBarrier(TextureBarrier().setSrc(TextureState_DepthStencilAttachment).setDst(TextureState_General).setTexture(getCurrentSwapchainDepthStencilImage()));
}

void VrExample::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    beginRenderPass(commandList);
    m_drawCubeTask.draw(commandList);
    endRenderPass(commandList);

    commandList->end();
}

void VrExample::handleInput() {}

void VrExample::handleXrEvents()
{
    while (true)
    {
        XrEventDataBuffer eventData = { XR_TYPE_EVENT_DATA_BUFFER };
        if (xrPollEvent(m_xrInstance, &eventData) != XR_SUCCESS)
        {
            break;
        }
        switch (eventData.type)
        {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            XrEventDataSessionStateChanged* sessionStateChanged = (XrEventDataSessionStateChanged*)(&eventData);
            auto newState = sessionStateChanged->state;
            if (sessionStateChanged->session != m_xrSession)
            {
                break;
            }
            if (newState == XR_SESSION_STATE_READY)
            {
                XrSessionBeginInfo s{ XR_TYPE_SESSION_BEGIN_INFO };
                s.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                QHWI_LOGV("begin xr session");
                XR_V(xrBeginSession(m_xrSession, &s));
                m_state.sessionRunning = true;
            }
            else if (newState == XR_SESSION_STATE_STOPPING)
            {
                QHWI_LOGV("end xr session");
                XR_V(xrEndSession(m_xrSession));
                m_state.sessionRunning = false;
            }
            else if (newState == XR_SESSION_STATE_EXITING || newState == XR_SESSION_STATE_LOSS_PENDING)
            {
                m_state.sessionRunning = false;
                m_state.appRunning = false;
            }
            m_state.xrSessionState = newState;
            break;
        }
        default:
            break;
        }
    }
}

void VrExample::destroy()
{
    m_swapchain = nullptr;
    XR_V(xrDestroySpace(m_xrLocalSpace));
    XR_V(xrDestroySession(m_xrSession));
    XR_V(xrDestroyInstance(m_xrInstance));
}

static void initCmdLineOpts(cxxopts::Options& opts)
{
#ifdef QHWI_BACKEND_D3D12
    std::string defaultGraphicsBackend = "d3d12";
#elif QHWI_BACKEND_VULKAN
    std::string defaultGraphicsBackend = "vk";
#endif
    opts.add_options()
#ifndef NDEBUG
        ("debug", "Enable debug layer", cxxopts::value<bool>()->default_value("true"))
#else
        ("debug", "Enable debug layer", cxxopts::value<bool>()->default_value("false"))
#endif
            ("backend", "Graphics backend", cxxopts::value<std::string>()->default_value(defaultGraphicsBackend));
}

static void parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, VrExample::Config& c)
{
    p = opts.parse(argc, argv);
    c.debug = p["debug"].as<bool>();
    fromString(p["backend"].as<std::string>().c_str(), c.backend);
}

#ifdef QHWI_PLATFORM_ANDROID
void VrExample::init(android_app* app)
{
    m_app = app;

    QHWI_LOGV("get intent args");
    int argc;
    char** argv;
    GetCommandLineArgsOp op(app, "vr", argc, argv);
    op.run();

    qre::OsSetLogConsole(true);
    cxxopts::Options opts("VrExample", "Run VR example");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    VrExample::Config config;
    parse(argc, argv, opts, parseResult, config);

    QHWI_LOGV("Initialize OpenXR loader");
    initXrLoader();

    // set app command callback
    m_app->userData = this;
    m_app->onAppCmd = handleCommandCb;

    init(config);
}

void android_main(android_app* app)
{
    FileUtil::setAndroidAssetManager(app->activity->assetManager);

    std::unique_ptr<VrExample> vrExample;
    vrExample.reset(new VrVkExample);
    vrExample->init(app);
    vrExample->run();
    vrExample->destroy();
}
#else
int main(int argc, char** argv)
{
    qre::OsSetLogConsole(true);
    cxxopts::Options opts("VrExample", "Run VR example");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    VrExample::Config config;
    parse(argc, argv, opts, parseResult, config);
    std::unique_ptr<VrExample> vrExample;
#    ifdef QHWI_BACKEND_VULKAN
    if (config.backend == GraphicsBackend::eVulkan)
    {
        vrExample.reset(new VrVkExample);
    }
#    endif
#    ifdef QHWI_BACKEND_D3D12
    if (config.backend == GraphicsBackend::eDirectX12)
    {
        vrExample.reset(new VrD3d12Example);
    }
#    endif
    vrExample->init(config);
    vrExample->run();
    vrExample->destroy();
    return 0;
}
#endif
