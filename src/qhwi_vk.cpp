//
//
//  qhwi_vk.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

//uncomment to enable debug break
//#define ENABLE_DEBUG_BREAK

#define VMA_IMPLEMENTATION
#include <qhwi/qhwi_vk.h>

#include "qhwi/qhwi_profiling.h"
#include <qhwi/qhwi_log_util.h>
#include <qhwi/qhwi_util.h>
#include <qre/math/hash.h>
#include <qre/text/textutil.h>
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
#    include <TileStream.h>
#endif
#include <array>
#include <sstream>
#include <cstdio>
#include <fstream>
#include <locale>

#if defined(QHWI_PLATFORM_WINDOWS)
#    include <windows.h>
#elif defined(QHWI_PLATFORM_LINUX) || defined(QHWI_PLATFORM_ANDROID)
#    include <signal.h>
#    include <unistd.h>
#endif

#if defined(QHWI_PLATFORM_ANDROID)
#    include <android/native_window.h>
#endif

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace qhwi
{

static bool supportsDynamicRenderingLocalRead(const qhwi::Device* device)
{
    QHWI_ASSERT(device);
    return static_cast<const qhwi::VkDevice*>(device)->supportsDynamicRenderingLocalRead();
}

static bool contains(const std::vector<vk::OpticalFlowImageFormatPropertiesNV>& v, vk::Format s)
{
    for (auto& v0 : v)
    {
        if (v0.format == s)
        {
            return true;
        }
    }
    return false;
}

static bool contains(std::span<const char* const> v, const char* s)
{
    for (auto& v0 : v)
    {
        if (0 == strcmp(v0, s))
        {
            return true;
        }
    }
    return false;
}

static const char* str(const char* v)
{
    return v ? v : "";
}

#define V(func)                                                                                                                                                                    \
    {                                                                                                                                                                              \
        vk::Result result = vk::Result(func);                                                                                                                                      \
        if (result != vk::Result::eSuccess)                                                                                                                                        \
        {                                                                                                                                                                          \
            QHWI_ERR("function: %s, result: %s", #func, vk::to_string(result).c_str());                                                                                            \
        }                                                                                                                                                                          \
    }

static VkBool32 debugUtilsMessengerCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                            vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
                                            const vk::DebugUtilsMessengerCallbackDataEXT* data,
                                            void* pUserData)
{
    std::stringstream debugMessage;
    const char* severityStr = "INFO";

    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
    {
        severityStr = "VERBOSE";
    }
    else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
    {
        severityStr = "INFO";
    }
    else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        severityStr = "WARNING";
    }
    else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
    {
        severityStr = "ERROR";
    }

    // Create a locale-independent stream to avoid comma separators in numbers
    debugMessage.imbue(std::locale::classic());
    debugMessage << "[" << severityStr << "][" << data->messageIdNumber << "][" << (data->pMessageIdName ? data->pMessageIdName : "")
                 << "] : " << (data->pMessage ? data->pMessage : "");

    QHWI_LOGI("%s", debugMessage.str().c_str());

#if defined(ENABLE_DEBUG_BREAK)
    if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
    {
#    if defined(QHWI_PLATFORM_WINDOWS)
        if (IsDebuggerPresent())
        {
            __debugbreak();
        }
#    elif defined(QHWI_PLATFORM_DARWIN) || defined(QHWI_PLATFORM_IOS)
        __builtin_trap();
#    elif defined(QHWI_PLATFORM_LINUX) || defined(QHWI_PLATFORM_ANDROID)
        raise(SIGTRAP);
#    endif
    }
#endif

    return VK_FALSE;
}

static void setName(vk::Device device, const vk::DebugUtilsObjectNameInfoEXT& nameInfo)
{
    if (VULKAN_HPP_DEFAULT_DISPATCHER.vkSetDebugUtilsObjectNameEXT)
    {
        V(device.setDebugUtilsObjectNameEXT(&nameInfo));
    }
}

static bool hasLocalReadBarrier(uint32_t numTextureBarriers, const TextureBarrier* textureBarriers)
{
    for (uint32_t j = 0; j < numTextureBarriers; j++)
    {
        if (textureBarriers[j].dst.layout == ImageLayout::eLocalRead)
        {
            return true;
        }
    }
    return false;
}

class VkStructChain
{
public:
    template <typename T>
    VkStructChain(T& v, const qhwi::VkPhysicalDevice* physicalDevice)
    {
        m_pNext = &v.pNext;
        m_physicalDevice = physicalDevice;
    }

    template <typename T>
    VkStructChain& link(T& v)
    {
        *m_pNext = &v;
        m_pNext = &v.pNext;
        return *this;
    }

    template <typename T>
    VkStructChain& link(const char* ext, T& v)
    {
        if (m_physicalDevice->supportsExtension(ext))
        {
            link(v);
        }
        return *this;
    }

    template <typename T>
    VkStructChain& linkAliased(const char* ext0, const char* ext1, T& v)
    {
        if (m_physicalDevice->supportsExtension(ext0) || m_physicalDevice->supportsExtension(ext1))
        {
            link(v);
        }
        return *this;
    }

private:
    void** m_pNext = nullptr;
    const qhwi::VkPhysicalDevice* m_physicalDevice = nullptr;
};

} // namespace qhwi

static qhwi::VkSharedHandleType getSharedHandle(vk::Device device, vk::DeviceMemory deviceMemory)
{
#if defined(QHWI_PLATFORM_WINDOWS)
    vk::MemoryGetWin32HandleInfoKHR getInfo(deviceMemory, vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32);
    return device.getMemoryWin32HandleKHR(getInfo);
#else
    vk::MemoryGetFdInfoKHR getInfo(deviceMemory, vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd);
    return device.getMemoryFdKHR(getInfo);
#endif
}

static qhwi::VkSharedHandleType getSharedHandle(vk::Device device, vk::Fence fence)
{
#if defined(QHWI_PLATFORM_WINDOWS)
    vk::FenceGetWin32HandleInfoKHR getInfo(fence, vk::ExternalFenceHandleTypeFlagBits::eOpaqueWin32);
    return device.getFenceWin32HandleKHR(getInfo);
#else
    vk::FenceGetFdInfoKHR getInfo(fence, vk::ExternalFenceHandleTypeFlagBits::eOpaqueFd);
    return device.getFenceFdKHR(getInfo);
#endif
}

#if defined(QHWI_PLATFORM_WINDOWS)
static constexpr vk::ExternalMemoryHandleTypeFlags s_ExternalMemoryHandleTypeFlags = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
static constexpr vk::ExternalFenceHandleTypeFlags s_ExternalFenceHandleTypeFlags = vk::ExternalFenceHandleTypeFlagBits::eOpaqueWin32;
#else
static constexpr vk::ExternalMemoryHandleTypeFlags s_ExternalMemoryHandleTypeFlags = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
static constexpr vk::ExternalFenceHandleTypeFlags s_ExternalFenceHandleTypeFlags = vk::ExternalFenceHandleTypeFlagBits::eOpaqueFd;
#endif

void qhwi::ext::VkSurface::init(vk::Instance instance, const WindowSurface& windowSurface)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    bool windowSurfaceChanged = (windowSurface.hwnd != m_windowSurface.hwnd);
    m_windowSurface = windowSurface;
    vk::Win32SurfaceCreateInfoKHR scInfo;
    scInfo.hwnd = windowSurface.hwnd;
    scInfo.hinstance = (HINSTANCE)GetWindowLongPtr(scInfo.hwnd, GWLP_HINSTANCE);

    if (windowSurface.width == 0 || windowSurface.height == 0)
    {
        RECT rect;
        GetClientRect(scInfo.hwnd, &rect);
        m_windowSurface.width = rect.right - rect.left;
        m_windowSurface.height = rect.bottom - rect.top;
    }
    if (windowSurfaceChanged)
    {
        m_surface = instance.createWin32SurfaceKHRUnique(scInfo);
    }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    bool windowSurfaceChanged = (windowSurface.aNativeWindow != m_windowSurface.aNativeWindow);
    m_windowSurface = windowSurface;
    vk::AndroidSurfaceCreateInfoKHR scInfo;
    scInfo.window = windowSurface.aNativeWindow;

    if (windowSurface.width == 0 || windowSurface.height == 0)
    {
        m_windowSurface.width = static_cast<unsigned int>(ANativeWindow_getWidth(scInfo.window));
        m_windowSurface.height = static_cast<unsigned int>(ANativeWindow_getHeight(scInfo.window));
    }
    if (windowSurfaceChanged)
    {
        m_surface = instance.createAndroidSurfaceKHRUnique(scInfo);
    }
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    bool windowSurfaceChanged = (windowSurface.wlSurface != m_windowSurface.wlSurface);
    m_windowSurface = windowSurface;
    vk::WaylandSurfaceCreateInfoKHR scInfo;

    scInfo.display = windowSurface.wlDisplay;
    scInfo.surface = windowSurface.wlSurface;

    QHWI_ASSERT_MSG(windowSurface.width != 0 && windowSurface.height != 0, "Window size must be provided for Wayland WSI");

    m_windowSurface.width = windowSurface.width;
    m_windowSurface.height = windowSurface.height;
    if (windowSurfaceChanged)
    {
        m_surface = instance.createWaylandSurfaceKHRUnique(scInfo);
    }
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    bool windowSurfaceChanged = (windowSurface.window != m_windowSurface.window);
    m_windowSurface = windowSurface;
    vk::XlibSurfaceCreateInfoKHR scInfo;
    XDisplay* dpy = windowSurface.display;
    XWindow window = windowSurface.window;

    scInfo.dpy = dpy;
    scInfo.window = window;

    if (windowSurface.width == 0 || windowSurface.height == 0)
    {
        XWindowAttributes attrib;
        Status ret = XGetWindowAttributes(dpy, window, &attrib);
        QHWI_ASSERT(ret != 0);

        m_windowSurface.width = attrib.width;
        m_windowSurface.height = attrib.height;
    }
    if (windowSurfaceChanged)
    {
        m_surface = instance.createXlibSurfaceKHRUnique(scInfo);
    }
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    bool windowSurfaceChanged = (windowSurface.drawable != m_windowSurface.drawable);
    m_windowSurface = windowSurface;
    vk::XcbSurfaceCreateInfoKHR scInfo;

    scInfo.connection = windowSurface.connection;
    scInfo.window = windowSurface.drawable;

    if (windowSurface.width == 0 || windowSurface.height == 0)
    {
        xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(windowSurface.connection, xcb_get_geometry(windowSurface.connection, windowSurface.drawable), nullptr);
        QHWI_ASSERT(geom != nullptr);

        m_windowSurface.width = geom->width;
        m_windowSurface.height = geom->height;
        free(geom);
    }
    if (windowSurfaceChanged)
    {
        m_surface = instance.createXcbSurfaceKHRUnique(scInfo);
    }
#endif
}

void qhwi::ext::VkSurface::init(VkDevice* device, const WindowSurface& surface)
{
    QHWI_ASSERT(device);
    init(device->getInstance(), surface);
}

qhwi::ext::VkSurface::~VkSurface() {}

void qhwi::ext::VkSwapchain::init(VkDevice* device, const Description& desc)
{
    vk::SwapchainCreateInfoKHR createInfo{};
    VkSwapchainUtil::initSwapchainCreateInfo(device, desc, createInfo);
    init(device, desc, createInfo);
}

void qhwi::ext::VkSwapchain::init(VkDevice* device, const Description& desc, const vk::SwapchainCreateInfoKHR& createInfo)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchain::init", QHWI_COLOR_RESOURCE_CREATE);
    m_device = device;
    QHWI_ASSERT(m_device);
    m_surface = m_device->getVkWindowSurface();
    m_vsync = desc.vsync;
    m_autoBarrier = desc.automaticBarriers;
    auto vkDevice = m_device->get();

    m_colorFormat = createInfo.imageFormat;
    m_colorSpace = createInfo.imageColorSpace;
    m_swapchain = vkDevice.createSwapchainKHRUnique(createInfo);

    uint32_t numSwapchainImages = 0;
    V(vkDevice.getSwapchainImagesKHR(m_swapchain.get(), &numSwapchainImages, nullptr));
    QHWI_LOGI("num swapchain images requested: %d allocated: %d", desc.numImages, numSwapchainImages);
    if (numSwapchainImages > MaxSwapchainImages)
    {
        QHWI_FATAL("# swapchain images (%d) > max swapchain images (%d)", numSwapchainImages, MaxSwapchainImages);
    }
    m_numSwapchainImages = numSwapchainImages;

    createSwapchainImages(desc.name, desc.automaticBarriers);

    createSemaphores(desc.name);

    m_recycledImageAcquiredSemaphores.clear();

    for (uint32_t i = 0; i < numSwapchainImages; ++i)
    {
        vk::UniqueSemaphore semaphore = vkDevice.createSemaphoreUnique(vk::SemaphoreCreateInfo());

        m_recycledImageAcquiredSemaphores.push(std::move(semaphore));
    }

    m_currentImageIndex = uint32_t(-1);
    m_frameRendered = false;
}

void qhwi::ext::VkSwapchain::createSwapchainImages(const char* namePrefix, bool autoBarrier)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchain::createSwapchainImages", QHWI_COLOR_RESOURCE_CREATE);

    auto vkDevice = m_device->get();
    vk::Image swapchainImages[MaxSwapchainImages];
    V(vkDevice.getSwapchainImagesKHR(m_swapchain.get(), &m_numSwapchainImages, swapchainImages));
    const auto windowSurface = m_device->getVkWindowSurface();

    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        char name[MaxStringSize];
        qre::PrintfSafe(name, "%s_color_%d", namePrefix ? (const char*)namePrefix : "swapchain", j);

        auto textureDesc = qhwi::Texture::Description()
                               .setFormat(qhwi::qhwi(m_colorFormat))
                               .setWidth(windowSurface->width())
                               .setHeight(windowSurface->height())
                               .setMipLevels(1)
                               .setUsage(TextureUsage_ColorAttachment)
                               .setName(name)
                               .setAutomaticBarriers(autoBarrier);

        m_colorSwapchainTextures[j] = m_device->createTextureFromVkHandle(textureDesc, swapchainImages[j]).CastTo<VkTexture>();
    }
}

void qhwi::ext::VkSwapchain::destroy()
{
    // Explicitly release all semaphores while the device and dispatch loader
    // are still valid. Without this, the UniqueSemaphore destructors fire after
    // the device teardown, triggering the "m_owner && m_dispatch" assert added
    // in vulkan.hpp 1.4.354.
    if (!m_device)
    {
        return;
    }
    m_recycledImageAcquiredSemaphores.clear();
    m_imageAcquiredSemaphore.reset();
    for (auto& s : m_renderCompleteSemaphore)
    {
        s.reset();
    }
    m_swapchain.reset();
    m_device = nullptr;
}

void qhwi::ext::VkSwapchain::createSemaphores(const char* namePrefix)
{
    for (uint32_t j = 0; j < m_numSwapchainImages; j++)
    {
        char name[MaxStringSize];
        m_renderCompleteSemaphore[j] = m_device->get().createSemaphoreUnique(vk::SemaphoreCreateInfo());

        qre::PrintfSafe(name, "%s_renderCompleteSemaphore_%d", namePrefix ? (const char*)namePrefix : "swapchain", j);
        setName(m_device->get(), vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eSemaphore, (uint64_t)(VkSemaphore)m_renderCompleteSemaphore[j].get(), name));
    }
}

void qhwi::VkPhysicalDevice::getVkSurfaceFormats(vk::SurfaceKHR vkSurface, uint32_t* numSurfaceFormatsPtr, vk::SurfaceFormatKHR* surfaceFormats) const
{
    uint32_t& numSurfaceFormats = *numSurfaceFormatsPtr;
    V(m_physicalDevice.getSurfaceFormatsKHR(vkSurface, &numSurfaceFormats, nullptr));
    if (!surfaceFormats)
    {
        return;
    }
    V(m_physicalDevice.getSurfaceFormatsKHR(vkSurface, &numSurfaceFormats, surfaceFormats));
}

void qhwi::ext::VkSwapchain::resize(int width, int height)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchain::resize", QHWI_COLOR_GRAPHICS);

    auto vkSurface = m_device->getVkWindowSurface();
    QHWI_ASSERT(vkSurface);
    qhwi::ext::WindowSurface windowSurface = vkSurface->windowSurface();
    windowSurface.width = width;
    windowSurface.height = height;
    resize(windowSurface);
}

void qhwi::ext::VkSwapchain::resize(const WindowSurface& surface)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchain::resize", QHWI_COLOR_GRAPHICS);

    m_device->get().destroySwapchainKHR(m_swapchain.release());
    m_device->setWindowSurface(surface);
    auto swapChainDesc = Swapchain::Description().setNumImages(m_numSwapchainImages).setAutomaticBarriers(m_autoBarrier).setVsync(m_vsync);
    vk::SwapchainCreateInfoKHR createInfo{};
    qhwi::ext::VkSwapchainUtil::initSwapchainCreateInfo(m_device, swapChainDesc, createInfo);
    createInfo.setImageFormat(m_colorFormat);
    createInfo.setImageColorSpace(m_colorSpace);
    init(m_device, swapChainDesc, createInfo);
}

void qhwi::ext::VkSwapchainUtil::initSwapchainCreateInfo(VkDevice* device, const Swapchain::Description& desc, vk::SwapchainCreateInfoKHR& swCreateInfo)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchainUtil::initSwapchainCreateInfo", QHWI_COLOR_GRAPHICS);

    qhwi::ext::VkSurface* surface = device->getVkWindowSurface();
    QHWI_ASSERT(surface);
    vk::SurfaceKHR vkSurface = surface->get();
    auto physicalDevice = device->getPhysicalDevice1().get();
    vk::SurfaceCapabilitiesKHR surfaceCaps;
    surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(vkSurface);
    uint32_t imageCount = std::clamp(uint32_t(desc.numImages), surfaceCaps.minImageCount, surfaceCaps.maxImageCount);
    vk::ImageUsageFlags imageUsage = qhwi::vkImageUsageFlags(desc.imageUsage);
    imageUsage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
    swCreateInfo = vk::SwapchainCreateInfoKHR().setMinImageCount(imageCount).setImageArrayLayers(1).setImageUsage(imageUsage).setClipped(true);
    swCreateInfo.surface = vkSurface;

    // Candidate formats
    uint32_t numSurfaceFormats = 0;
    vk::SurfaceFormatKHR surfaceFormats[MaxSurfaceFormats];
    V(physicalDevice.getSurfaceFormatsKHR(vkSurface, &numSurfaceFormats, nullptr));
    if (numSurfaceFormats > MaxSurfaceFormats)
    {
        QHWI_FATAL("# surface formats (%d) > max surface formats (%d)", numSurfaceFormats, MaxSurfaceFormats);
    }
    V(physicalDevice.getSurfaceFormatsKHR(vkSurface, &numSurfaceFormats, surfaceFormats));

    uint32_t numPresentModes = 0;
    vk::PresentModeKHR presentModes[MaxPresentModes];
    V(physicalDevice.getSurfacePresentModesKHR(vkSurface, &numPresentModes, nullptr));
    if (numPresentModes > MaxPresentModes)
    {
        QHWI_FATAL("# present modes (%d) > max present modes (%d)", numPresentModes, MaxPresentModes);
    }
    V(physicalDevice.getSurfacePresentModesKHR(vkSurface, &numPresentModes, presentModes));

    // Check if currentExtent is defined (not UINT32_MAX)
    // When currentExtent is UINT32_MAX, it means the surface size is determined by the swapchain extent
    if ((surfaceCaps.currentExtent.width == UINT32_MAX && surfaceCaps.currentExtent.height == UINT32_MAX) && surface)
    {
        // Use the window surface dimensions
        swCreateInfo.imageExtent.width = surface->width();
        swCreateInfo.imageExtent.height = surface->height();

        // Clamp to the surface capabilities
        swCreateInfo.imageExtent.width = std::clamp(swCreateInfo.imageExtent.width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width);
        swCreateInfo.imageExtent.height = std::clamp(swCreateInfo.imageExtent.height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height);
    }
    else
    {
        swCreateInfo.imageExtent = surfaceCaps.currentExtent;
    }

    if (desc.preRotation && (surfaceCaps.currentTransform & (vk::SurfaceTransformFlagBitsKHR::eRotate90 | vk::SurfaceTransformFlagBitsKHR::eRotate270)))
    {
        std::swap(swCreateInfo.imageExtent.width, swCreateInfo.imageExtent.height);
    }
    qre::StaticVector<vk::Format, qhwi::MaxSurfaceFormats> vkFormatCandidates(desc.formatCandidates.size());
    for (uint32_t j = 0; j < desc.formatCandidates.size(); j++)
    {
        vkFormatCandidates[j] = qhwi::vk(desc.formatCandidates[j]);
    }
    swCreateInfo.imageFormat = VkSwapchainUtil::chooseSurfaceFormat(surfaceFormats, vkFormatCandidates);
    swCreateInfo.presentMode = choosePresentMode(std::span(presentModes, numPresentModes), desc.vsync);
    swCreateInfo.preTransform = desc.preRotation ? surfaceCaps.currentTransform : vk::SurfaceTransformFlagBitsKHR::eIdentity;
    swCreateInfo.compositeAlpha = chooseCompositeAlpha(surfaceCaps);
}

vk::PresentModeKHR qhwi::ext::VkSwapchainUtil::choosePresentMode(std::span<const vk::PresentModeKHR> supportedPresentModes,
                                                                 std::span<const vk::PresentModeKHR> candidatePresentModes)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchainUtil::choosePresentMode", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(!supportedPresentModes.empty());
    for (const vk::PresentModeKHR& presentMode : candidatePresentModes)
    {
        if (std::find(supportedPresentModes.begin(), supportedPresentModes.end(), presentMode) != supportedPresentModes.end())
        {
            return presentMode;
        }
    }
    return supportedPresentModes[0];
}

vk::HdrMetadataEXT qhwi::ext::VkSwapchainUtil::hdrMetadata(qhwi::ext::DisplayGamut displayGamut)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchainUtil::hdrMetadata", QHWI_COLOR_GRAPHICS);
    vk::HdrMetadataEXT s{};
    DisplayChromaticityValues v = DisplayChromaticities[displayGamut];
    s.displayPrimaryRed.x = v.RedPrimaryX;
    s.displayPrimaryRed.y = v.RedPrimaryY;
    s.displayPrimaryGreen.x = v.GreenPrimaryX;
    s.displayPrimaryGreen.y = v.GreenPrimaryY;
    s.displayPrimaryBlue.x = v.BluePrimaryX;
    s.displayPrimaryBlue.y = v.BluePrimaryY;
    s.maxLuminance = DefaultHdrMetadata::MaxLuminance;
    s.minLuminance = DefaultHdrMetadata::MinLuminance;
    s.maxContentLightLevel = DefaultHdrMetadata::MaxContentLightLevel;
    s.maxFrameAverageLightLevel = DefaultHdrMetadata::MaxFrameAverageLightLevel;
    return s;
}

vk::CompositeAlphaFlagBitsKHR qhwi::ext::VkSwapchainUtil::chooseCompositeAlpha(const vk::SurfaceCapabilitiesKHR& surfaceCaps)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchainUtil::chooseCompositeAlpha", QHWI_COLOR_GRAPHICS);
    const auto& supportedCompositeAlpha = surfaceCaps.supportedCompositeAlpha;
    if (supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)
    {
        return vk::CompositeAlphaFlagBitsKHR::eOpaque;
    }
    else if (supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)
    {
        return vk::CompositeAlphaFlagBitsKHR::eInherit;
    }
    else if (supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
    {
        return vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
    }
    else if (supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
    {
        return vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
    }
    return vk::CompositeAlphaFlagBitsKHR::eOpaque;
}

qhwi::Format qhwi::ext::VkSwapchain::getFormat() const
{
    return qhwi::qhwi(m_colorFormat);
}

vk::Format qhwi::ext::VkSwapchainUtil::chooseSurfaceFormat(const std::span<const vk::SurfaceFormatKHR>& supportedSurfaceFormats,
                                                           const std::span<const vk::Format>& candidateSurfaceFormats)
{
    for (vk::Format fmt : candidateSurfaceFormats)
    {
        for (vk::SurfaceFormatKHR supportedFormat : supportedSurfaceFormats)
        {
            if (supportedFormat.format == fmt)
            {
                return supportedFormat.format;
            }
        }
    }
    return supportedSurfaceFormats[0].format;
}

vk::ColorSpaceKHR qhwi::ext::VkSwapchainUtil::chooseColorSpace(const std::span<const vk::ColorSpaceKHR>& supportedColorSpaces,
                                                               const std::span<const vk::ColorSpaceKHR>& candidateColorSpaces)
{
    for (vk::ColorSpaceKHR colorSpace : candidateColorSpaces)
    {
        if (Algorithm::contains(supportedColorSpaces, colorSpace))
        {
            return colorSpace;
        }
    }
    return supportedColorSpaces[0];
}

qre::StaticVector<vk::ColorSpaceKHR, qhwi::MaxColorSpaces> qhwi::ext::VkSwapchainUtil::getSupportedColorSpacesForFormat(
    const std::span<const vk::SurfaceFormatKHR>& supportedSurfaceFormats,
    vk::Format format)
{
    qre::StaticVector<vk::ColorSpaceKHR, qhwi::MaxColorSpaces> supportedColorSpaces;
    for (vk::SurfaceFormatKHR surfaceFormat : supportedSurfaceFormats)
    {
        if (surfaceFormat.format == format)
        {
            supportedColorSpaces.push_back(surfaceFormat.colorSpace);
        }
    }
    return supportedColorSpaces;
}

void qhwi::ext::VkSwapchain::acquireNextImage()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchain::acquireNextImage", QHWI_COLOR_GRAPHICS);
    auto vkDevice = m_device->get();

    vk::UniqueSemaphore acquireSemaphore;

    if (m_recycledImageAcquiredSemaphores.empty())
    {
        acquireSemaphore = vkDevice.createSemaphoreUnique(vk::SemaphoreCreateInfo());
    }
    else
    {
        acquireSemaphore = std::move(m_recycledImageAcquiredSemaphores.front());
        m_recycledImageAcquiredSemaphores.pop();
    }

    vk::Result vkResult = vkDevice.acquireNextImageKHR(m_swapchain.get(), UINT64_MAX, acquireSemaphore.get(), {}, &m_currentImageIndex);
    if (vkResult != vk::Result::eSuccess)
    {
        QHWI_LOGW("acquire next image result: %s", vk::to_string(vkResult).c_str());
    }

    // Recyle the previous acquired semaphore and save the new one
    if (m_imageAcquiredSemaphore.get() != nullptr)
    {
        m_recycledImageAcquiredSemaphores.push(std::move(m_imageAcquiredSemaphore));
    }
    m_imageAcquiredSemaphore = std::move(acquireSemaphore);
}

void qhwi::ext::VkSwapchain::presentImage(CommandQueue* queue)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkSwapchain::presentImage", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(queue);
    if (m_frameRendered == false)
    {
        QHWI_ERR("no frame rendered");
        return;
    }
    auto vkQueue = static_cast<VkCommandQueue*>(queue)->get();
    vk::Semaphore waitSemaphores[] = { getRenderCompleteSemaphore(m_currentImageIndex) };
    vk::SwapchainKHR swapchains[1] = { m_swapchain.get() };
    uint32_t imageIndices[1] = { m_currentImageIndex };
    vk::Result vkResult = vkQueue.presentKHR(
        vk::PresentInfoKHR().setWaitSemaphoreCount(1).setPWaitSemaphores(waitSemaphores).setSwapchainCount(1).setSwapchains(swapchains).setPImageIndices(imageIndices));
    if (vkResult != vk::Result::eSuccess)
    {
        //QHWI_LOGW("queue present result: %s", vk::to_string(vkResult).c_str());
    }
    m_frameRendered = false;
}

qhwi::Texture* qhwi::ext::VkSwapchain::getColorImage(uint32_t index) const
{
    QRE_ASSERT(index < MaxSwapchainImages);
    return m_colorSwapchainTextures[index];
}

void qhwi::VkImageViewCache::init(qhwi::VkDevice* device)
{
    QHWI_ASSERT(device);
    m_device = device;
}

vk::ImageView qhwi::VkImageViewCache::get(const VkTexture* texture, vk::ImageViewType imageViewType, const vk::ImageSubresourceRange* subresourceRange)
{
    QHWI_ASSERT(texture != nullptr);
    vk::ImageViewCreateInfo imageViewCreateInfo = texture->defaultImageViewCreateInfo();
    imageViewCreateInfo.setViewType(imageViewType).setSubresourceRange((subresourceRange == nullptr) ? imageViewCreateInfo.subresourceRange : *subresourceRange);

    auto iter = m_cache.find(imageViewCreateInfo);
    if (iter != m_cache.end())
    {
        return iter->second.imageView.get();
    }

    auto& entry = m_cache[imageViewCreateInfo];

    int w = texture->desc()->w;
    int h = texture->desc()->h;
    vk::ImageViewSampleWeightCreateInfoQCOM imageViewSampleWeightCreateInfoQCOM{};

    if (m_device->getVkFeaturesInfo().supportsImageProcessingQCOM() && ((texture->desc()->usage & TextureUsage_SampleWeightQCOM) != 0))
    {
        imageViewSampleWeightCreateInfoQCOM.sType = vk::StructureType::eImageViewSampleWeightCreateInfoQCOM;
        imageViewSampleWeightCreateInfoQCOM.filterCenter.x = w / 2;
        imageViewSampleWeightCreateInfoQCOM.filterCenter.y = h / 2;
        imageViewSampleWeightCreateInfoQCOM.numPhases = texture->desc()->arrayLayers;
        imageViewSampleWeightCreateInfoQCOM.filterSize.width = w;
        imageViewSampleWeightCreateInfoQCOM.filterSize.height = h;

        imageViewCreateInfo.pNext = &imageViewSampleWeightCreateInfoQCOM;
    }

    entry.imageView = m_device->get().createImageViewUnique(imageViewCreateInfo);
    entry.imageViewCreateInfo = imageViewCreateInfo;
    entry.subresourceRange = imageViewCreateInfo.subresourceRange;

    if (texture->desc()->name)
    {
        setName(m_device->get(), vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eImageView, (uint64_t)(VkImageView)entry.imageView.get(), texture->desc()->name));
    }

    return entry.imageView.get();
}

size_t qhwi::VkImageViewCache::VkImageViewCreateInfoHash::operator()(const vk::ImageViewCreateInfo& v) const
{
    uint64_t hash = qre::Fnv1a64Init();
    return hash_util::Fnv1a64Continue(hash, v);
}

uint32_t qhwi::VkTexture::calculatePitch(Format format, uint32_t w) const
{
    return FormatUtil::getRowPitch(format, w);
}

uint32_t qhwi::VkTexture::pitch(uint16_t mipLevel) const
{
    return calculatePitch(m_desc.format, std::max(m_desc.w >> mipLevel, 1u));
}

void qhwi::VkTexture::init(const Description& desc,
                           vk::ImageCreateInfo& imageCreateInfo,
                           vk::OpticalFlowImageFormatInfoNV& opticalFlowImageFormatInfoNV,
                           vk::ExternalMemoryImageCreateInfo& externalMemoryImageCreateInfo)
{
    const uint32_t w = desc.w, h = desc.h, d = desc.d, arrayLayers = desc.arrayLayers;
    const SampleCount numSamples = desc.numSamples;
    const Format format = desc.format;
    auto& mipLevels = m_desc.mipLevels;
    if (mipLevels == 0)
    {
        mipLevels = calculateMipLevels(w, h, d);
    }
    const TextureType type = desc.type;
    const auto usage = desc.usage;
    vk::ImageCreateFlags imageCreateFlags = vk::ImageCreateFlagBits(0U);
    vk::ImageType vkImageType;

    switch (type)
    {
    case TextureType::e1D:
    case TextureType::e1DArray:
        vkImageType = vk::ImageType::e1D;
        break;
    case TextureType::e2D:
    case TextureType::e2DArray:
        vkImageType = vk::ImageType::e2D;
        break;
    case TextureType::e3D:
        vkImageType = vk::ImageType::e3D;
        break;
    case TextureType::eCube:
        vkImageType = vk::ImageType::e2D;
        imageCreateFlags = vk::ImageCreateFlagBits::eCubeCompatible;
        break;
    default:
        vkImageType = vk::ImageType::e2D;
        break;
    }

    vk::ImageUsageFlags imageUsageFlags = qhwi::vkImageUsageFlags(usage);
    if ((usage & (TextureUsage_TransientAttachment | TextureUsage_TileMemoryQCOM)) == 0)
    {
        imageUsageFlags |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
    }

    vk::ImageLayout initialImageLayout = vk::ImageLayout::eUndefined;
    auto vkFormat = qhwi::vk(format);

    imageCreateInfo = vk::ImageCreateInfo(imageCreateFlags,
                                          vkImageType,
                                          vkFormat,
                                          vk::Extent3D(w, h, d),
                                          mipLevels,
                                          arrayLayers,
                                          qhwi::vk(numSamples),
                                          vk::ImageTiling::eOptimal,
                                          imageUsageFlags,
                                          {},
                                          {},
                                          initialImageLayout);
    void* pNext = nullptr;
    if (desc.usage & TextureUsage_OpticalFlowInput)
    {
        opticalFlowImageFormatInfoNV = vk::OpticalFlowImageFormatInfoNV(vk::OpticalFlowUsageFlagBitsNV::eInput).setPNext(pNext);
        pNext = &opticalFlowImageFormatInfoNV;
    }
    else if (desc.usage & TextureUsage_OpticalFlowOutput)
    {
        opticalFlowImageFormatInfoNV = vk::OpticalFlowImageFormatInfoNV(vk::OpticalFlowUsageFlagBitsNV::eOutput).setPNext(pNext);
        pNext = &opticalFlowImageFormatInfoNV;
    }
    if (desc.heapFlags & HeapFlag_Shared)
    {
        externalMemoryImageCreateInfo.setHandleTypes(s_ExternalMemoryHandleTypeFlags).setPNext(pNext);
        pNext = &externalMemoryImageCreateInfo;
    }
    imageCreateInfo.setPNext(pNext);
    m_format = imageCreateInfo.format;
    m_extent = imageCreateInfo.extent;

    //TODO: "support additional image view types"
    vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eNoneKHR;
    bool formatSupportsDepth = supportsDepth(vkFormat);
    bool formatSupportsStencil = supportsStencil(vkFormat);
    if (formatSupportsDepth)
    {
        aspectFlags |= vk::ImageAspectFlagBits::eDepth;
    }
    if (formatSupportsStencil)
    {
        aspectFlags |= vk::ImageAspectFlagBits::eStencil;
    }
    if (!formatSupportsDepth && !formatSupportsStencil)
    {
        aspectFlags = vk::ImageAspectFlagBits::eColor;
    }
    auto subresourceRange = vk::ImageSubresourceRange(aspectFlags, 0, mipLevels, 0, arrayLayers);
    m_defaultImageViewCreateInfo = vk::ImageViewCreateInfo({},
                                                           {},
                                                           qhwi::vk(type),
                                                           vkFormat,
                                                           { qhwi::vk(desc.rSwizzle), qhwi::vk(desc.gSwizzle), qhwi::vk(desc.bSwizzle), qhwi::vk(desc.aSwizzle) },
                                                           subresourceRange);
    m_numSubresources = mipLevels * arrayLayers;
}

qhwi::VkTexture::VkTexture(VkDevice* device, const Description& desc, vk::Image image, vk::ImageView imageView)
    : Texture(desc)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkTexture::VkTexture", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(m_device);

    vk::ImageCreateInfo imageCreateInfo;
    vk::ExternalMemoryImageCreateInfo externalMemoryImageCreateInfo{};
    vk::OpticalFlowImageFormatInfoNV opticalFlowImageFormatInfo{};
    init(desc, imageCreateInfo, opticalFlowImageFormatInfo, externalMemoryImageCreateInfo);

    auto vkDevice = m_device->get();

    if (image)
    {
        QRE_ASSERT(!m_image);

        m_image = image;
        m_ownsImage = false;
    }
    else
    {
        m_VmaAllocation = {};
        m_ownsImage = true;
        bool tileMemory = (VkImageUsageFlags(imageCreateInfo.usage) & VK_IMAGE_USAGE_TILE_MEMORY_BIT_QCOM);
        if (tileMemory)
        {
            // Use explicit allocation method for tile memory targeted textures
            m_image = vkDevice.createImage(imageCreateInfo);

            auto imgMemReqs2 = vk::ImageMemoryRequirementsInfo2().setImage(m_image);
            m_tileMemReqs = {};
            m_tileMemReqs.sType = VK_STRUCTURE_TYPE_TILE_MEMORY_REQUIREMENTS_QCOM;
            m_tileMemReqs.pNext = NULL;
            vk::MemoryRequirements2 memReqs2{};
            memReqs2.pNext = &m_tileMemReqs;

            vkDevice.getImageMemoryRequirements2(&imgMemReqs2, &memReqs2);
            QHWI_LOGI("Texture tileMemReqs.size: %lld tileMemReqs.alignment: %lld", m_tileMemReqs.size, m_tileMemReqs.alignment);
            QHWI_ASSERT(m_tileMemReqs.size);
            QHWI_ASSERT(m_tileMemReqs.size >= memReqs2.memoryRequirements.size);

            m_tileMemory = device->tileMem();
            m_tileMemoryOffset = device->allocateTileMemory(m_tileMemReqs.size, m_tileMemReqs.alignment);
            vkDevice.bindImageMemory(m_image, m_tileMemory, m_tileMemoryOffset);
        }
        else
        {
            //use Vulkan Memory Allocator
            VkImageCreateInfo imageInfo(imageCreateInfo);

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            if (desc.usage & TextureUsage_TransientAttachment)
            {
                // Place transient attachments in lazily allocated memory, if available
                allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
            }
            if (m_desc.heapFlags & HeapFlag_Shared)
            {
                // Force dedicated allocation for shared resources for robust interop.
                allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                // use shared pool
                allocInfo.pool = m_device->getVmaSharedAllocationsPool();
            }

            V(vmaCreateImage(m_device->getVmaAllocator(), &imageInfo, &allocInfo, (VkImage*)&m_image, &m_VmaAllocation, nullptr));

            if (m_desc.heapFlags & HeapFlag_Shared)
            {
                m_sharedHandle = ::getSharedHandle(vkDevice, m_VmaAllocation->GetMemory());
            }
        }
    }
    if (desc.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eImage, (uint64_t)(VkImage)m_image, desc.name));
        if (m_ownsImage && m_VmaAllocation)
        {
            vmaSetAllocationName(m_device->getVmaAllocator(), m_VmaAllocation, desc.name);
        }
    }

    m_currentImageLayout = imageCreateInfo.initialLayout;
    m_defaultImageViewCreateInfo.image = m_image;
    vk::ImageViewSampleWeightCreateInfoQCOM imageViewSampleWeightCreateInfoQCOM{};

    if (m_device->getVkFeaturesInfo().supportsImageProcessingQCOM() && ((desc.usage & TextureUsage_SampleWeightQCOM) != 0))
    {
        imageViewSampleWeightCreateInfoQCOM.sType = vk::StructureType::eImageViewSampleWeightCreateInfoQCOM;
        imageViewSampleWeightCreateInfoQCOM.filterCenter.x = desc.w / 2;
        imageViewSampleWeightCreateInfoQCOM.filterCenter.y = desc.h / 2;
        imageViewSampleWeightCreateInfoQCOM.numPhases = desc.arrayLayers;
        imageViewSampleWeightCreateInfoQCOM.filterSize.width = desc.w;
        imageViewSampleWeightCreateInfoQCOM.filterSize.height = desc.h;

        m_defaultImageViewCreateInfo.pNext = &imageViewSampleWeightCreateInfoQCOM;
    }
    if (imageView)
    {
        m_defaultImageView.reset(imageView);
        m_ownsDefaultImageView = false;
    }
    else
    {
        m_ownsDefaultImageView = true;
        m_defaultImageView = vkDevice.createImageViewUnique(m_defaultImageViewCreateInfo);
    }
}

qhwi::VkTexture::~VkTexture()
{
    // Detach the underlying object from the unique handle if qhwi doesn't own the image.
    // Otherwise, the unique handle would attempt to destroy the object.
    if (m_image)
    {
        m_device->onDestroyResource(m_image);
    }
    if (m_VmaAllocation)
    {
        // image was allocated by vma
        vmaDestroyImage(m_device->getVmaAllocator(), m_image, m_VmaAllocation);
    }
    else if (m_tileMemory)
    {
        // TODO: free region at offset: m_tileMemoryOffset
        m_device->freeTileMemory(m_tileMemoryOffset);
    }
    if (!m_ownsDefaultImageView)
    {
        m_defaultImageView.release();
    }
}

void qhwi::VkTexture::updateFromHandle(Handle handle) {}

vk::ImageView qhwi::VkTexture::getImageView(vk::ImageViewType imageViewType, vk::ImageSubresourceRange* subresourceRange) const
{
    const auto& defaultImageViewCreateInfo = m_defaultImageViewCreateInfo;

    if (defaultImageViewCreateInfo.viewType == imageViewType && subresourceRange == nullptr)
    {
        return m_defaultImageView.get();
    }
    else
    {
        return m_device->getImageView(this, imageViewType, subresourceRange);
    }
}

vk::SamplerCreateInfo qhwi::VkSampler::initCreateInfo(const Description& desc)
{
    vk::SamplerCreateInfo s{};
    s.minFilter = qhwi::vk(desc.minFilter);
    s.magFilter = qhwi::vk(desc.magFilter);
    s.mipmapMode = qhwi::vkSamplerMipmapMode(desc.mipFilter);
    s.addressModeU = qhwi::vk(desc.addressModeU);
    s.addressModeV = qhwi::vk(desc.addressModeV);
    s.addressModeW = qhwi::vk(desc.addressModeW);
    s.mipLodBias = desc.mipLodBias;
    s.anisotropyEnable = desc.maxAnisotropy > 1.0f;
    s.maxAnisotropy = desc.maxAnisotropy;
    s.minLod = desc.minLod;
    s.maxLod = desc.maxLod;
    s.borderColor = vk::BorderColor::eFloatTransparentBlack;
    s.unnormalizedCoordinates = false;
    s.flags = vkSamplerCreateFlags(desc.flags);
    s.compareOp = qhwi::vk(desc.compareOp);
    s.compareEnable = desc.compareOp != CompareOp::eNone;
    return s;
}

qhwi::VkSampler::VkSampler(VkDevice* device, const Description& desc)
    : m_device(device)
{
    QHWI_ASSERT(device);
    auto reductionModeCreateInfo = vk::SamplerReductionModeCreateInfoEXT().setReductionMode(qhwi::vk(desc.reductionMode));
    vk::SamplerCreateInfo createInfo = initCreateInfo(desc);
    if (m_device->supportsSamplerFilterMinMax())
    {
        createInfo.setPNext(&reductionModeCreateInfo);
    }
    auto vkDevice = m_device->get();
    m_sampler = vkDevice.createSamplerUnique(createInfo);
}

qhwi::VkShader::VkShader(VkDevice* device, const Description& description, const void* byteCode, size_t length)
    : m_device(device)
{
    QHWI_ASSERT(device);
    QHWI_ASSERT(byteCode);
    QHWI_ASSERT(length);

    m_desc = description;
    auto vkDevice = m_device->get();

    vk::ShaderModuleCreateInfo createInfo = vk::ShaderModuleCreateInfo({}, length, (const uint32_t*)byteCode);

    m_shaderModule = vkDevice.createShaderModuleUnique(createInfo);
}

vk::PipelineShaderStageCreateInfo qhwi::vkPipelineShaderStageCreateInfo(const Shader* shader, ShaderStage shaderStage, const vk::SpecializationInfo* pSpecializationInfo)
{
    QHWI_ASSERT(shader);
    auto shaderDesc = shader->desc();
    const qhwi::VkShader* vkShader = static_cast<const qhwi::VkShader*>(shader);
    QHWI_ASSERT(shaderDesc->entryPointName);
    return vk::PipelineShaderStageCreateInfo({}, qhwi::vk(shaderStage), vkShader->get(), shaderDesc->entryPointName, pSpecializationInfo);
}

void qhwi::vk(const BlendState& blendState, qhwi::VkPipelineColorBlendStateCreateInfo& r)
{
    r.numAttachments = uint32_t(blendState.numColorAttachments());
    for (size_t j = 0; j < blendState.numColorAttachments(); j++)
    {
        const auto& s = blendState.colorAttachmentBlendStates[j];
        r.attachmentBlendStates[j] = vk::PipelineColorBlendAttachmentState(s.enableBlend,
                                                                           vk(s.srcColorBlendFactor),
                                                                           vk(s.dstColorBlendFactor),
                                                                           vk(s.colorBlendOp),
                                                                           vk(s.srcAlphaBlendFactor),
                                                                           vk(s.dstAlphaBlendFactor),
                                                                           vk(s.alphaBlendOp),
                                                                           vk::ColorComponentFlags(s.colorWriteMask));
    }

    bool logicOpEnable = false;
    LogicOp logicOp = LogicOp::eClear;

    if (blendState.numColorAttachments() > 0)
    {
        logicOpEnable = blendState.colorAttachmentBlendStates[0].enableLogicOp;
        logicOp = blendState.colorAttachmentBlendStates[0].logicOp;
        for (size_t j = 1; j < blendState.numColorAttachments(); j++)
        {
            QHWI_ASSERT(logicOpEnable == blendState.colorAttachmentBlendStates[j].enableLogicOp);
            QHWI_ASSERT(logicOp == blendState.colorAttachmentBlendStates[j].logicOp);
        }
    }

    r.createInfo = vk::PipelineColorBlendStateCreateInfo({}, logicOpEnable, vk(logicOp), r.numAttachments, r.attachmentBlendStates, r.blendConstants);
}

vk::PipelineColorBlendAttachmentState qhwi::vk(const ColorAttachmentBlendState& s)
{
    return vk::PipelineColorBlendAttachmentState(s.enableBlend,
                                                 vk(s.srcColorBlendFactor),
                                                 vk(s.dstColorBlendFactor),
                                                 vk(s.colorBlendOp),
                                                 vk(s.srcAlphaBlendFactor),
                                                 vk(s.dstAlphaBlendFactor),
                                                 vk(s.alphaBlendOp),
                                                 vk::ColorComponentFlags(s.colorWriteMask));
}

vk::PipelineDepthStencilStateCreateInfo qhwi::vk(const DepthStencilState& s)
{
    return vk::PipelineDepthStencilStateCreateInfo({},
                                                   s.enableDepthTest,
                                                   s.enableDepthWrite,
                                                   vk(s.depthCompareOp),
                                                   VK_FALSE,
                                                   s.enableStencilTest,
                                                   vk(s.frontFace, s),
                                                   vk(s.backFace, s));
}

vk::StencilOpState qhwi::vk(const StencilOpState& s, const DepthStencilState& r)
{
    return vk::StencilOpState(vk(s.failOp), vk(s.passOp), vk(s.depthFailOp), vk(s.compareOp), r.stencilReadMask, r.stencilWriteMask, r.stencilReference);
}

vk::PipelineRasterizationStateCreateInfo qhwi::vk(const RasterizerState& s)
{
    const float DefaultLineWidth = 1.0f;
    vk::FrontFace frontFace = s.frontFaceCounterClockwise ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise;

    //depth clip: clip vertices past the far plane
    //depth clamp: ignore near and far plane, clamp depth values instead (to range -1 to 1)
    bool enableDepthClamp = !s.enableDepthClipping;
    return vk::PipelineRasterizationStateCreateInfo({}, enableDepthClamp, false, vk(s.fillMode), vk(s.cullMode), frontFace, false, 0.0f, 0.0f, 0.0f, DefaultLineWidth);
}

vk::PipelineMultisampleStateCreateInfo qhwi::vk(const MultisampleState& s)
{
    return vk::PipelineMultisampleStateCreateInfo({}, vk(s.sampleCount), VK_FALSE, 0.0f, nullptr, s.enableAlphaToCoverage, VK_FALSE);
}

void qhwi::vk(VkGraphicsPipeline::Description* graphicsPipelineDesc, const VertexInputState& s, qhwi::VkPipelineVertexInputStateCreateInfo& r)
{
    r.numVertexInputAttributeDescriptions = uint32_t(s.vertexInputAttributeDescriptions.size());
    for (size_t j = 0; j < s.vertexInputAttributeDescriptions.size(); j++)
    {
        const auto& vertexInputAttrDesc = s.vertexInputAttributeDescriptions[j];
        r.vertexInputAttributeDescriptions[j] =
            vk::VertexInputAttributeDescription(uint32_t(j), vertexInputAttrDesc.binding, qhwi::vk(vertexInputAttrDesc.format), vertexInputAttrDesc.offset);
    }
    auto& numVertexInputBindingDescriptions = graphicsPipelineDesc->vk.numVertexInputBindingDescriptions;
    auto& vertexInputBindingDescriptions = graphicsPipelineDesc->vk.vertexInputBindingDescriptions;
    for (uint32_t j = 0; j < s.vertexInputAttributeDescriptions.size(); j++)
    {
        const auto& vertexInputAttrDesc = s.vertexInputAttributeDescriptions[j];
        uint32_t binding = vertexInputAttrDesc.binding;
        QHWI_ASSERT(binding < MaxVertexShaderInputBindings);
        auto& vertexInputBindingDesc = vertexInputBindingDescriptions[binding];
        vertexInputBindingDesc = vk::VertexInputBindingDescription(binding, vertexInputAttrDesc.stride, qhwi::vk(vertexInputAttrDesc.inputRate));
        numVertexInputBindingDescriptions = std::max(numVertexInputBindingDescriptions, binding + 1);
    }

    r.createInfo = (vk::PipelineVertexInputStateCreateInfo({},
                                                           numVertexInputBindingDescriptions,
                                                           vertexInputBindingDescriptions,
                                                           r.numVertexInputAttributeDescriptions,
                                                           r.vertexInputAttributeDescriptions));
}

qhwi::VkPipelineLayout::Description::Description(const PipelineLayout::Description& desc)
    : base(desc)
{
    vk.numDescriptorSetLayouts = uint32_t(base.descriptorSetLayouts.size());
    if (base.descriptorSetLayouts.size() > 0)
    {
        QHWI_ASSERT(!base.descriptorSetLayouts.empty());
    }
    for (size_t j = 0; j < base.descriptorSetLayouts.size(); j++)
    {
        QHWI_ASSERT(base.descriptorSetLayouts[j]);
        vk.descriptorSetLayouts[j] = static_cast<VkDescriptorSetLayout*>(base.descriptorSetLayouts[j].Get())->get();
    }
    vk.numPushConstantRanges = uint32_t(base.pushConstantRanges.size());
    for (size_t j = 0; j < base.pushConstantRanges.size(); j++)
    {
        const auto& pushConstantRange = base.pushConstantRanges[j];
        vk.pushConstantRanges[j] = vk::PushConstantRange(qhwi::vkShaderStageFlags(pushConstantRange.shaderStages), pushConstantRange.offset, pushConstantRange.size);
    }
    vk.createInfo = vk::PipelineLayoutCreateInfo({}, vk.numDescriptorSetLayouts, vk.descriptorSetLayouts, vk.numPushConstantRanges, vk.pushConstantRanges, nullptr);
}

qhwi::VkPipelineLayout::VkPipelineLayout(VkDevice* device, const Description& desc)
    : PipelineLayout(desc.base)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkPipelineLayout::VkPipelineLayout", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    auto vkDevice = m_device->get();
    m_pipelineLayout = vkDevice.createPipelineLayoutUnique(desc.vk.createInfo);
    if (desc.base.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::ePipelineLayout, (uint64_t)(::VkPipelineLayout)m_pipelineLayout.get(), desc.base.name));
    };
}

qhwi::VkGraphicsPipeline::Description::Description(const GraphicsPipeline::Description& desc)
    : base(desc)
{
}

void qhwi::VkGraphicsPipeline::Description::init()
{
    if (!initOnce)
    {
        return;
    }
    initOnce = false;
    void* pLast = nullptr;
    vk::PipelineCreateFlags createflags = (vk::PipelineCreateFlags)0;

    vk.numShaderStages = 0;
    if (vk.graphicPipelineLibraries.size() > 0)
    {
        vk.pipelineLibraryCreateInfo =
            vk::PipelineLibraryCreateInfoKHR((uint32_t)vk.graphicPipelineLibraries.size(), (const vk::Pipeline*)vk.graphicPipelineLibraries.data(), nullptr);
        pLast = &vk.pipelineLibraryCreateInfo;

        vk.createInfo = vk::GraphicsPipelineCreateInfo(createflags).setPNext(pLast);
        return;
    }

    constexpr vk::GraphicsPipelineLibraryFlagsEXT CompleteGPLSet =
        vk::GraphicsPipelineLibraryFlagBitsEXT::eVertexInputInterface | vk::GraphicsPipelineLibraryFlagBitsEXT::ePreRasterizationShaders |
        vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentShader | vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentOutputInterface;

    vk::PipelineVertexInputStateCreateInfo* pVertexInputStateCreateInfo = nullptr;
    vk::PipelineViewportStateCreateInfo* pViewportStateCreateInfo = nullptr;
    vk::PipelineDepthStencilStateCreateInfo* pDepthStencilStateCreateInfo = nullptr;
    vk::PipelineMultisampleStateCreateInfo* pMultisampleStateCreateInfo = nullptr;
    vk::PipelineInputAssemblyStateCreateInfo* pInputAssemblyStateCreateInfo = nullptr;
    vk::PipelineRasterizationStateCreateInfo* pRasterizationStateCreateInfo = nullptr;
    vk::PipelineColorBlendStateCreateInfo* pColorBlendStateCreateInfo = nullptr;
    vk::PipelineDynamicStateCreateInfo* pDynamicStateCreateInfo = nullptr;

    auto shaderStagePtr = vk.shaderStages;
    auto ssCreateInfo = qhwi::vkPipelineShaderStageCreateInfo;
    auto pipelineSectionFlags = vk.graphicsPipelineLibraryCreateInfo.flags;
    bool isGraphicsPipelineLibrary = (pipelineSectionFlags > (vk::GraphicsPipelineLibraryFlagsEXT)0 && pipelineSectionFlags < CompleteGPLSet);
    if (!isGraphicsPipelineLibrary)
    {
        // This is a monolithic pipeline.  Enable all pipeline subsets
        pipelineSectionFlags = CompleteGPLSet;
    }
    // Vertex input
    if (pipelineSectionFlags & vk::GraphicsPipelineLibraryFlagBitsEXT::eVertexInputInterface)
    {
        qhwi::vk(this, base.vertexInputState, vk.vertexInputState);
        pVertexInputStateCreateInfo = &vk.vertexInputState.createInfo;
        bool primitiveRestartEnable = (base.primitiveTopology == PrimitiveTopology::eLineStrip) || (base.primitiveTopology == PrimitiveTopology::eTriangleStrip);
        vk.inputAssemblyState = vk::PipelineInputAssemblyStateCreateInfo({}, qhwi::vk(base.primitiveTopology), primitiveRestartEnable);
        pInputAssemblyStateCreateInfo = &vk.inputAssemblyState;
    }

    // Vertex shaders
    if (pipelineSectionFlags & vk::GraphicsPipelineLibraryFlagBitsEXT::ePreRasterizationShaders)
    {
        if (base.vertexShader)
        {
            vk.numShaderStages++;
            *shaderStagePtr++ = ssCreateInfo(base.vertexShader.Get(), ShaderStage_Vertex, shaderStagePtr->pSpecializationInfo);
        }
        if (base.geometryShader)
        {
            vk.numShaderStages++;
            *shaderStagePtr++ = ssCreateInfo(base.geometryShader.Get(), ShaderStage_Geometry, shaderStagePtr->pSpecializationInfo);
        }
        if (base.amplificationShader)
        {
            vk.numShaderStages++;
            *shaderStagePtr++ = ssCreateInfo(base.amplificationShader.Get(), ShaderStage_Amplification, shaderStagePtr->pSpecializationInfo);
        }
        if (base.meshShader)
        {
            vk.numShaderStages++;
            *shaderStagePtr++ = ssCreateInfo(base.meshShader.Get(), ShaderStage_Mesh, shaderStagePtr->pSpecializationInfo);
        }

        // Set up dynamic state
        vk.numDynamicStates = 2;
        auto dynamicStatesPtr = vk.dynamicStates;
        *dynamicStatesPtr++ = vk::DynamicState::eViewport;
        *dynamicStatesPtr++ = vk::DynamicState::eScissor;
        vk.dynamicState = vk::PipelineDynamicStateCreateInfo({}, vk.numDynamicStates, vk.dynamicStates);
        pDynamicStateCreateInfo = &vk.dynamicState;

        // Viewport State
        vk.viewportState = vk::PipelineViewportStateCreateInfo({}, base.viewportState.numViewports, nullptr, base.scissorState.numScissors, nullptr);
        pViewportStateCreateInfo = &vk.viewportState;

        // RasterizerState
        vk.rasterizerState = qhwi::vk(base.rasterizerState);
        pRasterizationStateCreateInfo = &vk.rasterizerState;
    }

    // fragment
    if (pipelineSectionFlags & vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentShader)
    {
        if (base.pixelShader)
        {
            vk.numShaderStages++;
            *shaderStagePtr++ = ssCreateInfo(base.pixelShader.Get(), ShaderStage_Pixel, shaderStagePtr->pSpecializationInfo);
        }

        vk.depthStencilState = qhwi::vk(base.depthStencilState);
        pDepthStencilStateCreateInfo = &vk.depthStencilState;
    }

    // Fragment Output Blend
    if (pipelineSectionFlags & vk::GraphicsPipelineLibraryFlagBitsEXT::eFragmentOutputInterface)
    {
        // Blend state
        if (base.blendState.colorAttachmentBlendStates.empty())
        {
            uint32_t numColorAttachments = uint32_t(base.numColorAttachments());
            base.blendState.colorAttachmentBlendStates.resize(numColorAttachments, {});
        }
        qhwi::vk(base.blendState, vk.blendState);
        pColorBlendStateCreateInfo = &vk.blendState.createInfo;

        // Multisample
        vk.multisampleState = qhwi::vk(base.multisampleState);
        pMultisampleStateCreateInfo = &vk.multisampleState;
        uint32_t numColorAttachments = uint32_t(base.numColorAttachments());
        const auto& colorAttachmentRemap = base.colorAttachmentRemapping;
        vk.colorAttachmentFormats.resize(numColorAttachments);
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            vk.colorAttachmentFormats[j] = qhwi::vk(base.colorAttachmentFormats[j]);
        }

        vk.depthFormat = qhwi::vk(base.depthStencilFormat);
        bool supportsStencil = qhwi::supportsStencil(vk.depthFormat);
        vk.stencilFormat = supportsStencil ? vk.depthFormat : vk::Format::eUndefined;
        if (!vk.createInfo.renderPass)
        {
            vk.pipelineRenderingCreateInfo =
                vk::PipelineRenderingCreateInfoKHR(0, uint32_t(vk.colorAttachmentFormats.size()), vk.colorAttachmentFormats.data(), vk.depthFormat, vk.stencilFormat);
            vk.pipelineRenderingCreateInfo.setPNext(pLast);
            pLast = &vk.pipelineRenderingCreateInfo;
        }

        uint32_t numViews = base.multiViewState.numViews;
        bool multiView = (numViews != 0);
        if (multiView)
        {
            uint32_t viewMask = (1 << numViews) - 1;
            vk.pipelineRenderingCreateInfo.setViewMask(viewMask);
        }

        if (!colorAttachmentRemap.empty())
        {
            vk.renderingAttachmentLocationInfo = vk::RenderingAttachmentLocationInfoKHR(uint32_t(colorAttachmentRemap.size()), colorAttachmentRemap.data());
            vk.renderingAttachmentLocationInfo.setPNext(pLast);
            pLast = &vk.renderingAttachmentLocationInfo;
        }
    }
    if (isGraphicsPipelineLibrary)
    {
        createflags |= (vk::PipelineCreateFlags)vk::PipelineCreateFlagBits::eLibraryKHR;
        vk.graphicsPipelineLibraryCreateInfo = vk::GraphicsPipelineLibraryCreateInfoEXT(pipelineSectionFlags, nullptr);
        vk.graphicsPipelineLibraryCreateInfo.pNext = pLast;
        pLast = &vk.graphicsPipelineLibraryCreateInfo;
    }
    vk.createInfo = vk::GraphicsPipelineCreateInfo(createflags,
                                                   vk.numShaderStages,
                                                   vk.shaderStages,
                                                   pVertexInputStateCreateInfo,
                                                   pInputAssemblyStateCreateInfo,
                                                   nullptr, //pTessellationState
                                                   pViewportStateCreateInfo,
                                                   pRasterizationStateCreateInfo,
                                                   pMultisampleStateCreateInfo,
                                                   pDepthStencilStateCreateInfo,
                                                   pColorBlendStateCreateInfo,
                                                   pDynamicStateCreateInfo,
                                                   {},                       // pipeline layout
                                                   vk.createInfo.renderPass, // renderpass (allow user to override)
                                                   vk.createInfo.subpass);   // subpass (allow user to override)

    // If VK_EXT_custom_resolve and Dynamic Rendering are being used
    if (base.customResolve && !vk.createInfo.renderPass)
    {
        uint32_t numResolveFormats = static_cast<uint32_t>(base.customResolveAttachmentFormats.size());
        QHWI_ASSERT_MSG(numResolveFormats > 0, "Custom resolve pipelines must specify customResolveAttachmentFormats.");
        QHWI_ASSERT_MSG(numResolveFormats <= vk.colorAttachmentFormats.size(), "customResolveAttachmentFormats cannot exceed colorAttachmentFormats.");
        vk.customResolveAttachmentFormats.resize(vk.colorAttachmentFormats.size());
        for (uint32_t j = 0; j < vk.customResolveAttachmentFormats.size(); j++)
        {
            vk.customResolveAttachmentFormats[j] = (j < numResolveFormats) ? qhwi::vk(base.customResolveAttachmentFormats[j]) : vk::Format::eUndefined;
        }
        vk.customResolveCreateInfo = vk::CustomResolveCreateInfoEXT(true,
                                                                    static_cast<uint32_t>(vk.customResolveAttachmentFormats.size()),
                                                                    vk.customResolveAttachmentFormats.data(),
                                                                    vk.depthFormat,
                                                                    vk.stencilFormat);
        vk.customResolveCreateInfo.setPNext(pLast);
        pLast = &vk.customResolveCreateInfo;
    }

    vk.createInfo.setPNext(pLast);
}

bool qhwi::VkGraphicsPipeline::supportsDynamicRenderingLocalRead() const
{
    return m_device->supportsDynamicRenderingLocalRead();
}

static bool checkCustomResolveEnabled(const qhwi::VkDevice* device, const char* operation)
{
    const bool enabled = device->desc()->enableCustomResolve;
    const bool supported = device->supportsCustomResolve();
    QHWI_ASSERT_MSG(enabled, "%s requested, but Device::Description::enableCustomResolve is false.", operation);
    QHWI_ASSERT_MSG(supported, "%s requested, but QHWI custom resolve requires VK_EXT_custom_resolve, dynamic rendering, and dynamic rendering local read support.", operation);
    return enabled && supported;
}

qhwi::VkGraphicsPipeline::VkGraphicsPipeline(VkDevice* device, const Description& desc1)
    : m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkGraphicsPipeline::VkGraphicsPipeline", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    Description desc = desc1;
    desc.base.customResolve = desc.base.customResolve && checkCustomResolveEnabled(m_device, "Custom resolve pipeline");
    desc.init();
    m_layout = desc.base.layout;

    auto vkDevice = m_device->get();
    auto gpCreateInfo = desc.vk.createInfo;
    auto vkPipelineLayout = static_cast<VkPipelineLayout*>(desc.base.layout.Get());
    gpCreateInfo.setLayout(vkPipelineLayout->get());

    // Configure conservative rasterization if enabled
    vk::PipelineRasterizationConservativeStateCreateInfoEXT pipelineRasterizationConservativeStateCreateInfo = {};
    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationCreateInfo = {};
    if (desc.base.rasterizerState.enableConservativeRasterization && device->supportsConservativeRasterization())
    {
        pipelineRasterizationCreateInfo = desc.vk.rasterizerState;

        pipelineRasterizationConservativeStateCreateInfo.conservativeRasterizationMode = vk::ConservativeRasterizationModeEXT::eOverestimate;
        pipelineRasterizationConservativeStateCreateInfo.extraPrimitiveOverestimationSize = 0;
        pipelineRasterizationCreateInfo.pNext = &pipelineRasterizationConservativeStateCreateInfo;

        gpCreateInfo.pRasterizationState = &pipelineRasterizationCreateInfo;
    }
    if (!m_device->supportsDynamicRendering())
    {
        VkRenderPassFallback::RenderPassDesc rpDesc;
        uint32_t numColorAttachments = uint32_t(desc1.base.numColorAttachments());
        rpDesc.colorAttachments.resize(numColorAttachments);
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            const auto& c0 = desc1.base.colorAttachmentFormats[j];
            auto& c1 = rpDesc.colorAttachments[j];
            c1.format = qhwi::vk(c0);
            c1.loadOp = vk::AttachmentLoadOp::eDontCare;
            c1.resolveMode = vk::ResolveModeFlagBits::eNone;
            c1.storeOp = vk::AttachmentStoreOp::eDontCare;
        }
        {
            const auto d0 = desc1.base.depthStencilFormat;
            auto& d1 = rpDesc.depthStencilAttachment;
            d1.format = qhwi::vk(d0);
            d1.loadOp = vk::AttachmentLoadOp::eDontCare;
            d1.resolveMode = vk::ResolveModeFlagBits::eNone;
            d1.storeOp = vk::AttachmentStoreOp::eNone;

            rpDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            rpDesc.stencilResolveMode = vk::ResolveModeFlagBits::eNone;
            rpDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        }
        rpDesc.sampleCount = qhwi::vk(desc1.base.multisampleState.sampleCount);
        rpDesc.customResolve = desc.base.customResolve;

        m_vkRenderPass = VkRenderPassFallback::createRenderPass(m_device, rpDesc);

        static int debugId = 0;
        char name[128];
        qre::PrintfSafe(name, "gpRenderPass_%d", debugId++);
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eRenderPass, (uint64_t)(VkRenderPass)m_vkRenderPass.get(), name));

        gpCreateInfo.renderPass = m_vkRenderPass.get();
    }

    auto result = vkDevice.createGraphicsPipelineUnique(desc.vk.enablePipelineCache ? m_device->getPipelineCache() : nullptr, gpCreateInfo);
    if (result.result != vk::Result::eSuccess)
    {
        QHWI_FATAL("create graphics pipeline failed, result: %s", vk::to_string(result.result).c_str());
    }
    if (desc.base.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::ePipeline, (uint64_t)(VkPipeline)result.value.get(), desc.base.name));
    }
    m_graphicsPipeline = std::move(result.value);
}

qhwi::VkBuffer::Description::Description(const Buffer::Description& desc)
    : base(desc)
{
    vk.createInfo = vk::BufferCreateInfo({}, vk::DeviceSize(base.size), qhwi::vkBufferUsageFlags(desc.usage, desc.heapType), vk::SharingMode::eExclusive, 0, nullptr);
    if (base.heapFlags & HeapFlag_Shared)
    {
        vk.externalMemoryBufferCreateInfo.handleTypes = s_ExternalMemoryHandleTypeFlags;
        vk.createInfo.setPNext(&vk.externalMemoryBufferCreateInfo);
    }
}

const char* decimalToBinaryString(uint32_t decimal)
{
    const int numDigits = 8;
    static char binary[numDigits + 1];

    for (int i = 0; i < numDigits; i++)
    {
        binary[i] = decimal & (1 << (numDigits - 1 - i)) ? '1' : '0';
    }
    binary[numDigits] = '\0';
    return binary;
}

void qhwi::VkDevice::logMemoryReport()
{
    QHWI_LOGI("Dedicated Video Memory: %8.2f MB", getDedicatedVideoMemory() / (1024.0 * 1024.0));
    QHWI_LOGI("Shared System Memory: %8.2f MB", getSharedSystemMemory() / (1024.0 * 1024.0));

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_vmaAllocator, budgets);

    uint32_t numHeaps = getPhysicalDevice().getMemoryProperties().memoryHeapCount;
    for (uint32_t heapIndex = 0; heapIndex < numHeaps; heapIndex++)
    {
        QHWI_LOGI("Heap %2d size %8.2f MB  flags %2d",
                  heapIndex,
                  getPhysicalDevice().getMemoryProperties().memoryHeaps[heapIndex].size / (1024.0 * 1024.0),
                  getPhysicalDevice().getMemoryProperties().memoryHeaps[heapIndex].flags);

        QHWI_LOGI("Heap %2d has %4u allocations taking %8.2f MB",
                  heapIndex,
                  budgets[heapIndex].statistics.allocationCount,
                  budgets[heapIndex].statistics.allocationBytes / (1024.0 * 1024.0));
        QHWI_LOGI("allocated out of %4u Vulkan device memory blocks taking %8.2f MB,\n",
                  budgets[heapIndex].statistics.blockCount,
                  budgets[heapIndex].statistics.blockBytes / (1024.0 * 1024.0));
        QHWI_LOGI("Vulkan reports total usage %8.2f MB with budget %.2f MB.\n\n", budgets[heapIndex].usage / (1024.0 * 1024.0), budgets[heapIndex].budget / (1024.0 * 1024.0));
    }

    uint32_t numTypes = getPhysicalDevice().getMemoryProperties().memoryTypeCount;
    for (uint32_t memoryType = 0; memoryType < numTypes; memoryType++)
    {
        uint32_t flags = (uint32_t)getPhysicalDevice().getMemoryProperties().memoryTypes[memoryType].propertyFlags;

        QHWI_LOGI("Memory Type %2d properties %4d (%s) heapIndex %d",
                  memoryType,
                  flags,
                  decimalToBinaryString(flags),
                  getPhysicalDevice().getMemoryProperties().memoryTypes[memoryType].heapIndex);
    }
}

void qhwi::VkDevice::saveMemoryAllocations(const char* fileName)
{
    if (fileName)
    {
        char* statsString = nullptr;
        vmaBuildStatsString(m_vmaAllocator, &statsString, true);
        std::ofstream OutStats{ fileName };
        OutStats << statsString;

        vmaFreeStatsString(m_vmaAllocator, statsString);
    }
}

qhwi::VkBuffer::VkBuffer(VkDevice* device, const Description& desc, const void* data)
    : Buffer(desc.base)
    , m_desc(desc)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkBuffer::VkBuffer", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    auto vkDevice = m_device->get();
    bool supportsDeviceAddress = m_device->getVkFeaturesInfo().supportsDeviceAddress();
    if (!supportsDeviceAddress)
    {
        m_desc.vk.createInfo.usage &= ~vk::BufferUsageFlagBits::eShaderDeviceAddress;
    }
    bool deviceAddressEnabled = bool(m_desc.vk.createInfo.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress);

    auto pd = m_device->getPhysicalDevice1();
    int scratchOffsetAlignment = pd.getAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
    int shaderBindingTableAlignment = pd.getRayTracingPipelinesPropertiesKHR().shaderGroupBaseAlignment;

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = (VkDeviceSize)m_desc.vk.createInfo.size;
    bufferInfo.usage = (VkBufferUsageFlags)m_desc.vk.createInfo.usage;
    bufferInfo.pNext = m_desc.vk.createInfo.pNext;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    auto heapType = m_desc.base.heapType;
    if (heapType == HeapType::eDownload)
    {
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    else if (heapType == HeapType::eUpload)
    {
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    }
    else
    {
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    if (m_desc.base.heapFlags & HeapFlag_Shared)
    {
        // Force dedicated allocation for shared resources for robust interop.
        allocInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        // use shared pool
        allocInfo.pool = m_device->getVmaSharedAllocationsPool();
    }

    m_VmaAllocation = {};

    bool tileMemory = VkBufferUsageFlags(m_desc.vk.createInfo.usage) & VK_BUFFER_USAGE_TILE_MEMORY_BIT_QCOM;

    if (tileMemory)
    {
        m_buffer = vkDevice.createBuffer(m_desc.vk.createInfo);

        m_tileMemReqs = {};
        m_tileMemReqs.sType = VK_STRUCTURE_TYPE_TILE_MEMORY_REQUIREMENTS_QCOM;

        vk::MemoryRequirements2 memReqs2 = {};
        memReqs2.pNext = &m_tileMemReqs;

        vk::BufferMemoryRequirementsInfo2 bufferMemReqs2 = {};
        bufferMemReqs2.buffer = m_buffer;

        vkDevice.getBufferMemoryRequirements2(&bufferMemReqs2, &memReqs2);
        QHWI_LOGI("Buffer tileMemReqs.size: %lld tileMemReqs.alignment: %lld", m_tileMemReqs.size, m_tileMemReqs.alignment);
        QHWI_ASSERT(m_tileMemReqs.size);
        m_tileMemoryOffset = m_device->allocateTileMemory(m_tileMemReqs.size, m_tileMemReqs.alignment);
        vkDevice.bindBufferMemory(m_buffer, m_device->tileMem(), m_tileMemoryOffset);
    }
    else
    {
        if (m_desc.vk.createInfo.usage & (vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR))
        {
            V(vmaCreateBufferWithAlignment(m_device->getVmaAllocator(), &bufferInfo, &allocInfo, scratchOffsetAlignment, &m_buffer, &m_VmaAllocation, nullptr));
        }
        else if (m_desc.vk.createInfo.usage & vk::BufferUsageFlagBits::eShaderBindingTableKHR)
        {
            V(vmaCreateBufferWithAlignment(m_device->getVmaAllocator(), &bufferInfo, &allocInfo, shaderBindingTableAlignment, &m_buffer, &m_VmaAllocation, nullptr));
        }
        else
        {
            V(vmaCreateBuffer(m_device->getVmaAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_VmaAllocation, nullptr));
        }

        QHWI_ASSERT(m_VmaAllocation);

        vmaSetAllocationName(m_device->getVmaAllocator(), m_VmaAllocation, m_desc.base.name);
    }

    QHWI_ASSERT(m_buffer);

    if (deviceAddressEnabled)
    {
        m_deviceAddress = vkDevice.getBufferAddressKHR(vk::BufferDeviceAddressInfo().setBuffer(m_buffer));
    }

    if (m_desc.base.heapFlags & HeapFlag_Shared)
    {
        m_sharedHandle = ::getSharedHandle(vkDevice, m_VmaAllocation->GetMemory());
    }
}

qhwi::VkBuffer::~VkBuffer()
{
    unmap();
    vmaDestroyBuffer(m_device->getVmaAllocator(), m_buffer, m_VmaAllocation);
}

void* qhwi::VkBuffer::map(uint64_t offset, uint64_t size)
{
    if (size == 0)
    {
        size = m_desc.base.size - offset;
    }
    return m_VmaAllocation->GetMappedData();
}

void qhwi::VkBuffer::unmap()
{
    m_mapAddr = nullptr;
}

uint64_t qhwi::VkBuffer::getDeviceAddress() const
{
    return m_device->getVkFeaturesInfo().supportsDeviceAddress() ? m_deviceAddress : vk::DeviceAddress();
}

qhwi::VkDescriptorSetLayout::Description::Description(const DescriptorSetLayout::Description& desc)
    : base(desc)
{
    vk.numBindings = uint32_t(base.bindings.size());
    vk.numBindingFlags = vk.numBindings;
    auto& bindings = base.bindings;
    std::sort(bindings.begin(), bindings.end());
    for (size_t j = 0; j < bindings.size(); j++)
    {
        const auto& bindingDesc = base.bindings[j];
        uint32_t binding = base.getBindingSetOffset(bindingDesc.type) + bindingDesc.shaderRegister;
        vk.bindings[j] = vk::DescriptorSetLayoutBinding(binding, qhwi::vk(bindingDesc.type), bindingDesc.count, qhwi::vkShaderStageFlags(bindingDesc.shaderStages));
        vk.bindingFlags[j] = qhwi::vkDescriptorBindingFlags(bindingDesc.flags);
    }
    vk.bindingFlagsCreateInfo = vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT(vk.numBindingFlags, vk.bindingFlags);
    vk.createInfo = vk::DescriptorSetLayoutCreateInfo(
        //TODO: expose an API to the user to create descriptor pools, set this flag conditionally
        vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
        vk.numBindings,
        vk.bindings,
        &vk.bindingFlagsCreateInfo);
}

qhwi::VkDescriptorSetLayout::VkDescriptorSetLayout(VkDevice* device, const DescriptorSetLayoutDesc& desc)
    : m_device(device)
    , m_desc(desc)
{
    QHWI_ASSERT(device);
    auto vkDevice = m_device->get();
    m_descriptorSetLayout = vkDevice.createDescriptorSetLayoutUnique(m_desc.vk.createInfo);

    if (m_desc.base.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eDescriptorSetLayout, (uint64_t)(::VkDescriptorSetLayout)m_descriptorSetLayout.get(), m_desc.base.name));
    }
}

bool qhwi::VkDescriptorSet::supportsDynamicRenderingLocalRead() const
{
    return m_device->supportsDynamicRenderingLocalRead();
}

qhwi::VkDescriptorSet::Description::Description(const DescriptorSet::Description& desc)
    : base(desc)
{
    QHWI_ASSERT(base.layout);
    vk.layout = static_cast<VkDescriptorSetLayout*>(base.layout.Get())->get();
    vk.allocateInfo = vk::DescriptorSetAllocateInfo({}, 1, &vk.layout);
    if (base.variableDescriptorCount)
    {
        vk.variableDescriptorCount = base.variableDescriptorCount;
        vk.variableDescriptorCountAllocateInfo = vk::DescriptorSetVariableDescriptorCountAllocateInfo(1, &vk.variableDescriptorCount);
    }

    DescriptorSetLayout::Description* layoutDesc = base.layout->desc();
    QHWI_ASSERT(layoutDesc);

    base.reorderBindings();
    size_t numBindings = base.numBindings();
    uint32_t variableDescriptorCount = base.variableDescriptorCount;

    if (variableDescriptorCount == 0)
    {
        for (size_t j = 0; j < numBindings; ++j)
        {
            const qre::RefCountPtr<DescriptorSetBinding>& bindingData = base.bindings[j];
            if (!bindingData)
            {
                continue;
            }

            VkDescriptorSetBinding& vkBindingData = vk.bindings[j];
            uint32_t count = bindingData->count();
            auto descriptorType = bindingData->type();
            if ((descriptorType == DescriptorType::eConstantBuffer) || (descriptorType == DescriptorType::eDynamicConstantBuffer) ||
                (descriptorType == DescriptorType::eRWStructuredBuffer) || (descriptorType == DescriptorType::eStructuredBuffer) ||
                (descriptorType == DescriptorType::eByteAddressBuffer) || (descriptorType == DescriptorType::eRWByteAddressBuffer))
            {
                vkBindingData.initAsBufferInfos(count);
                qhwi::toVkBufferInfos(bindingData->m_bufferInfos, vkBindingData.bufferInfos, count);
            }
            else if (descriptorType == DescriptorType::eTypedBuffer || descriptorType == DescriptorType::eRWTypedBuffer)
            {
                vkBindingData.initAsBufferViews(count);
                qhwi::toVkBufferViews(bindingData->m_bufferInfos, vkBindingData.bufferViews, count);
            }
            else if ((descriptorType == DescriptorType::eTexture) || (descriptorType == DescriptorType::eInputAttachment) || (descriptorType == DescriptorType::eRWTexture) ||
                     (descriptorType == DescriptorType::eSampleWeightImageQCOM))
            {
                vkBindingData.initAsImageInfos(count, true);
                vk::ImageLayout imageLayout;
                if (descriptorType == DescriptorType::eInputAttachment)
                {
                    imageLayout = vk::ImageLayout::eRenderingLocalReadKHR;
                }
                else
                {
                    imageLayout = (descriptorType == DescriptorType::eRWTexture) ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
                }
                qhwi::toVkImageInfos(bindingData->m_textureInfos, vkBindingData.imageInfos, vkBindingData.imageViews, imageLayout, count);
            }
            else if (descriptorType == DescriptorType::eAccelerationStructure)
            {
                vkBindingData.initAsAccelStructInfos(count);
                qhwi::toVkAccelStructInfos(bindingData->m_accelStructInfos, vkBindingData.accelStructInfos, count);
            }
            else if (descriptorType == DescriptorType::eSampler)
            {
                vkBindingData.initAsImageInfos(count);
                qhwi::toVkImageInfos(bindingData->m_samplerInfos, vkBindingData.imageInfos, count);
            }
            else
            {
                QHWI_ASSERT(false);
            }
        }
    }
}

qhwi::VkDescriptorSet::VkDescriptorSet(VkDevice* device, const DescriptorSet::Description& desc)
    : m_device(device)
    , m_desc(desc)
{
    QHWI_ASSERT(device);
    auto vkDevice = m_device->get();
    auto& allocateInfo = m_desc.vk.allocateInfo;
    allocateInfo.pNext = m_desc.base.variableDescriptorCount ? &m_desc.vk.variableDescriptorCountAllocateInfo : nullptr;

    qre::StaticVector<vk::UniqueDescriptorPool, MaxDescriptorHeaps>& descriptorPools = m_device->getDescriptorPools();
    size_t numDescriptorPools = descriptorPools.size();
    vk::Result result;
    uint32_t descriptorPoolIndex = 0;

    do
    {
        allocateInfo.descriptorPool = m_device->getDescriptorPool(descriptorPoolIndex);
        result = vkDevice.allocateDescriptorSets(&allocateInfo, &m_descriptorSet);

        if (result == vk::Result::eSuccess)
        {
            m_descriptorPool = allocateInfo.descriptorPool;
            break;
        }

        ++descriptorPoolIndex;
    }
    while (descriptorPoolIndex < numDescriptorPools);

    if (result == vk::Result::eErrorOutOfPoolMemory)
    {
        m_descriptorPool = m_device->createDescriptorPool();
        result = vkDevice.allocateDescriptorSets(&allocateInfo, &m_descriptorSet);
    }

    if (result != vk::Result::eSuccess)
    {
        QHWI_FATAL("Unable to create descriptor set");
    }

    if (m_desc.base.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eDescriptorSet, (uint64_t)(::VkDescriptorSet)m_descriptorSet, m_desc.base.name));
    }

    size_t numBindings = m_desc.base.numBindings();
    uint32_t variableDescriptorCount = m_desc.base.variableDescriptorCount;
    uint32_t numDescriptorSetWrites = 0;
    vk::WriteDescriptorSet descriptorSetWrites[MaxDescriptorSetLayoutBindings];
    auto vkLayoutDesc = static_cast<VkDescriptorSetLayout*>(m_desc.base.layout.Get())->vkDesc();
    QHWI_ASSERT(vkLayoutDesc);

    if (variableDescriptorCount == 0)
    {
        for (size_t j = 0; j < numBindings; j++)
        {
            auto& vkBindingData = m_desc.vk.bindings[j];
            uint32_t count = vkBindingData.count;
            if (count == 0)
            {
                continue;
            }
            auto& bindingData = m_desc.base.bindings[j];
            auto descriptorType = bindingData->type();
            auto& descSetWrite = descriptorSetWrites[numDescriptorSetWrites];
            descSetWrite = vk::WriteDescriptorSet(m_descriptorSet,
                                                  (vkLayoutDesc->base.getBindingSetOffset(descriptorType) + bindingData->shaderRegister()),
                                                  0,
                                                  count,
                                                  qhwi::vk(descriptorType),
                                                  nullptr,
                                                  nullptr,
                                                  nullptr);
            if ((descriptorType == DescriptorType::eConstantBuffer) || (descriptorType == DescriptorType::eDynamicConstantBuffer) ||
                (descriptorType == DescriptorType::eRWStructuredBuffer) || (descriptorType == DescriptorType::eStructuredBuffer) ||
                (descriptorType == DescriptorType::eByteAddressBuffer) || (descriptorType == DescriptorType::eRWByteAddressBuffer))
            {
                if (vkBindingData.bufferInfos->buffer)
                {
                    descSetWrite.pBufferInfo = vkBindingData.bufferInfos;
                }
                else
                {
                    continue;
                }
            }
            else if (descriptorType == DescriptorType::eTypedBuffer || descriptorType == DescriptorType::eRWTypedBuffer)
            {
                descSetWrite.pTexelBufferView = vkBindingData.bufferViews;
            }
            else if (descriptorType == DescriptorType::eTexture || descriptorType == DescriptorType::eInputAttachment || descriptorType == DescriptorType::eRWTexture ||
                     descriptorType == DescriptorType::eSampleWeightImageQCOM)
            {
                descSetWrite.pImageInfo = vkBindingData.imageInfos;
            }
            else if (descriptorType == DescriptorType::eAccelerationStructure)
            {
                descSetWrite.pNext = vkBindingData.accelStructInfos;
            }
            else if (descriptorType == DescriptorType::eSampler)
            {
                descSetWrite.pImageInfo = vkBindingData.imageInfos;
            }
            else
            {
                QHWI_ASSERT(false);
            }
            numDescriptorSetWrites++;
        }
    }
    if (numDescriptorSetWrites == 0)
    {
        return;
    }
    vkDevice.updateDescriptorSets(numDescriptorSetWrites, descriptorSetWrites, 0, nullptr);
}

qhwi::VkDescriptorSet::~VkDescriptorSet()
{
    auto vkDevice = m_device->get();
    V(vkDevice.freeDescriptorSets(m_descriptorPool, 1, &m_descriptorSet));
    for (auto& b : m_desc.vk.bindings)
    {
        b.onDestroy(vkDevice);
    }
}

qhwi::VkAccelerationStructure::Description::Description(const AccelerationStructure::Description& desc)
    : base(desc)
{
}

qhwi::VkAccelerationStructure::VkAccelerationStructure(VkDevice* device, const Description& desc)
    : m_desc(desc)
    , m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkAccelerationStructure::VkAccelerationStructure", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    Buffer::Description bufferDesc;
    bufferDesc.heapType = HeapType::eDefault;
    bufferDesc.usage = BufferUsage_AccelerationStructure, bufferDesc.size = desc.base.allocationSize, bufferDesc.name = "AccelerationStructure";

    VkBuffer::Description vkBufferDesc(bufferDesc);
    m_buffer = new VkBuffer(m_device, vkBufferDesc);
    QHWI_ASSERT(m_buffer);

    vk::AccelerationStructureCreateInfoKHR createInfo;
    createInfo.buffer = m_buffer->get();
    createInfo.type = desc.base.isTopLevel ? vk::AccelerationStructureTypeKHR::eTopLevel : vk::AccelerationStructureTypeKHR::eBottomLevel;
    createInfo.size = desc.base.allocationSize;

    m_accelerationStructure = m_device->get().createAccelerationStructureKHRUnique(createInfo, nullptr);

    if (desc.base.name)
    {
        auto vkDevice = device->get();
        setName(vkDevice,
                vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eAccelerationStructureKHR, (uint64_t)(VkAccelerationStructureKHR)m_accelerationStructure.get(), desc.base.name));
    }
}

qhwi::VkCommandList::Description::Description(const qhwi::CommandList::Description& desc)
    : base(desc)
{
    vk.level = qhwi::vk(base.type);
}

qhwi::VkCommandList::VkCommandList(VkDevice* device, const Description& desc)
    : m_device(device)
    , m_desc(desc)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::VkCommandList", QHWI_COLOR_RESOURCE_CREATE);

    QHWI_ASSERT(device);
    auto vkDevice = device->get();
    bool supportsOpticalFlow = desc.base.supportsOpticalFlow;
    auto commandBuffers = vkDevice.allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo(supportsOpticalFlow ? m_device->getOpticalFlowCommandPool() : m_device->getDefaultCommandPool(), m_desc.vk.level, 1));
    m_commandBuffer = std::move(commandBuffers[0]);
    QHWI_ASSERT(m_commandBuffer.get());
    if (m_desc.base.name)
    {
        setName(m_device->get(), vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eCommandBuffer, (uint64_t)(VkCommandBuffer)m_commandBuffer.get(), m_desc.base.name));
    }
}

qhwi::VkCommandList::~VkCommandList()
{
    m_device->uploadPool().reset(this);
}

void qhwi::VkCommandList::begin()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::begin", QHWI_COLOR_COMMAND_LIST);
    onBegin();
    if (m_device->tileMem())
    {
        VkTileMemoryBindInfoQCOM tileMemoryBindInfo;
        tileMemoryBindInfo.memory = m_device->tileMem();
        tileMemoryBindInfo.sType = VK_STRUCTURE_TYPE_TILE_MEMORY_BIND_INFO_QCOM;
        bindTileMemoryQCOM(&tileMemoryBindInfo);
    }
    m_commandBuffer->begin(vk::CommandBufferBeginInfo());
}

void qhwi::VkCommandList::end()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::end", QHWI_COLOR_COMMAND_LIST);
    m_commandBuffer->end();
}

void qhwi::VkCommandList::beginMarker(const char* name)
{
    QHWI_ASSERT(name);
    if (!m_device->debugUtilsEnabled())
    {
        return;
    }
    if (defer())
    {
        m_rp.beginMarker(name);
        return;
    }

    m_commandBuffer->beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT(name));
}

void qhwi::VkCommandList::endMarker()
{
    if (!m_device->debugUtilsEnabled())
    {
        return;
    }
    if (defer())
    {
        m_rp.endMarker();
        return;
    }

    m_commandBuffer->endDebugUtilsLabelEXT();
}

void qhwi::VkCommandList::dispatchTileQCOM(const VkDispatchTileInfoQCOM* pDispatchTileInfo)
{
    m_device->fn().cmdDispatchTileQCOM(m_commandBuffer.get(), pDispatchTileInfo);
}

void qhwi::VkCommandList::beginPerTileExecutionQCOM(const VkPerTileBeginInfoQCOM* pPerTileBeginInfo)
{
    m_device->fn().cmdBeginPerTileExecutionQCOM(m_commandBuffer.get(), pPerTileBeginInfo);
    this->notifyInComputePass(true);
}

void qhwi::VkCommandList::endPerTileExecutionQCOM(const VkPerTileEndInfoQCOM* pPerTileEndInfo)
{
    m_device->fn().cmdEndPerTileExecutionQCOM(m_commandBuffer.get(), pPerTileEndInfo);
    this->notifyInComputePass(false);
}

void qhwi::VkCommandList::bindTileMemoryQCOM(const VkTileMemoryBindInfoQCOM* pTileMemoryBindInfo)
{
    m_device->fn().cmdBindTileMemoryQCOM(m_commandBuffer.get(), pTileMemoryBindInfo);
}

void qhwi::VkCommandList::beginRenderPass(uint32_t numColorAttachments,
                                          const ColorAttachmentDescription* colorAttachmentDescs,
                                          const DepthStencilAttachmentDescription* depthStencilAttachmentDesc,
                                          const MultiViewState* multiViewState,
                                          const Rect2D* multiViewPerViewRenderAreasQCOM,
                                          const uint32_t* colorAttachmentRemapping,
                                          const TileShadingInfoQCOM* tileShadingInfoQCOM,
                                          bool customResolve)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::beginRenderPass", QHWI_COLOR_COMMAND_LIST);
    m_hasPrevSubpass = hasPrevSubpass({ colorAttachmentDescs, numColorAttachments });
    m_hasNextSubpass = hasNextSubpass({ colorAttachmentDescs, numColorAttachments });

    if (!m_hasPrevSubpass)
    {
        QHWI_ASSERT(m_deferState == DeferState::eOff);
    }
    if (numColorAttachments == 0)
    {
        colorAttachmentDescs = nullptr;
    }
    QHWI_ASSERT(colorAttachmentDescs != nullptr || (numColorAttachments == 0 && depthStencilAttachmentDesc != nullptr));

    if (!m_hasPrevSubpass)
    {
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            auto& colorAttachment = colorAttachmentDescs[j];
            auto colorTexture = colorAttachment.textureData.texture;
            auto resolveTexture = colorAttachment.resolveTarget.texture;

            markResourceAsUsed(colorTexture);
            if (colorTexture->automaticBarriersEnabled())
            {
                bool useLocalReadLayout = (colorTexture->desc()->usage & TextureUsage_InputAttachment) != 0;
                TextureState targetState = useLocalReadLayout ? TextureState_ColorAttachment_LocalRead : TextureState_ColorAttachment;
                notifyState(colorTexture, TextureState_ColorAttachment_Undefined, colorAttachment.textureData.getSubresourceRange());
                setState(colorTexture, targetState, colorAttachment.textureData.getSubresourceRange());
            }
            if (resolveTexture)
            {
                markResourceAsUsed(resolveTexture);
                if (resolveTexture->automaticBarriersEnabled())
                {
                    notifyState(resolveTexture, TextureState_ColorAttachment_Undefined, colorAttachment.resolveTarget.getSubresourceRange());
                    setState(resolveTexture, TextureState_ColorResolveDest, colorAttachment.resolveTarget.getSubresourceRange());
                }
            }
        }
        if (depthStencilAttachmentDesc)
        {
            auto depthTexture = depthStencilAttachmentDesc->textureData.texture;
            auto resolveTexture = depthStencilAttachmentDesc->resolveTarget.texture;

            markResourceAsUsed(depthTexture);
            if (depthTexture->automaticBarriersEnabled())
            {
                const bool useLocalReadLayout = (depthTexture->desc()->usage & TextureUsage_InputAttachment) != 0;
                const TextureState targetState = useLocalReadLayout ? TextureState_DepthStencilAttachment_LocalRead : TextureState_DepthStencilAttachment;
                setState(depthTexture, targetState, depthStencilAttachmentDesc->textureData.getSubresourceRange());
            }
            if (resolveTexture)
            {
                markResourceAsUsed(resolveTexture);
                if (resolveTexture->automaticBarriersEnabled())
                {
                    setState(resolveTexture, TextureState_DepthStencilResolveDest, depthStencilAttachmentDesc->textureData.getSubresourceRange());
                }
            }
        }
    }

    m_multiViewState = multiViewState ? *multiViewState : MultiViewState();

    if (m_desc.base.supportsAutomaticBarriers)
    {
        m_deferState = DeferState::eRecord;
        m_rp.beginRecording(this);
        m_rp.beginRenderPass(numColorAttachments,
                             colorAttachmentDescs,
                             depthStencilAttachmentDesc,
                             multiViewState,
                             multiViewPerViewRenderAreasQCOM,
                             colorAttachmentRemapping,
                             customResolve);
        return;
    }
    beginRenderPass1(numColorAttachments,
                     colorAttachmentDescs,
                     depthStencilAttachmentDesc,
                     multiViewState,
                     multiViewPerViewRenderAreasQCOM,
                     colorAttachmentRemapping,
                     tileShadingInfoQCOM,
                     customResolve);
}

void qhwi::VkCommandList::setRenderingAttachmentLocations(std::span<const uint32_t> colorAttachmentRemapping)
{
    if (!colorAttachmentRemapping.empty() && supportsDynamicRenderingLocalRead(m_device))
    {
        m_commandBuffer->setRenderingAttachmentLocationsKHR(vk::RenderingAttachmentLocationInfoKHR(colorAttachmentRemapping));
    }
}

void qhwi::VkCommandList::beginRenderPass1(uint32_t numColorAttachments,
                                           const ColorAttachmentDescription* colorAttachmentDescs,
                                           const DepthStencilAttachmentDescription* depthStencilAttachmentDesc,
                                           const MultiViewState* multiViewState,
                                           const Rect2D* multiViewPerViewRenderAreasQCOM,
                                           const uint32_t* colorAttachmentRemapping,
                                           const TileShadingInfoQCOM* tileShadingInfoQCOM,
                                           bool customResolve)
{
    customResolve = customResolve && checkCustomResolveEnabled(m_device, "Custom resolve render pass");
    if (customResolve)
    {
        bool hasColorResolveTarget = false;
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            hasColorResolveTarget |= (colorAttachmentDescs[j].resolveTarget.texture != nullptr);
        }
        QHWI_ASSERT_MSG(hasColorResolveTarget, "Custom resolve render passes require at least one color resolve target.");
        if (depthStencilAttachmentDesc)
        {
            QHWI_ASSERT_MSG(depthStencilAttachmentDesc->depthResolveMode == ResolveMode::eNone && depthStencilAttachmentDesc->stencilResolveMode == ResolveMode::eNone,
                            "Custom resolve render passes currently support color resolves only.");
        }
    }

    if (!m_device->supportsDynamicRendering())
    {
        VkRenderPassFallback::RenderPassDesc rpDesc;
        rpDesc.colorAttachments.resize(numColorAttachments);
        qre::StaticVector<vk::ClearValue, qhwi::MaxAttachments> clearValues;
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            const auto& c0 = colorAttachmentDescs[j];
            auto& c1 = rpDesc.colorAttachments[j];
            c1.format = qhwi::vk(c0.textureData.texture->desc()->format);
            c1.loadOp = qhwi::vk(c0.loadOp);
            bool hasResolveTarget = (c0.resolveTarget.texture != nullptr);
            c1.resolveMode = hasResolveTarget ? (customResolve ? vk::ResolveModeFlagBits::eCustomEXT : vk::ResolveModeFlagBits::eAverage) : vk::ResolveModeFlagBits::eNone;
            c1.storeOp = qhwi::vk(c0.storeOp);
            clearValues.push_back(vk::ClearColorValue(c0.clearValue.rgba));
        }
        {
            const auto d0 = depthStencilAttachmentDesc;
            auto& d1 = rpDesc.depthStencilAttachment;
            d1.format = d0 ? qhwi::vk(d0->textureData.texture->desc()->format) : vk::Format::eUndefined;
            d1.loadOp = d0 ? qhwi::vk(d0->depthLoadOp) : vk::AttachmentLoadOp::eDontCare;
            d1.resolveMode = d0 ? qhwi::vk(d0->depthResolveMode) : vk::ResolveModeFlagBits::eNone;
            d1.storeOp = d0 ? qhwi::vk(d0->depthStoreOp) : vk::AttachmentStoreOp::eNone;
            if (d0)
            {
                clearValues.push_back(vk::ClearDepthStencilValue(d0->clearValue.depth, d0->clearValue.stencil));
            }

            rpDesc.stencilLoadOp = d0 ? qhwi::vk(d0->stencilLoadOp) : vk::AttachmentLoadOp::eDontCare;
            rpDesc.stencilResolveMode = d0 ? qhwi::vk(d0->stencilResolveMode) : vk::ResolveModeFlagBits::eNone;
            rpDesc.stencilStoreOp = d0 ? qhwi::vk(d0->stencilStoreOp) : vk::AttachmentStoreOp::eDontCare;
        }
        rpDesc.sampleCount = (numColorAttachments > 0) ? qhwi::vk(colorAttachmentDescs[0].textureData.texture->desc()->numSamples) : vk::SampleCountFlagBits::e1;
        rpDesc.customResolve = customResolve;

        auto vkRenderPass = VkRenderPassFallback::createRenderPass(m_device, rpDesc);

        static int debugId = 0;
        char name[128];
        qre::PrintfSafe(name, "renderPass_%d", debugId++);
        setName(m_device->get(), vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eRenderPass, (uint64_t)(VkRenderPass)vkRenderPass.get(), name));

        VkRenderPassFallback::FramebufferDesc fbDesc;
        fbDesc.numColorAttachments = numColorAttachments;
        fbDesc.colorAttachments = colorAttachmentDescs;
        fbDesc.depthStencilAttachment = depthStencilAttachmentDesc;
        fbDesc.renderPass = vkRenderPass.get();
        auto vkFramebuffer = VkRenderPassFallback::createFramebuffer(m_device->get(), fbDesc);
        vk::Rect2D renderArea;
        if (numColorAttachments > 0)
        {
            const auto& p = colorAttachmentDescs[0].textureData.texture->desc();
            renderArea.setExtent({ p->w, p->h });
        }
        else if (depthStencilAttachmentDesc)
        {
            const auto& p = depthStencilAttachmentDesc->textureData.texture->desc();
            renderArea.setExtent({ p->w, p->h });
        }
        beginVkRenderPass(vk::RenderPassBeginInfo()
                              .setRenderPass(vkRenderPass.get())
                              .setFramebuffer(vkFramebuffer.get())
                              .setClearValueCount(uint32_t(clearValues.size()))
                              .setPClearValues(clearValues.data())
                              .setRenderArea(renderArea));
        m_vkRenderPasses.emplace(std::move(vkRenderPass));
        m_vkFramebuffers.emplace(std::move(vkFramebuffer));
        return;
    }
    if (multiViewPerViewRenderAreasQCOM && !m_device->getVkFeaturesInfo().supportsMultiViewPerViewRenderAreasQCOM())
    {
        // Ignore param if device doesn't support extension
        multiViewPerViewRenderAreasQCOM = nullptr;
    }

    if (tileShadingInfoQCOM && !m_device->getVkFeaturesInfo().supportsTileShadingQCOM())
    {
        // Ignore param if device doesn't support extension
        tileShadingInfoQCOM = nullptr;
    }

    vk::Extent3D extent = colorAttachmentDescs ? getExtent(colorAttachmentDescs[0]) : getExtent(*depthStencilAttachmentDesc);
    uint32_t numClearValues = 0;
    vk::ClearValue clearValues[MaxAttachments];

    for (uint32_t j = 0; j < numColorAttachments; j++)
    {
        QHWI_ASSERT(numClearValues < MaxAttachments);

        clearValues[numClearValues++].color = vk::ClearColorValue(colorAttachmentDescs[j].clearValue.rgba);
        if (colorAttachmentDescs[j].resolveTarget.texture != nullptr)
        {
            // clear values for resolve attachments do nothing, but keep the clearValues array the same
            // size as the attachment array
            clearValues[numClearValues++].color = vk::ClearColorValue(0, 0, 0, 0);
        }
    }

    if (depthStencilAttachmentDesc)
    {
        QHWI_ASSERT(numClearValues < MaxAttachments);

        const auto& v = depthStencilAttachmentDesc->clearValue;
        clearValues[numClearValues++].depthStencil = vk::ClearDepthStencilValue(v.depth, v.stencil);
        if (depthStencilAttachmentDesc->resolveTarget.texture != nullptr)
        {
            clearValues[numClearValues++].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);
        }
    }

    qre::StaticVector<vk::RenderingAttachmentInfoKHR, MaxColorAttachments> colorAttachments(numColorAttachments);
    for (uint32_t j = 0; j < numColorAttachments; j++)
    {
        const auto& colorAttachmentDesc = colorAttachmentDescs[j];
        auto& vkColorAttachment = colorAttachments[j];
        const auto& resolveTarget = colorAttachmentDesc.resolveTarget;
        bool hasResolveTarget = (resolveTarget.texture != nullptr);
        bool useLocalReadLayout = (colorAttachmentDesc.textureData.texture->desc()->usage & TextureUsage_InputAttachment) != 0;
        vk::ImageLayout attachmentLayout = useLocalReadLayout ? vk::ImageLayout::eRenderingLocalReadKHR : vk::ImageLayout::eColorAttachmentOptimal;
        vkColorAttachment = vk::RenderingAttachmentInfoKHR()
                                .setImageView(vkImageView(colorAttachmentDesc.textureData))
                                .setImageLayout(attachmentLayout)
                                .setLoadOp(vk(colorAttachmentDesc.loadOp))
                                .setStoreOp(vk(colorAttachmentDesc.storeOp))
                                .setClearValue(vk::ClearColorValue(colorAttachmentDescs[j].clearValue.rgba));
        if (hasResolveTarget)
        {
            vkColorAttachment.setResolveImageView(vkImageView(resolveTarget))
                .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setResolveMode(customResolve ? vk::ResolveModeFlagBitsKHR::eCustomEXT : vk::ResolveModeFlagBitsKHR::eAverage);
        }
    }
    auto depthAttachment = vk::RenderingAttachmentInfoKHR();
    auto stencilAttachment = vk::RenderingAttachmentInfoKHR();

    bool supportsStencil = false;
    if (depthStencilAttachmentDesc)
    {
        const auto& clearValue = depthStencilAttachmentDesc->clearValue;
        const auto imageView = vkImageView(depthStencilAttachmentDesc->textureData);
        const auto* vkTexture = static_cast<VkTexture*>(depthStencilAttachmentDesc->textureData.texture);
        supportsStencil = qhwi::supportsStencil(vkTexture->format());
        const auto& resolveTarget = depthStencilAttachmentDesc->resolveTarget;
        const bool hasResolveTarget = (resolveTarget.texture != nullptr);
        const bool useLocalReadLayout = (depthStencilAttachmentDesc->textureData.texture->desc()->usage & TextureUsage_InputAttachment) != 0;
        const vk::ImageLayout attachmentLayout = useLocalReadLayout ? vk::ImageLayout::eRenderingLocalReadKHR : vk::ImageLayout::eDepthStencilAttachmentOptimal;

        depthAttachment.setImageView(imageView)
            .setImageLayout(attachmentLayout)
            .setLoadOp(vk(depthStencilAttachmentDesc->depthLoadOp))
            .setStoreOp(vk(depthStencilAttachmentDesc->depthStoreOp))
            .setClearValue(vk::ClearDepthStencilValue(clearValue.depth, clearValue.stencil));

        if (hasResolveTarget)
        {
            depthAttachment.setResolveImageView(vkImageView(resolveTarget))
                .setResolveImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                .setResolveMode(vk(depthStencilAttachmentDesc->depthResolveMode));
        }

        if (supportsStencil)
        {
            stencilAttachment.setImageView(imageView)
                .setImageLayout(attachmentLayout)
                .setLoadOp(vk(depthStencilAttachmentDesc->stencilLoadOp))
                .setStoreOp(vk(depthStencilAttachmentDesc->stencilStoreOp))
                .setClearValue(vk::ClearDepthStencilValue(clearValue.depth, clearValue.stencil))
                .setResolveMode(vk(depthStencilAttachmentDesc->stencilResolveMode));
        }
    }
    uint32_t numViews = multiViewState ? multiViewState->numViews : 0;
    bool multiView = (numViews != 0);
    uint32_t viewMask = 0;
    uint32_t layerCount = 1;
    if (multiView)
    {
        viewMask = (1 << numViews) - 1;
    }

    auto rpBegin = vk::RenderingInfoKHR()
                       .setRenderArea(vk::Rect2D({ 0, 0 }, { extent.width, extent.height }))
                       .setViewMask(viewMask)
                       .setLayerCount(layerCount)
                       .setColorAttachmentCount(numColorAttachments)
                       .setPColorAttachments(colorAttachments.data())
                       .setPDepthAttachment(depthStencilAttachmentDesc ? &depthAttachment : nullptr)
                       .setPStencilAttachment(supportsStencil ? &stencilAttachment : nullptr);

    if (customResolve)
    {
        rpBegin.flags |= vk::RenderingFlagBits::eCustomResolveEXT;
    }

    vk::MultiviewPerViewRenderAreasRenderPassBeginInfoQCOM perViewRenderAreasRpBeginInfoQCOM;
    vk::Rect2D vkMultiViewPerViewRenderAreasQCOM[MaxViews];

    const void** ppNext = &rpBegin.pNext;

    if (multiViewPerViewRenderAreasQCOM)
    {
        for (uint32_t j = 0; j < numViews; j++)
        {
            vkMultiViewPerViewRenderAreasQCOM[j] = qhwi::vk(multiViewPerViewRenderAreasQCOM[j]);
        }
        perViewRenderAreasRpBeginInfoQCOM = vk::MultiviewPerViewRenderAreasRenderPassBeginInfoQCOM(numViews, vkMultiViewPerViewRenderAreasQCOM);

        *ppNext = &perViewRenderAreasRpBeginInfoQCOM;
        ppNext = &perViewRenderAreasRpBeginInfoQCOM.pNext;
    }

    VkRenderPassTileShadingCreateInfoQCOM vkRenderPassTileShadingInfoQCOM{};
    if (tileShadingInfoQCOM)
    {
        vkRenderPassTileShadingInfoQCOM.sType = VK_STRUCTURE_TYPE_RENDER_PASS_TILE_SHADING_CREATE_INFO_QCOM;
        if (tileShadingInfoQCOM->enableTileShading)
        {
            vkRenderPassTileShadingInfoQCOM.flags |= VK_TILE_SHADING_RENDER_PASS_ENABLE_BIT_QCOM;
        }
        m_inTileShadingPass = tileShadingInfoQCOM->enableTileShading;
        vkRenderPassTileShadingInfoQCOM.tileApronSize = { tileShadingInfoQCOM->tileApronSize.x, tileShadingInfoQCOM->tileApronSize.y };

        *ppNext = &vkRenderPassTileShadingInfoQCOM;
        ppNext = &vkRenderPassTileShadingInfoQCOM.pNext;
    }

    if (!m_hasPrevSubpass)
    {
        beginVkRendering(rpBegin);
    }
    if (colorAttachmentRemapping)
    {
        setRenderingAttachmentLocations({ colorAttachmentRemapping, numColorAttachments });
    }
}

void qhwi::VkCommandList::beginVkRendering(const vk::RenderingInfo& rInfo)
{
    onBeginRenderPass();
    beginProfilingPass();
    m_commandBuffer->beginRenderingKHR(rInfo);
}

void qhwi::VkCommandList::beginVkRenderPass(const vk::RenderPassBeginInfo& rpBeginInfo, vk::SubpassContents subpassContents)
{
    onBeginRenderPass();
    beginProfilingPass();
    m_commandBuffer->beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);
}

void qhwi::VkCommandList::endRenderPass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::endRenderPass", QHWI_COLOR_COMMAND_LIST);
    m_inTileShadingPass = false;
    if (!m_device->supportsDynamicRendering())
    {
        endVkRenderPass();
        return;
    }
    if (m_hasNextSubpass)
    {
        // Starting the next subpass instead of a new renderpass
        return;
    }
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.endRenderPass();
        m_inRenderPass = false;
        m_deferState = DeferState::ePlayback;
        m_rp.playback();
        m_rp.reset();
        m_deferState = DeferState::eOff;
        return;
    }
    endRenderPass1();
}

void qhwi::VkCommandList::endRenderPass1()
{
    endVkRendering();
}

void qhwi::VkCommandList::beginCustomResolve()
{
    QHWI_ASSERT_MSG(m_inRenderPass, "beginCustomResolve must be called inside a render pass.");
    if (m_desc.base.supportsAutomaticBarriers)
    {
        m_rp.beginCustomResolve();
        return;
    }
    beginCustomResolve1();
}

void qhwi::VkCommandList::beginCustomResolve1()
{
    if (m_device->desc()->enableCustomResolve && m_device->supportsCustomResolve())
    {
        const vk::BeginCustomResolveInfoEXT beginInfo = {};
        m_commandBuffer->beginCustomResolveEXT(beginInfo);
    }
}

void qhwi::VkCommandList::endVkRendering()
{
    onEndRenderPass();
    endProfilingPass();
    m_commandBuffer->endRenderingKHR();
}

void qhwi::VkCommandList::endVkRenderPass()
{
    onEndRenderPass();
    endProfilingPass();
    m_commandBuffer->endRenderPass();
}

void qhwi::VkCommandList::nextSubpass(vk::SubpassContents subpassContents)
{
    m_commandBuffer->nextSubpass(subpassContents);
}

void qhwi::VkCommandList::setRenderingAttachmentLocations(const vk::RenderingAttachmentLocationInfoKHR* pLocationInfo)
{
    m_commandBuffer->setRenderingAttachmentLocationsKHR(pLocationInfo);
}

void qhwi::VkCommandList::beginProfilingPass()
{
    if (m_scheduledTimerQuery)
    {
        m_commandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_device->getQueryHeap(), m_scheduledTimerQuery->beginQueryIndex);
    }
}

void qhwi::VkCommandList::endProfilingPass()
{
    if (m_scheduledTimerQuery)
    {
        m_commandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_device->getQueryHeap(), m_scheduledTimerQuery->endQueryIndex);
        m_scheduledTimerQuery = nullptr;
    }
}

void qhwi::VkCommandList::beginComputePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::beginComputePass", QHWI_COLOR_COMMAND_LIST);
    onBeginComputePass();
    beginProfilingPass();
}

void qhwi::VkCommandList::endComputePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::endComputePass", QHWI_COLOR_COMMAND_LIST);
    onEndComputePass();
    endProfilingPass();
}

void qhwi::VkCommandList::beginTransferPass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::beginTransferPass", QHWI_COLOR_COMMAND_LIST);
    onBeginTransferPass();
    beginProfilingPass();
}

void qhwi::VkCommandList::endTransferPass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::endTransferPass", QHWI_COLOR_COMMAND_LIST);
    onEndTransferPass();
    endProfilingPass();
}

void qhwi::VkCommandList::beginAccelerationStructurePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::beginAccelerationStructure", QHWI_COLOR_COMMAND_LIST);
    onBeginAccelerationStructurePass();

    beginProfilingPass();
}

void qhwi::VkCommandList::endAccelerationStructurePass()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::endAccelerationStructure", QHWI_COLOR_COMMAND_LIST);

    onEndAccelerationStructurePass();

    endProfilingPass();
}

void qhwi::VkCommandList::dispatch(uint32_t numThreadGroupsX,
                                   uint32_t numThreadGroupsY,
                                   uint32_t numThreadGroupsZ,
                                   uint32_t numThreadsPerThreadgroupX,
                                   uint32_t numThreadsPerThreadgroupY,
                                   uint32_t numThreadsPerThreadgroupZ)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::dispatch", QHWI_COLOR_COMPUTE);

    QHWI_ASSERT(m_inComputePass);
    CommandList::setComputeState();
    m_commandBuffer->dispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
}

void qhwi::VkCommandList::dispatchMesh(const DispatchMeshParams& p)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkCommandList::dispatchMesh", QHWI_COLOR_COMPUTE);
    QHWI_ASSERT(m_inRenderPass);
    const uint32_t* numThreadGroups = p.numAmplificationShaderThreadGroups.data();
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawMeshTasks(numThreadGroups[0], numThreadGroups[1], numThreadGroups[2]);
        return;
    }
    m_commandBuffer->drawMeshTasksEXT(numThreadGroups[0], numThreadGroups[1], numThreadGroups[2]);
}

void qhwi::VkCommandList::draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::draw", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.draw(vertexCount, instanceCount, firstVertex, firstInstance);
        return;
    }
    m_commandBuffer->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void qhwi::VkCommandList::drawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::drawIndexed", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        return;
    }
    m_commandBuffer->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void qhwi::VkCommandList::dispatchIndirect(uint32_t numThreadsPerThreadgroupX,
                                           uint32_t numThreadsPerThreadgroupY,
                                           uint32_t numThreadsPerThreadgroupZ,
                                           Buffer* buffer,
                                           size_t offset,
                                           uint32_t dispatchCount)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchIndirect", QHWI_COLOR_COMPUTE);
    QHWI_ASSERT(m_inComputePass);

    if (buffer->automaticBarriersEnabled())
    {
        m_nextComputeState.setIndirectBuffer(buffer);
    }
    CommandList::setComputeState();

    for (uint32_t i = 0; i < dispatchCount; ++i)
    {
        m_commandBuffer->dispatchIndirect(static_cast<VkBuffer*>(buffer)->get(), offset + (i * sizeof(VkDispatchIndirectCommand)));
    }

    markResourceAsUsed(buffer);
}

void qhwi::VkCommandList::drawIndirect(Buffer* buffer, size_t offset, uint32_t drawCount)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchIndirect", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    markResourceAsUsed(buffer);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawIndirect(static_cast<VkBuffer*>(buffer), offset, drawCount);
        return;
    }
    m_commandBuffer->drawIndirect(static_cast<VkBuffer*>(buffer)->get(), offset, drawCount, sizeof(VkDrawIndirectCommand));
}

void qhwi::VkCommandList::drawIndexedIndirect(Buffer* buffer, size_t offset, uint32_t drawCount)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::drawIndexedIndirect", QHWI_COLOR_GRAPHICS);
    QHWI_ASSERT(m_inRenderPass);
    markResourceAsUsed(buffer);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.drawIndexedIndirect(static_cast<VkBuffer*>(buffer), offset, drawCount);
        return;
    }
    m_commandBuffer->drawIndexedIndirect(static_cast<VkBuffer*>(buffer)->get(), offset, drawCount, sizeof(VkDrawIndexedIndirectCommand));
}

void qhwi::VkCommandList::dispatchRays(const StridedDeviceAddressRegion& raygenShaderBindingTable,
                                       const StridedDeviceAddressRegion& missShaderBindingTable,
                                       const StridedDeviceAddressRegion& hitShaderBindingTable,
                                       const StridedDeviceAddressRegion& callableShaderBindingTable,
                                       uint32_t width,
                                       uint32_t height,
                                       uint32_t depth)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchRays", QHWI_COLOR_RAYTRACING);
    QHWI_ASSERT(m_inComputePass);

    auto VkStridedDeviceAddressRegion = [](const StridedDeviceAddressRegion& region) { return vk::StridedDeviceAddressRegionKHR(region.startAddress, region.stride, region.size); };

    vk::StridedDeviceAddressRegionKHR raygenShaderTable = VkStridedDeviceAddressRegion(raygenShaderBindingTable);
    vk::StridedDeviceAddressRegionKHR missShaderTable = VkStridedDeviceAddressRegion(missShaderBindingTable);
    vk::StridedDeviceAddressRegionKHR hitShaderTable = VkStridedDeviceAddressRegion(hitShaderBindingTable);
    vk::StridedDeviceAddressRegionKHR callableShaderTable = VkStridedDeviceAddressRegion(callableShaderBindingTable);

    CommandList::setRayTracingState();
    m_commandBuffer->traceRaysKHR(&raygenShaderTable, &missShaderTable, &hitShaderTable, &callableShaderTable, width, height, depth);
}

void qhwi::VkCommandList::dispatchRaysIndirect(Buffer* buffer)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::dispatchRaysIndirect", QHWI_COLOR_RAYTRACING);

    QHWI_ASSERT_MSG(!m_device->getVkFeaturesInfo().supportsRayTraceMaintenance1Features(), "Feature %s is not supported\n", VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME);
    QHWI_ASSERT(buffer);
    markResourceAsUsed(buffer);
    vk::DeviceAddress indirectbufferAddress = static_cast<VkBuffer*>(buffer)->getDeviceAddress();
    m_commandBuffer->traceRaysIndirect2KHR(indirectbufferAddress);
}

void qhwi::VkCommandList::pushConstants(PipelineLayout* pipelineLayout, PipelineBindPoint pipelineBindPoint, const void* data, uint32_t index)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::pushConstants", QHWI_COLOR_COMMAND_LIST)

    QHWI_ASSERT(pipelineLayout);

    if (pipelineBindPoint == PipelineBindPoint::eGraphics)
    {
        QHWI_ASSERT(m_inRenderPass);
    }
    else
    {
        QHWI_ASSERT(m_inComputePass);
    }
    markResourceAsUsed(pipelineLayout);
    const auto& pushConstantRange = pipelineLayout->pushConstantRange(index);
    auto vkPipelineLayout = static_cast<VkPipelineLayout*>(pipelineLayout);

    if (defer())
    {
        m_rp.pushConstants(vkPipelineLayout->get(), qhwi::vkShaderStageFlags(pushConstantRange.shaderStages), pushConstantRange.offset, pushConstantRange.size, data);
        return;
    }

    m_commandBuffer->pushConstants(vkPipelineLayout->get(), qhwi::vkShaderStageFlags(pushConstantRange.shaderStages), pushConstantRange.offset, pushConstantRange.size, data);
}

void qhwi::VkCommandList::bindDescriptorSet(DescriptorSet* descriptorSet,
                                            PipelineBindPoint pipelineBindPoint,
                                            PipelineLayout* pipelineLayout,
                                            uint32_t numDynamicOffsets,
                                            const uint32_t* dynamicOffsets)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::bindDecriptorSet", QHWI_COLOR_PIPELINE);
    QHWI_ASSERT(descriptorSet);
    QHWI_ASSERT(pipelineLayout);

    m_currentPipelineLayout = pipelineLayout;
    markResourceAsUsed(descriptorSet);
    markResourceAsUsed(pipelineLayout);
    markDescSetResourcesAsUsed(descriptorSet);

    const qre::RefCountPtr<DescriptorSetLayout>& layout = descriptorSet->desc()->layout;
    uint32_t set = layout->desc()->registerSpace;
    vk::PipelineBindPoint vkPipelineBindPoint = qhwi::vk(pipelineBindPoint);
    vk::PipelineLayout vkPipelineLayout = static_cast<VkPipelineLayout*>(pipelineLayout)->get();
    auto vkDescriptorSet = static_cast<VkDescriptorSet*>(descriptorSet);
    auto vkDescriptorSet1 = vkDescriptorSet->get();

    if (defer())
    {
        m_rp.bindDescriptorSet(vkPipelineBindPoint, vkPipelineLayout, vkDescriptorSet, numDynamicOffsets, dynamicOffsets);
        return;
    }

    if (m_desc.base.supportsAutomaticBarriers)
    {
        if (pipelineBindPoint == PipelineBindPoint::eGraphics)
        {
            // note: bindDescriptorSet might be called before beginRenderPass
            m_nextGraphicsState.setDescSet(set, descriptorSet);
        }
        else if (pipelineBindPoint == PipelineBindPoint::eCompute)
        {
            m_nextComputeState.setDescSet(set, descriptorSet);
        }
        else if (pipelineBindPoint == PipelineBindPoint::eRayTracing)
        {
            m_nextRayTracingState.setDescSet(set, descriptorSet);
        }
    }
    m_commandBuffer->bindDescriptorSets(vkPipelineBindPoint, vkPipelineLayout, set, 1, &vkDescriptorSet1, numDynamicOffsets, dynamicOffsets);
}

void qhwi::VkCommandList::bindVertexBuffer(Buffer* buffer, uint32_t slot, size_t offset, size_t stride)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::bindVertexBuffer", QHWI_COLOR_PIPELINE);
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(buffer);
    markResourceAsUsed(buffer);

    auto vkBuffer = static_cast<VkBuffer*>(buffer);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.bindVertexBuffer(vkBuffer, slot, offset, stride);
        return;
    }
    vk::Buffer buffers[] = { vkBuffer->get() };
    VkDeviceSize offsets[] = { VkDeviceSize(offset) };
    m_commandBuffer->bindVertexBuffers(slot, buffers, offsets);
}

void qhwi::VkCommandList::bindIndexBuffer(Buffer* buffer, IndexFormat format, size_t offset)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(buffer);
    markResourceAsUsed(buffer);
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.bindIndexBuffer(static_cast<VkBuffer*>(buffer), vk(format), offset);
        return;
    }
    m_commandBuffer->bindIndexBuffer(static_cast<VkBuffer*>(buffer)->get(), offset, qhwi::vk(format));
}

void qhwi::VkCommandList::clear(Buffer* dst, uint32_t v)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }

    auto vkDstBuffer = static_cast<VkBuffer*>(dst);
    m_commandBuffer->fillBuffer(vkDstBuffer->get(), 0, vkDstBuffer->size(), v);

    markResourceAsUsed(dst);
}

// TODO: add subresource range as parameter.
void qhwi::VkCommandList::clear(Texture* dst, Color v)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);

    auto vkDstTexture = static_cast<VkTexture*>(dst);
    if (dst->desc()->usage & TextureUsage_DepthStencilAttachment)
    {
        QHWI_ERR("not supported");
        return;
    }

    vk::ImageSubresourceRange vkSubresourceRange = vkDstTexture->defaultImageViewCreateInfo().subresourceRange;

    m_commandBuffer->clearColorImage(vkDstTexture->getImage(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(v), vkSubresourceRange);

    markResourceAsUsed(dst);
}

void qhwi::VkCommandList::copyBuffer(Buffer* dst, Buffer* src, std::span<const CopyBufferRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    auto vkSrcBuffer = static_cast<VkBuffer*>(src);
    auto vkDstBuffer = static_cast<VkBuffer*>(dst);

    if (src->automaticBarriersEnabled())
    {
        setState(src, BufferState_Transfer_Src);
    }
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }
    std::vector<vk::BufferCopy> vkRegions(regions.size());
    auto vkRegionsIt = vkRegions.begin();
    for (const auto& r : regions)
    {
        size_t srcSize = r.srcSize;
        if (srcSize == 0)
        {
            srcSize = vkSrcBuffer->size();
        }
        *vkRegionsIt++ = vk::BufferCopy(r.srcOffset, r.dstOffset, srcSize);
    }
    m_commandBuffer->copyBuffer(vkSrcBuffer->get(), vkDstBuffer->get(), uint32_t(vkRegions.size()), vkRegions.data());

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::VkCommandList::copyBufferToTexture(Texture* dst, Buffer* src, std::span<const CopyBufferToTextureRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    auto vkSrcBuffer = static_cast<VkBuffer*>(src);
    auto vkDstTexture = static_cast<VkTexture*>(dst);
    auto dstDesc = vkDstTexture->desc();

    if (src->automaticBarriersEnabled())
    {
        setState(src, BufferState_Transfer_Src);
    }
    std::vector<vk::BufferImageCopy> vkRegions(regions.size());
    auto vkRegionsIt = vkRegions.begin();
    for (const auto& region : regions)
    {
        if (dst->automaticBarriersEnabled())
        {
            SubresourceRange subresourceRange = EntireResource;

            if (region.dstSubresource)
            {
                subresourceRange.setStart(*region.dstSubresource);
                subresourceRange.setEnd(*region.dstSubresource);
            }

            setState(dst, TextureState_Transfer_Dst, subresourceRange);
        }

        vk::ImageAspectFlags aspectFlags = vkDstTexture->getImageAspectFlags();
        vk::ImageSubresourceLayers dstSubresourceLayers = qhwi::vkImageSubresourceLayers(region.dstSubresource, aspectFlags);

        //TODO: handle bufferRowLength and bufferImageHeight
        const uvec3& dstOrigin = region.dstOrigin;
        Extent3D dstExtent = region.dstExtent;
        uint32_t dstMipLevel = dstSubresourceLayers.mipLevel;
        if (dstExtent.w == 0)
        {
            dstExtent.w = std::max(1u, dstDesc->w >> dstMipLevel);
        }
        if (dstExtent.h == 0)
        {
            dstExtent.h = std::max(1u, dstDesc->h >> dstMipLevel);
        }
        if (dstExtent.d == 0)
        {
            dstExtent.d = std::max(1u, dstDesc->d >> dstMipLevel);
        }

        *vkRegionsIt++ = vk::BufferImageCopy(region.srcOffset, 0, 0, dstSubresourceLayers, qhwi::vkOffset3D(dstOrigin), qhwi::vkExtent3D(dstExtent));
    }
    m_commandBuffer->copyBufferToImage(vkSrcBuffer->get(), vkDstTexture->getImage(), vk::ImageLayout::eTransferDstOptimal, uint32_t(vkRegions.size()), vkRegions.data());

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::VkCommandList::copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    VkTexture* vkSrcTexture = static_cast<VkTexture*>(src);
    VkTexture* vkDstTexture = static_cast<VkTexture*>(dst);
    const Texture::Description* srcDesc = vkSrcTexture->desc();

    if (src->automaticBarriersEnabled())
    {
        setState(src, TextureState_Transfer_Src);
    }
    std::vector<vk::ImageCopy> vkImageCopies(regions.size());
    for (size_t regionIndex = 0; regionIndex < regions.size(); regionIndex++)
    {
        const CopyTextureRegion& region = regions[regionIndex];

        if (dst->automaticBarriersEnabled())
        {
            SubresourceRange subresourceRange = EntireResource;

            if (region.dstSubresource)
            {
                subresourceRange.setStart(*region.dstSubresource);
                subresourceRange.setEnd(*region.dstSubresource);
            }

            setState(dst, TextureState_Transfer_Dst, subresourceRange);
        }

        vk::ImageSubresourceLayers srcSubresourceLayers = qhwi::vkImageSubresourceLayers(region.srcSubresource, vkSrcTexture->getImageAspectFlags());
        vk::ImageSubresourceLayers dstSubresourceLayers = qhwi::vkImageSubresourceLayers(region.dstSubresource, vkDstTexture->getImageAspectFlags());

        Extent3D srcExtent = region.srcExtent;
        uint32_t srcMipLevel = srcSubresourceLayers.mipLevel;
        if (srcExtent.w == 0)
        {
            srcExtent.w = std::max(1u, srcDesc->w >> srcMipLevel);
        }
        if (srcExtent.h == 0)
        {
            srcExtent.h = std::max(1u, srcDesc->h >> srcMipLevel);
        }
        if (srcExtent.d == 0)
        {
            srcExtent.d = std::max(1u, srcDesc->d >> srcMipLevel);
        }

        vk::ImageCopy& imageCopy = vkImageCopies[regionIndex];
        imageCopy.srcSubresource = srcSubresourceLayers;
        imageCopy.srcOffset = qhwi::vkOffset3D(region.srcOrigin);
        imageCopy.dstSubresource = dstSubresourceLayers;
        imageCopy.dstOffset = qhwi::vkOffset3D(region.dstOrigin);
        imageCopy.extent = qhwi::vkExtent3D(srcExtent);
    }

    m_commandBuffer->copyImage(vkSrcTexture->getImage(),
                               vk::ImageLayout::eTransferSrcOptimal,
                               vkDstTexture->getImage(),
                               vk::ImageLayout::eTransferDstOptimal,
                               uint32_t(vkImageCopies.size()),
                               vkImageCopies.data());

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::VkCommandList::copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion2> regions, Filter filter)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    VkTexture* vkSrcTexture = static_cast<VkTexture*>(src);
    VkTexture* vkDstTexture = static_cast<VkTexture*>(dst);
    const Texture::Description* srcDesc = vkSrcTexture->desc();
    const Texture::Description* dstDesc = vkDstTexture->desc();

    std::vector<vk::ImageBlit> vkImageBlits(regions.size());
    for (size_t j = 0; j < regions.size(); j++)
    {
        vk::ImageBlit& vkImageBlit = vkImageBlits[j];
        const CopyTextureRegion2& region = regions[j];
        vk::ImageSubresourceLayers srcSubresourceLayers = qhwi::vkImageSubresourceLayers(region.srcSubresource, vkSrcTexture->getImageAspectFlags());
        vk::ImageSubresourceLayers dstSubresourceLayers = qhwi::vkImageSubresourceLayers(region.dstSubresource, vkDstTexture->getImageAspectFlags());

        Extent3D srcExtent = region.srcExtent;
        uint32_t srcMipLevel = srcSubresourceLayers.mipLevel;
        if (srcExtent.w == 0)
        {
            srcExtent.w = std::max(1u, srcDesc->w >> srcMipLevel);
        }
        if (srcExtent.h == 0)
        {
            srcExtent.h = std::max(1u, srcDesc->h >> srcMipLevel);
        }
        if (srcExtent.d == 0)
        {
            srcExtent.d = std::max(1u, srcDesc->d >> srcMipLevel);
        }
        Extent3D dstExtent = region.dstExtent;
        uint32_t dstMipLevel = dstSubresourceLayers.mipLevel;
        if (dstExtent.w == 0)
        {
            dstExtent.w = std::max(1u, dstDesc->w >> dstMipLevel);
        }
        if (dstExtent.h == 0)
        {
            dstExtent.h = std::max(1u, dstDesc->h >> dstMipLevel);
        }
        if (dstExtent.d == 0)
        {
            dstExtent.d = std::max(1u, dstDesc->d >> dstMipLevel);
        }

        vkImageBlit.srcSubresource = srcSubresourceLayers;
        vkImageBlit.srcOffsets[0] = qhwi::vkOffset3D(region.srcOrigin);
        vkImageBlit.dstSubresource = dstSubresourceLayers;
        vkImageBlit.dstOffsets[0] = qhwi::vkOffset3D(region.dstOrigin);
        const auto& s0 = region.srcOrigin;
        const auto& s1 = srcExtent;
        const auto& d0 = region.dstOrigin;
        const auto& d1 = dstExtent;
        vkImageBlit.srcOffsets[1] = qhwi::vkOffset3D({ s0.x + s1.w, s0.y + s1.h, s0.z + s1.d });
        vkImageBlit.dstOffsets[1] = qhwi::vkOffset3D({ d0.x + d1.w, d0.y + d1.h, d0.z + d1.d });
    }
    m_commandBuffer->blitImage(vkSrcTexture->getImage(),
                               vk::ImageLayout::eTransferSrcOptimal,
                               vkDstTexture->getImage(),
                               vk::ImageLayout::eTransferDstOptimal,
                               uint32_t(vkImageBlits.size()),
                               vkImageBlits.data(),
                               qhwi::vk(filter));

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::VkCommandList::copyTextureToBuffer(Buffer* dst, Texture* src, std::span<const CopyTextureToBufferRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    QHWI_ASSERT(dst);
    QHWI_ASSERT(src);

    auto vkDstBuffer = static_cast<VkBuffer*>(dst);
    auto vkSrcTexture = static_cast<VkTexture*>(src);
    auto srcDesc = vkSrcTexture->desc();

    if (src->automaticBarriersEnabled())
    {
        setState(src, TextureState_Transfer_Src);
    }
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }

    std::vector<vk::BufferImageCopy> vkRegions(regions.size());
    auto vkRegionIt = vkRegions.begin();
    for (const auto& region : regions)
    {
        uvec3 srcOrigin = region.srcOrigin;
        Extent3D srcExtent = region.srcExtent;
        vk::ImageSubresourceLayers srcSubresourceLayers = qhwi::vkImageSubresourceLayers(region.srcSubresource, vkSrcTexture->getImageAspectFlags());
        uint32_t srcMipLevel = srcSubresourceLayers.mipLevel;
        if (srcExtent.w == 0)
        {
            srcExtent.w = std::max(1u, srcDesc->w >> srcMipLevel);
        }
        if (srcExtent.h == 0)
        {
            srcExtent.h = std::max(1u, srcDesc->h >> srcMipLevel);
        }
        if (srcExtent.d == 0)
        {
            srcExtent.d = std::max(1u, srcDesc->d >> srcMipLevel);
        }

        //TODO: handle bufferRowLength and bufferImageHeight
        *vkRegionIt++ = vk::BufferImageCopy(region.dstOffset, 0, 0, srcSubresourceLayers, qhwi::vkOffset3D(srcOrigin), qhwi::vkExtent3D(srcExtent));
    }

    m_commandBuffer->copyImageToBuffer(vkSrcTexture->getImage(), vk::ImageLayout::eTransferSrcOptimal, vkDstBuffer->get(), uint32_t(vkRegions.size()), vkRegions.data());

    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::VkCommandList::updateBuffer(Buffer* dst, std::span<const UpdateBufferRegion> regions)
{
    if (dst->automaticBarriersEnabled())
    {
        setState(dst, BufferState_Transfer_Dst);
    }
    CommandList::updateBuffer(dst, regions);
}

void qhwi::VkCommandList::updateTexture(Texture* dst, std::span<const UpdateTextureRegion> regions)
{
    QHWI_ASSERT(m_inTransferPass);
    uint64_t totalDeviceMemSize = 0;

    const FormatUtil::FormatInfo& formatInfo = FormatUtil::getFormatInfo(dst->desc()->format);

    size_t alignment = formatInfo.bytesPerBlock;

    for (const auto& r : regions)
    {
        QHWI_ASSERT(r.srcData && r.srcRowPitch && r.srcDepthPitch);

        if (dst->automaticBarriersEnabled())
        {
            setState(dst, TextureState_Transfer_Dst, SubresourceRange(Subresource().setArraySlice(uint16_t(r.arraySlice)).setMipSlice(uint16_t(r.mipLevel))));
        }

        auto s = TextureUtil::getSubresourceInfo(dst, r.mipLevel);
        s.deviceMemSize = qre::AlignUp(s.deviceMemSize, alignment);
        totalDeviceMemSize += s.deviceMemSize;
    }

    // Upload to staging buffer
    Buffer* src = nullptr;
    size_t srcOffset = 0;
    if (!device()->uploadPool().get(device(), this, totalDeviceMemSize, src, srcOffset, alignment))
    {
        QHWI_FATAL("no staging region found");
    }
    uint8_t* srcAddr = (uint8_t*)src->mapAddr();
    bool firstMapCall = (srcAddr == nullptr);
    if (firstMapCall)
    {
        srcAddr = (uint8_t*)src->map();
    }

    auto vkTexture = static_cast<qhwi::VkTexture*>(dst);

    std::vector<vk::BufferImageCopy> vkRegions(regions.size());
    auto vkRegionsIt = vkRegions.begin();
    for (const auto& r : regions)
    {
        const auto& s = TextureUtil::getSubresourceInfo(dst, r.mipLevel);

        size_t minRowPitch = std::min(size_t(s.deviceRowPitch), r.srcRowPitch);
        uint32_t depthSliceSize = s.deviceRowPitch * s.numBlocksY;
        const uint8_t* srcPtr = (const uint8_t*)r.srcData;
        uint8_t* dstPtr = srcAddr + srcOffset;

        if (s.deviceRowPitch == r.srcRowPitch) // Fast path, we can copy all the regions texture data in one copy
        {
            size_t size = r.srcRowPitch * (s.numBlocksY * s.mipDepth);
            memcpy(dstPtr, srcPtr, size);
        }
        else
        {
            for (uint32_t depthSlice = 0; depthSlice < s.mipDepth; ++depthSlice)
            {
                for (uint32_t row = 0; row < s.numBlocksY; ++row)
                {
                    memcpy(dstPtr, srcPtr, minRowPitch);
                    dstPtr += s.deviceRowPitch;
                    srcPtr += r.srcRowPitch;
                }
            }
        }

        vk::ImageAspectFlags aspectFlag = qhwi::vkImageAspectFlagsFromFormat(dst->desc()->format);
        *vkRegionsIt++ = vk::BufferImageCopy()
                             .setBufferOffset(srcOffset)
                             .setBufferRowLength(s.numBlocksX * formatInfo.blockWidth)
                             .setBufferImageHeight(s.numBlocksY * formatInfo.blockHeight)
                             .setImageSubresource(vk::ImageSubresourceLayers().setAspectMask(aspectFlag).setBaseArrayLayer(r.arraySlice).setLayerCount(1).setMipLevel(r.mipLevel))
                             .setImageExtent(vk::Extent3D().setWidth(s.mipWidth).setHeight(s.mipHeight).setDepth(s.mipDepth));

        srcOffset += (depthSliceSize * s.mipDepth);
    }
    m_commandBuffer->copyBufferToImage(static_cast<qhwi::VkBuffer*>(src)->get(),
                                       vkTexture->getImage(),
                                       vk::ImageLayout::eTransferDstOptimal,
                                       uint32_t(vkRegions.size()),
                                       vkRegions.data());
    markResourceAsUsed(dst);
    markResourceAsUsed(src);
}

void qhwi::VkCommandList::bindComputePipeline(ComputePipeline* pipeline)
{
    QHWI_ASSERT(m_inComputePass || m_inTileShadingPass);
    QHWI_ASSERT(pipeline);

    auto vkPipeline = static_cast<VkComputePipeline*>(pipeline);
    m_commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, vkPipeline->get());
    m_currentPipelineLayout = pipeline->getLayout();
    m_currentPipeline = pipeline;

    markResourceAsUsed(pipeline);
}

void qhwi::VkCommandList::bindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(pipeline);

    m_currentPipelineLayout = pipeline->getLayout();
    m_currentPipeline = pipeline;
    markResourceAsUsed(pipeline);

    auto vkPipeline = static_cast<VkGraphicsPipeline*>(pipeline)->get();
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.bindGraphicsPipeline(vkPipeline);
        return;
    }
    m_commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, vkPipeline);
}

void qhwi::VkCommandList::bindRayTracingPipeline(RayTracingPipeline* pipeline)
{
    QHWI_ASSERT(m_inComputePass);
    QHWI_ASSERT(pipeline);

    VkRayTracingPipeline* vkPipeline = static_cast<VkRayTracingPipeline*>(pipeline);
    m_commandBuffer->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, vkPipeline->get());

    m_currentPipelineLayout = pipeline->getLayout();
    m_currentPipeline = pipeline;

    markResourceAsUsed(pipeline);
}

void qhwi::VkCommandList::setViewports(uint32_t numViewports, const Viewport* viewports)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(viewports);
    vk::Viewport vp[MaxViewports];
    for (uint32_t j = 0; j < numViewports; j++)
    {
        vp[j] = vk(viewports[j], m_device->flipViewportY());
    }
    uint32_t numViews = m_multiViewState.numViews;
    if (numViews != 0 && m_device->getVkFeaturesInfo().supportsMultiViewPerViewViewportsQCOM() && numViewports < numViews)
    {
        // QCOM multiview extensions require the number of viewports to match the number of views
        auto v = vp[numViewports - 1];
        for (uint32_t j = numViewports; j < numViews; j++)
        {
            vp[j] = v;
        }
        numViewports = numViews;
    }
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.setViewports(numViewports, vp);
        return;
    }
    m_commandBuffer->setViewport(0, numViewports, vp);
}

void qhwi::VkCommandList::setScissors(uint32_t numScissors, const ScissorRect* scissorRects)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_ASSERT(scissorRects);
    vk::Rect2D sr[MaxScissors];
    for (uint32_t j = 0; j < numScissors; j++)
    {
        sr[j] = vk(scissorRects[j]);
    }
    uint32_t numViews = m_multiViewState.numViews;
    if (numViews != 0 && m_device->getVkFeaturesInfo().supportsMultiViewPerViewViewportsQCOM() && numScissors < numViews)
    {
        // QCOM multiview extensions require the number of scissor rects to match the number of views
        auto s = sr[numScissors - 1];
        for (uint32_t j = numScissors; j < numViews; j++)
        {
            sr[j] = s;
        }
        numScissors = numViews;
    }
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.setScissors(numScissors, sr);
        return;
    }
    m_commandBuffer->setScissor(0, numScissors, sr);
}

void qhwi::VkCommandList::setStencilReference(uint32_t reference)
{
    QHWI_ASSERT(m_inRenderPass);
    QHWI_TODO_MSG("Enable pipeline dynamic states");
    if (m_desc.base.supportsAutomaticBarriers)
    {
        QHWI_ASSERT(defer());
        m_rp.setStencilReference(reference);
        return;
    }
    m_commandBuffer->setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, reference);
}

void qhwi::VkCommandList::setState(Buffer* buffer, BufferState nextState)
{
    if (defer())
    {
        m_rp.setState(buffer, nextState);
        return;
    }
    CommandList::setState(buffer, nextState);
}

void qhwi::VkCommandList::setState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange)
{
    if (defer())
    {
        m_rp.setState(texture, nextState, subresourceRange);
        return;
    }
    CommandList::setState(texture, nextState, subresourceRange);
}

void qhwi::VkCommandList::addBarriers(uint32_t numBufferBarriers,
                                      const BufferBarrier* bufferBarriers,
                                      uint32_t numTextureBarriers,
                                      const TextureBarrier* textureBarriers,
                                      uint32_t numGlobalBarriers,
                                      const GlobalBarrier* globalBarriers,
                                      DependencyFlags dependencyFlags)
{
    bool hasLocalReadBarrier = qhwi::hasLocalReadBarrier(numTextureBarriers, textureBarriers);
    if (!hasLocalReadBarrier)
    {
        QHWI_ASSERT_MSG(m_inTileShadingPass || (!m_inRenderPass), "Cannot set a barrier inside a render pass");
    }

    if (numBufferBarriers == 0 && numTextureBarriers == 0 && numGlobalBarriers == 0)
    {
        return;
    }

    for (uint32_t j = 0; j < numBufferBarriers; j++)
    {
        const BufferBarrier& b = bufferBarriers[j];
#ifndef NDEBUG
        bool isBufferUsageTileMemoryHeap = b.buffer->usage() & qhwi::BufferUsage::BufferUsage_TileMemoryQCOM;
        bool isBufferStageTransfer = b.src == qhwi::BufferState_Transfer_Src || b.dst == qhwi::BufferState_Transfer_Dst;

        if (isBufferUsageTileMemoryHeap && isBufferStageTransfer)
        {
            QHWI_ASSERT_MSG(m_device->getVkPropertiesInfo().tileMemoryHeapQCOM.tileBufferTransfers, "Device doesn't support tile buffer transfers");
        }
#endif
        if (!b.buffer->automaticBarriersEnabled())
        {
            continue;
        }
        auto& s = bufferState(b.buffer);
        s.currentState = b.dst;
    }
    for (uint32_t j = 0; j < numTextureBarriers; j++)
    {
        const TextureBarrier& b = textureBarriers[j];
        if (!b.texture->automaticBarriersEnabled())
        {
            continue;
        }
        setCurrentState(b.texture, b.dst, b.subresourceRange);
    }

    QHWI_ASSERT(numBufferBarriers < MaxBarriers);
    QHWI_ASSERT(numTextureBarriers < MaxBarriers);
    QHWI_ASSERT(numGlobalBarriers < MaxBarriers);
    VkBufferMemoryBarrier2KHR bufferMemoryBarriers[MaxBarriers];
    auto bufferMemoryBarriersPtr = bufferMemoryBarriers;
    VkImageMemoryBarrier2KHR imageMemoryBarriers[MaxBarriers];
    auto imageMemoryBarriersPtr = imageMemoryBarriers;
    VkMemoryBarrier2KHR globalMemoryBarriers[MaxBarriers];
    auto globalMemoryBarriersPtr = globalMemoryBarriers;
    for (uint32_t j = 0; j < numBufferBarriers; j++)
    {
        const BufferBarrier& b = bufferBarriers[j];
        QHWI_ASSERT(b.buffer);
        VkBuffer* vkBuffer = static_cast<VkBuffer*>(b.buffer);
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
        QHWI_LOGI("barrier buffer: %p srcStages: %s srcAccess %s dstStages: %s dstAccess: %s",
                  b.buffer,
                  vk::to_string(qhwi::vkPipelineStageFlags2(b.src.stages)).c_str(),
                  vk::to_string(qhwi::vkAccessFlags2(b.src.access)).c_str(),
                  vk::to_string(qhwi::vkPipelineStageFlags2(b.dst.stages)).c_str(),
                  vk::to_string(qhwi::vkAccessFlags2(b.dst.access)).c_str());
#endif
        *bufferMemoryBarriersPtr++ = vk::BufferMemoryBarrier2KHR(qhwi::vkPipelineStageFlags2(b.src.stages),
                                                                 qhwi::vkAccessFlags2(b.src.access),
                                                                 qhwi::vkPipelineStageFlags2(b.dst.stages),
                                                                 qhwi::vkAccessFlags2(b.dst.access),
                                                                 0,
                                                                 0,
                                                                 vkBuffer->get(),
                                                                 0,
                                                                 vkBuffer->size());
        markResourceAsUsed(b.buffer);
    }
    for (uint32_t j = 0; j < numTextureBarriers; j++)
    {
        const TextureBarrier& b = textureBarriers[j];
        QHWI_ASSERT(b.texture);
        auto vkTexture = static_cast<VkTexture*>(b.texture);
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
        QHWI_LOGI("barrier texture: %p srcStages: %s srcAccess %s srcLayout: %s dstStages: %s dstAccess: %s dstLayout: %s subresourceRange: (%d,%d,%d) to (%d,%d,%d)",
                  vkTexture,
                  vk::to_string(qhwi::vkPipelineStageFlags2(b.src.stages)).c_str(),
                  vk::to_string(qhwi::vkAccessFlags2(b.src.access)).c_str(),
                  vk::to_string(qhwi::vkImageLayout(m_device, b.src.access, b.src.layout)).c_str(),
                  vk::to_string(qhwi::vkPipelineStageFlags2(b.dst.stages)).c_str(),
                  vk::to_string(qhwi::vkAccessFlags2(b.dst.access)).c_str(),
                  vk::to_string(qhwi::vkImageLayout(m_device, b.dst.access, b.dst.layout)).c_str(),
                  b.subresourceRange.start.mipSlice,
                  b.subresourceRange.start.arraySlice,
                  b.subresourceRange.start.planeSlice,
                  b.subresourceRange.end.mipSlice,
                  b.subresourceRange.end.arraySlice,
                  b.subresourceRange.end.planeSlice);
#endif

        vk::ImageSubresourceRange vkSubresourceRange = vkTexture->defaultImageViewCreateInfo().subresourceRange;
        const auto& subresourceRange = b.subresourceRange;
        if (!subresourceRange.isEntireResource())
        {
            vkSubresourceRange.baseArrayLayer = subresourceRange.start.arraySlice;
            vkSubresourceRange.layerCount = subresourceRange.end.arraySlice - subresourceRange.start.arraySlice + 1;
            vkSubresourceRange.baseMipLevel = subresourceRange.start.mipSlice;
            vkSubresourceRange.levelCount = subresourceRange.end.mipSlice - subresourceRange.start.mipSlice + 1;
        }
        *imageMemoryBarriersPtr++ = vk::ImageMemoryBarrier2KHR(qhwi::vkPipelineStageFlags2(b.src.stages),
                                                               qhwi::vkAccessFlags2(b.src.access),
                                                               qhwi::vkPipelineStageFlags2(b.dst.stages),
                                                               qhwi::vkAccessFlags2(b.dst.access),
                                                               qhwi::vkImageLayout(m_device, b.src.access, b.src.layout),
                                                               qhwi::vkImageLayout(m_device, b.dst.access, b.dst.layout),
                                                               0,
                                                               0,
                                                               vkTexture->getImage(),
                                                               vkSubresourceRange);
        markResourceAsUsed(b.texture);
    }
    for (uint32_t j = 0; j < numGlobalBarriers; j++)
    {
        const GlobalBarrier& b = globalBarriers[j];
#ifdef QHWI_ENABLE_DEBUG_BARRIERS
        QHWI_LOGI("barrier global: srcStages: %s srcAccess %s dstStages: %s dstAccess: %s",
                  vk::to_string(qhwi::vkPipelineStageFlags2(b.srcStages)).c_str(),
                  vk::to_string(qhwi::vkAccessFlags2(b.srcAccess)).c_str(),
                  vk::to_string(qhwi::vkPipelineStageFlags2(b.dstStages)).c_str(),
                  vk::to_string(qhwi::vkAccessFlags2(b.dstAccess)).c_str());
#endif
        *globalMemoryBarriersPtr++ = vk::MemoryBarrier2KHR(qhwi::vkPipelineStageFlags2(b.srcStages),
                                                           qhwi::vkAccessFlags2(b.srcAccess),
                                                           qhwi::vkPipelineStageFlags2(b.dstStages),
                                                           qhwi::vkAccessFlags2(b.dstAccess));
    }
    VkDependencyInfoKHR vkDepInfo{};
    vkDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDepInfo.bufferMemoryBarrierCount = numBufferBarriers;
    vkDepInfo.pBufferMemoryBarriers = bufferMemoryBarriers;
    vkDepInfo.imageMemoryBarrierCount = numTextureBarriers;
    vkDepInfo.pImageMemoryBarriers = imageMemoryBarriers;
    vkDepInfo.memoryBarrierCount = numGlobalBarriers;
    vkDepInfo.pMemoryBarriers = globalMemoryBarriers;
    if ((dependencyFlags & DependencyFlag_ByRegion) || hasLocalReadBarrier)
    {
        vkDepInfo.dependencyFlags |= VK_DEPENDENCY_BY_REGION_BIT;
    }
    addBarriers(vkDepInfo);
}

void qhwi::VkCommandList::addImageMemoryBarrier(const VkImageMemoryBarrier2KHR& b, VkDependencyFlags dependencyFlags)
{
    VkDependencyInfoKHR vkDepInfo{};
    vkDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDepInfo.imageMemoryBarrierCount = 1;
    vkDepInfo.pImageMemoryBarriers = &b;
    vkDepInfo.dependencyFlags = dependencyFlags;
    addBarriers(vkDepInfo);
}

void qhwi::VkCommandList::addBufferMemoryBarrier(const VkBufferMemoryBarrier2KHR& b, VkDependencyFlags dependencyFlags)
{
    VkDependencyInfoKHR vkDepInfo{};
    vkDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDepInfo.bufferMemoryBarrierCount = 1;
    vkDepInfo.pBufferMemoryBarriers = &b;
    vkDepInfo.dependencyFlags = dependencyFlags;
    addBarriers(vkDepInfo);
}

void qhwi::VkCommandList::addMemoryBarrier(const VkMemoryBarrier2KHR& b, VkDependencyFlags dependencyFlags)
{
    VkDependencyInfoKHR vkDepInfo{};
    vkDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDepInfo.memoryBarrierCount = 1;
    vkDepInfo.pMemoryBarriers = &b;
    vkDepInfo.dependencyFlags = dependencyFlags;
    addBarriers(vkDepInfo);
}

#define PIPELINE_STAGE_FLAGS_FROM_FLAGS2(s)                                                                                                                                        \
    if (flags2 & vk::PipelineStageFlagBits2KHR::e##s)                                                                                                                              \
    flags |= vk::PipelineStageFlagBits::e##s

static vk::PipelineStageFlags toPipelineStageFlags(const vk::PipelineStageFlags2KHR& flags2)
{
    if (flags2 == vk::PipelineStageFlagBits2KHR::eNone)
    {
        return vk::PipelineStageFlagBits::eNoneKHR;
    }

    vk::PipelineStageFlags flags;
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(TopOfPipe);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(DrawIndirect);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(VertexInput);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(VertexShader);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(TessellationControlShader);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(TessellationEvaluationShader);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(GeometryShader);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(FragmentShader);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(EarlyFragmentTests);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(LateFragmentTests);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(ColorAttachmentOutput);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(ComputeShader);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(Transfer);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(BottomOfPipe);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(Host);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(AllGraphics);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(AllCommands);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(TransformFeedbackEXT);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(ConditionalRenderingEXT);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(FragmentDensityProcessEXT);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(AccelerationStructureBuildKHR);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(RayTracingShaderKHR);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(FragmentShadingRateAttachmentKHR);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(TaskShaderEXT);
    PIPELINE_STAGE_FLAGS_FROM_FLAGS2(MeshShaderEXT);

    if (flags2 & (vk::PipelineStageFlagBits2KHR::eAllTransfer))
    {
        flags |= vk::PipelineStageFlagBits::eTransfer;
    }
    if (flags2 & (vk::PipelineStageFlagBits2KHR::eIndexInput | vk::PipelineStageFlagBits2KHR::eVertexAttributeInput))
    {
        flags |= vk::PipelineStageFlagBits::eVertexInput;
    }
    return flags;
}

#define ACCESS_FLAGS_FROM_FLAGS2(s)                                                                                                                                                \
    if (flags2 & vk::AccessFlagBits2KHR::e##s)                                                                                                                                     \
    flags |= vk::AccessFlagBits::e##s

static vk::AccessFlags toAccessFlags(const vk::AccessFlags2KHR& flags2)
{
    if (flags2 == vk::AccessFlagBits2KHR::eNone)
    {
        return vk::AccessFlagBits::eNoneKHR;
    }
    vk::AccessFlags flags;
    ACCESS_FLAGS_FROM_FLAGS2(IndirectCommandRead);
    ACCESS_FLAGS_FROM_FLAGS2(IndexRead);
    ACCESS_FLAGS_FROM_FLAGS2(VertexAttributeRead);
    ACCESS_FLAGS_FROM_FLAGS2(UniformRead);
    ACCESS_FLAGS_FROM_FLAGS2(InputAttachmentRead);
    ACCESS_FLAGS_FROM_FLAGS2(ShaderRead);
    ACCESS_FLAGS_FROM_FLAGS2(ShaderWrite);
    ACCESS_FLAGS_FROM_FLAGS2(ColorAttachmentRead);
    ACCESS_FLAGS_FROM_FLAGS2(ColorAttachmentWrite);
    ACCESS_FLAGS_FROM_FLAGS2(DepthStencilAttachmentRead);
    ACCESS_FLAGS_FROM_FLAGS2(DepthStencilAttachmentWrite);
    ACCESS_FLAGS_FROM_FLAGS2(TransferRead);
    ACCESS_FLAGS_FROM_FLAGS2(TransferWrite);
    ACCESS_FLAGS_FROM_FLAGS2(HostRead);
    ACCESS_FLAGS_FROM_FLAGS2(HostWrite);
    ACCESS_FLAGS_FROM_FLAGS2(MemoryRead);
    ACCESS_FLAGS_FROM_FLAGS2(MemoryWrite);
    ACCESS_FLAGS_FROM_FLAGS2(TransformFeedbackWriteEXT);
    ACCESS_FLAGS_FROM_FLAGS2(TransformFeedbackCounterReadEXT);
    ACCESS_FLAGS_FROM_FLAGS2(TransformFeedbackCounterWriteEXT);
    ACCESS_FLAGS_FROM_FLAGS2(ConditionalRenderingReadEXT);
    ACCESS_FLAGS_FROM_FLAGS2(FragmentShadingRateAttachmentReadKHR);
    ACCESS_FLAGS_FROM_FLAGS2(AccelerationStructureReadKHR);
    ACCESS_FLAGS_FROM_FLAGS2(AccelerationStructureWriteKHR);
    ACCESS_FLAGS_FROM_FLAGS2(FragmentDensityMapReadEXT);
    ACCESS_FLAGS_FROM_FLAGS2(ColorAttachmentReadNoncoherentEXT);
    if (flags2 & (vk::AccessFlagBits2KHR::eShaderSampledRead | vk::AccessFlagBits2KHR::eShaderStorageRead))
    {
        flags |= vk::AccessFlagBits::eShaderRead;
    }
    if (flags2 & vk::AccessFlagBits2KHR::eShaderStorageWrite)
    {
        flags |= vk::AccessFlagBits::eShaderWrite;
    }
    return flags;
}

static void toBufferMemoryBarrier(const vk::BufferMemoryBarrier2KHR& barrier2,
                                  vk::PipelineStageFlags& srcStages,
                                  vk::PipelineStageFlags& dstStages,
                                  vk::BufferMemoryBarrier& barrier)
{
    srcStages = toPipelineStageFlags(barrier2.srcStageMask);
    dstStages = toPipelineStageFlags(barrier2.dstStageMask);
    barrier = vk::BufferMemoryBarrier(toAccessFlags(barrier2.srcAccessMask),
                                      toAccessFlags(barrier2.dstAccessMask),
                                      barrier2.srcQueueFamilyIndex,
                                      barrier2.dstQueueFamilyIndex,
                                      barrier2.buffer,
                                      barrier2.offset,
                                      barrier2.size);
}

static void toImageMemoryBarrier(const vk::ImageMemoryBarrier2KHR& barrier2, vk::PipelineStageFlags& srcStages, vk::PipelineStageFlags& dstStages, vk::ImageMemoryBarrier& barrier)
{
    srcStages = toPipelineStageFlags(barrier2.srcStageMask);
    dstStages = toPipelineStageFlags(barrier2.dstStageMask);
    barrier = vk::ImageMemoryBarrier(toAccessFlags(barrier2.srcAccessMask),
                                     toAccessFlags(barrier2.dstAccessMask),
                                     barrier2.oldLayout,
                                     barrier2.newLayout,
                                     barrier2.srcQueueFamilyIndex,
                                     barrier2.dstQueueFamilyIndex,
                                     barrier2.image,
                                     barrier2.subresourceRange);
}

static void toMemoryBarrier(const vk::MemoryBarrier2KHR& barrier2, vk::PipelineStageFlags& srcStages, vk::PipelineStageFlags& dstStages, vk::MemoryBarrier& barrier)
{
    srcStages = toPipelineStageFlags(barrier2.srcStageMask);
    dstStages = toPipelineStageFlags(barrier2.dstStageMask);
    barrier = vk::MemoryBarrier(toAccessFlags(barrier2.srcAccessMask), toAccessFlags(barrier2.dstAccessMask));
}

void qhwi::VkCommandList::addBarriers(const vk::DependencyInfoKHR& b)
{
    if (m_device->supportsSynchronization2())
    {
        m_commandBuffer->pipelineBarrier2KHR(b);
    }
    else
    {
        for (uint32_t j = 0; j < b.bufferMemoryBarrierCount; j++)
        {
            vk::PipelineStageFlags srcStages, dstStages;
            vk::BufferMemoryBarrier bufferMemoryBarrier;
            toBufferMemoryBarrier(b.pBufferMemoryBarriers[j], srcStages, dstStages, bufferMemoryBarrier);
            m_commandBuffer->pipelineBarrier(srcStages, dstStages, b.dependencyFlags, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
        }
        for (uint32_t j = 0; j < b.imageMemoryBarrierCount; j++)
        {
            vk::PipelineStageFlags srcStages, dstStages;
            vk::ImageMemoryBarrier imageMemoryBarrier;
            toImageMemoryBarrier(b.pImageMemoryBarriers[j], srcStages, dstStages, imageMemoryBarrier);
            m_commandBuffer->pipelineBarrier(srcStages, dstStages, b.dependencyFlags, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        for (uint32_t j = 0; j < b.memoryBarrierCount; j++)
        {
            vk::PipelineStageFlags srcStages, dstStages;
            vk::MemoryBarrier memoryBarrier;
            toMemoryBarrier(b.pMemoryBarriers[j], srcStages, dstStages, memoryBarrier);
            m_commandBuffer->pipelineBarrier(srcStages, dstStages, b.dependencyFlags, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
        }
    }
}

qhwi::VkCommandQueue::VkCommandQueue(VkDevice* device, const Description& desc)
    : m_desc(desc)
    , m_device(device)
{
    QHWI_ASSERT(device);
    auto vkDevice = m_device->get();
    auto& queueFamilyIndex = m_desc.vk.queueFamilyIndex;
    queueFamilyIndex = m_device->findQueueFamilyIndex(vk(m_desc.base.capabilities));
    QHWI_ASSERT(queueFamilyIndex != -1);
    m_queue = vkDevice.getQueue(queueFamilyIndex, desc.vk.queueIndex);
    vk::SemaphoreTypeCreateInfo semaphoreTypeCreateInfo(vk::SemaphoreType::eTimeline);
    m_submissionIDSemaphore = vkDevice.createSemaphoreUnique(vk::SemaphoreCreateInfo({}, &semaphoreTypeCreateInfo));
    setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eSemaphore, (uint64_t)(VkSemaphore)m_submissionIDSemaphore.get(), "SubmissionIDSemaphore"));

#ifdef QHWI_TRACY_GPU_BACKEND_VULKAN
    vk::CommandBuffer setupCb = tracySetupCommandBuffer();
    // Use Calibrated tracy context if available, else fallback to regular version.
    if (m_device->supportsCalibratedTimestamps())
    {
        m_tracyCtx =
            TracyVkContextCalibrated(m_device->getInstance(), m_device->getPhysicalDevice(), m_device->get(), m_queue, setupCb, vkGetInstanceProcAddr, vkGetDeviceProcAddr);
    }
    else
    {
        m_tracyCtx = TracyVkContext(m_device->getInstance(), m_device->getPhysicalDevice(), m_device->get(), m_queue, setupCb, vkGetInstanceProcAddr, vkGetDeviceProcAddr);
    }

    if (m_tracyCtx)
    {
        qre::OsLogMessage(qre::LogChannel_Graphics, qre::LogSeverity_Info, "Vulkan Tracy GPU context created");
    }
    else
    {
        qre::OsLogMessage(qre::LogChannel_Graphics, qre::LogSeverity_Error, "Failed to create Vulkan Tracy GPU context");
    }

#endif
}

qhwi::VkCommandQueue::~VkCommandQueue()
{
#ifdef QHWI_TRACY_GPU_BACKEND_VULKAN
    if (m_tracyCtx)
    {
        TracyVkDestroy(m_tracyCtx);
    }
#endif
    waitForIdle();
}

// Tracy GPU profiling helper functions
#ifdef QHWI_TRACY_GPU_BACKEND_VULKAN
vk::CommandBuffer qhwi::VkCommandQueue::tracySetupCommandBuffer()
{
    // Temporary command pool for setup
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    poolInfo.queueFamilyIndex = m_desc.vk.queueFamilyIndex;
    m_tracySetupPool = m_device->get().createCommandPoolUnique(poolInfo);

    // Allocate command buffer from temp pool
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = m_tracySetupPool.get();
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto cmdBuffers = m_device->get().allocateCommandBuffersUnique(allocInfo);
    m_tracySetupCommandBuffer = std::move(cmdBuffers[0]);
    return m_tracySetupCommandBuffer.get();
}
#endif

uint64_t qhwi::VkCommandQueue::submit(uint32_t numCommandLists, CommandList** commandLists, Fence* fence, ext::Swapchain* swapchain)
{
    if (numCommandLists != 0)
    {
        QHWI_ASSERT(commandLists);
    }

    ext::VkSwapchain* vkSwapchain = swapchain ? static_cast<ext::VkSwapchain*>(swapchain) : nullptr;
    uint32_t waitSemaphoreCount = 0;
    constexpr uint32_t MaxWaitSemaphores = 2;
    vk::SemaphoreSubmitInfoKHR waitSemaphores[MaxWaitSemaphores];
    uint32_t signalSemaphoreCount = 0;
    constexpr uint32_t MaxSignalSemaphores = 3;
    vk::SemaphoreSubmitInfoKHR signalSemaphores[MaxSignalSemaphores];
    if (vkSwapchain)
    {
        waitSemaphores[waitSemaphoreCount++] =
            vk::SemaphoreSubmitInfoKHR().setSemaphore(vkSwapchain->getImageAcquiredSemaphore()).setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        signalSemaphores[signalSemaphoreCount++] = vk::SemaphoreSubmitInfoKHR()
                                                       .setSemaphore(vkSwapchain->getRenderCompleteSemaphore(swapchain->getCurrentImageIndex()))
                                                       .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
        vkSwapchain->setFrameRendered(true);
    }
    if (m_extraWaitSemaphoreSubmitInfo.semaphore)
    {
        waitSemaphores[waitSemaphoreCount++] = m_extraWaitSemaphoreSubmitInfo;
        m_extraWaitSemaphoreSubmitInfo = vk::SemaphoreSubmitInfoKHR();
    }
    if (m_extraSignalSemaphoreSubmitInfo.semaphore)
    {
        signalSemaphores[signalSemaphoreCount++] = m_extraSignalSemaphoreSubmitInfo;
        m_extraSignalSemaphoreSubmitInfo = vk::SemaphoreSubmitInfoKHR();
    }
    m_lastSubmittedID++;
    signalSemaphores[signalSemaphoreCount++] = vk::SemaphoreSubmitInfoKHR().setSemaphore(m_submissionIDSemaphore.get()).setValue(m_lastSubmittedID);

    vk::Fence vkFence = fence ? static_cast<VkFence*>(fence)->get() : VK_NULL_HANDLE;

    vk::CommandBufferSubmitInfoKHR vkCommandBuffers[MaxCommandLists];
    for (uint32_t j = 0; j < numCommandLists; j++)
    {
        CommandList* cmdList = commandLists[j];
        vk::CommandBuffer vkCommandBuffer = static_cast<VkCommandList*>(cmdList)->get();
        vkCommandBuffers[j] = vk::CommandBufferSubmitInfoKHR().setCommandBuffer(vkCommandBuffer);
    }

    vk::SubmitInfo2KHR submitInfo = vk::SubmitInfo2KHR().setCommandBufferInfoCount(numCommandLists).setPCommandBufferInfos(vkCommandBuffers);
    if (waitSemaphoreCount != 0)
    {
        submitInfo.setWaitSemaphoreInfoCount(waitSemaphoreCount).setPWaitSemaphoreInfos(waitSemaphores);
    }
    submitInfo.setSignalSemaphoreInfoCount(signalSemaphoreCount).setPSignalSemaphoreInfos(signalSemaphores);

    m_queue.submit2KHR(submitInfo, vkFence);

    if (numCommandLists != 0)
    {
        trackCommandLists((uint32_t)numCommandLists, commandLists, m_lastSubmittedID);
    }
    return m_lastSubmittedID;
}

void qhwi::VkCommandQueue::updateLastCompletedID()
{
    if (m_lastCompletedID < m_lastSubmittedID)
    {
        V(m_device->get().getSemaphoreCounterValueKHR(m_submissionIDSemaphore.get(), &m_lastCompletedID));
    }
}

void qhwi::VkCommandQueue::setPerfHintQCOM(VkPerfHintTypeQCOM type, uint32_t scale)
{
    QHWI_ASSERT(m_device->fn().queueSetPerfHintQCOM != nullptr);
    VkPerfHintInfoQCOM info{ .sType = VK_STRUCTURE_TYPE_PERF_HINT_INFO_QCOM, .type = type, .scale = scale };
    QHWI_ASSERT(m_device->fn().queueSetPerfHintQCOM(m_queue, &info) == VK_SUCCESS);
}

void qhwi::VkCommandList::buildBottomLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                                                Buffer* scratchBuffer,
                                                                BottomLevelDescriptor* bottomLevelDescriptors,
                                                                size_t descriptorCount,
                                                                void* scratchMemory,
                                                                AccelerationStructureBuildMode buildMode,
                                                                AccelerationStructureBuildFlags flags)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::buildBottomLevelAccelerationStructure", QHWI_COLOR_RAYTRACING);

    QHWI_ASSERT(m_inAccelerationStructurePass);
    QHWI_ASSERT(accelerationStructure);
    QHWI_ASSERT(scratchBuffer);
    QHWI_ASSERT(bottomLevelDescriptors);
    QHWI_ASSERT(scratchMemory);
    QHWI_ASSERT(descriptorCount);
    if (descriptorCount == 0)
    {
        return;
    }

    if (scratchBuffer->automaticBarriersEnabled())
    {
        setState(scratchBuffer, BufferState_ComputeShader_UnorderedAccess);
    }

    vk::AccelerationStructureGeometryKHR* descriptors = reinterpret_cast<vk::AccelerationStructureGeometryKHR*>(scratchMemory);
    vk::AccelerationStructureBuildRangeInfoKHR* buildRanges = reinterpret_cast<vk::AccelerationStructureBuildRangeInfoKHR*>(descriptors + descriptorCount);

    for (size_t i = 0; i < descriptorCount; i++)
    {
        vk::AccelerationStructureGeometryKHR& vkDescriptor(descriptors[i]);
        const BottomLevelDescriptor& inputDescriptor(bottomLevelDescriptors[i]);
        VkBuffer* vkVertexBuffer = static_cast<VkBuffer*>(inputDescriptor.triangles.vertexBuffer);

        vkDescriptor = vk::AccelerationStructureGeometryKHR();
        vkDescriptor.geometryType = vk::GeometryTypeKHR::eTriangles;
        vkDescriptor.flags = vk::GeometryFlagBitsKHR::eOpaque;
        vkDescriptor.geometry.triangles.transformData = vk::DeviceOrHostAddressConstKHR();
        vkDescriptor.geometry.triangles.maxVertex = (inputDescriptor.triangles.vertexCount == 0) ? 0 : inputDescriptor.triangles.vertexCount - 1;
        vkDescriptor.geometry.triangles.vertexFormat = vk(inputDescriptor.triangles.vertexFormat);
        vkDescriptor.geometry.triangles.vertexData = vkVertexBuffer->getDeviceAddress() + inputDescriptor.triangles.vertexOffset;
        vkDescriptor.geometry.triangles.vertexStride = inputDescriptor.triangles.vertexStride;

        buildRanges[i] = vk::AccelerationStructureBuildRangeInfoKHR();

        if (inputDescriptor.triangles.indexBuffer)
        {
            vkDescriptor.geometry.triangles.indexType = vk(inputDescriptor.triangles.indexFormat);
            vkDescriptor.geometry.triangles.indexData = static_cast<VkBuffer*>(inputDescriptor.triangles.indexBuffer)->getDeviceAddress() + inputDescriptor.triangles.indexOffset;

            buildRanges[i].primitiveCount = inputDescriptor.triangles.indexCount / 3;
        }
        else
        {
            buildRanges[i].primitiveCount = inputDescriptor.triangles.vertexCount / 3;
        }
    }

    vk::AccelerationStructureBuildGeometryInfoKHR buildInput;
    buildInput.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    buildInput.mode = qhwi::vk(buildMode);
    buildInput.flags = qhwi::vkBuildAccelerationStructureFlags(flags);
    buildInput.pGeometries = descriptors;
    buildInput.geometryCount = uint32_t(descriptorCount);
    buildInput.scratchData = static_cast<VkBuffer*>(scratchBuffer)->getDeviceAddress();
    buildInput.dstAccelerationStructure = *static_cast<VkAccelerationStructure*>(accelerationStructure)->getAccelerationStructure();

    vk::AccelerationStructureBuildGeometryInfoKHR buildInputs[] = { buildInput };

    m_commandBuffer->buildAccelerationStructuresKHR(buildInputs, buildRanges);

    markResourceAsUsed(accelerationStructure);
    markResourceAsUsed(scratchBuffer);
}

void qhwi::VkCommandList::buildTopLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                                             Buffer* instanceBuffer,
                                                             Buffer* scratchBuffer,
                                                             size_t instanceCount,
                                                             AccelerationStructure** blasArray,
                                                             size_t blasCount,
                                                             AccelerationStructureBuildMode buildMode,
                                                             AccelerationStructureBuildFlags flags)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::buildTopLevelAccelerationStructure", QHWI_COLOR_RAYTRACING);

    QHWI_ASSERT(m_inAccelerationStructurePass);
    QHWI_ASSERT(accelerationStructure);
    QHWI_ASSERT(instanceBuffer);
    QHWI_ASSERT(scratchBuffer);
    QHWI_ASSERT(instanceCount);

    if (instanceCount == 0)
    {
        return;
    }

    if (scratchBuffer->automaticBarriersEnabled())
    {
        setState(scratchBuffer, BufferState_ComputeShader_UnorderedAccess);
    }

    VkBuffer* vkInstanceBuffer = static_cast<VkBuffer*>(instanceBuffer);

    vk::AccelerationStructureGeometryKHR descriptor;
    descriptor.geometryType = vk::GeometryTypeKHR::eInstances;
    descriptor.geometry.instances = vk::AccelerationStructureGeometryInstancesDataKHR();
    descriptor.geometry.instances.arrayOfPointers = false;
    descriptor.geometry.instances.data = vkInstanceBuffer->getDeviceAddress();

    vk::AccelerationStructureBuildGeometryInfoKHR buildInput;
    buildInput.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    buildInput.mode = qhwi::vk(buildMode);
    buildInput.flags = qhwi::vkBuildAccelerationStructureFlags(flags);
    buildInput.pGeometries = &descriptor;
    buildInput.geometryCount = 1;
    buildInput.scratchData = static_cast<VkBuffer*>(scratchBuffer)->getDeviceAddress();
    buildInput.dstAccelerationStructure = *static_cast<VkAccelerationStructure*>(accelerationStructure)->getAccelerationStructure();

    vk::AccelerationStructureBuildGeometryInfoKHR buildInputs[] = { buildInput };
    vk::AccelerationStructureBuildRangeInfoKHR buildRanges[] = { vk::AccelerationStructureBuildRangeInfoKHR().setPrimitiveCount(uint32_t(instanceCount)) };

    m_commandBuffer->buildAccelerationStructuresKHR(buildInputs, buildRanges);

    markResourceAsUsed(accelerationStructure);
    markResourceAsUsed(instanceBuffer);
    markResourceAsUsed(scratchBuffer);
}

void qhwi::VkCommandList::beginTimerQuery(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(timerQuery->timerState == TimerState::eReset, "Cannot start a timer that has not been reset");

    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot set a barrier inside a render pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot set a barrier inside a transfer pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot set a barrier inside a compute pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot set a barrier inside an acceleration structure pass");

    m_scheduledTimerQuery = timerQuery;

    timerQuery->timerState = TimerState::eStarted;

    markResourceAsUsed(timerQuery);
}

void qhwi::VkCommandList::endTimerQuery(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(timerQuery->timerState == TimerState::eStarted, "Cannot end a timer that has not been started");

    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot end a timer inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot end a timer inside a compute pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot end a timer inside a transfer pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot end a timer inside an acceleration structure pass");

    m_scheduledTimerQuery = nullptr;

    timerQuery->timerState = TimerState::eStopped;

    markResourceAsUsed(timerQuery);
}

void qhwi::VkCommandList::resolveTimerQueryData(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(timerQuery->timerState == TimerState::eStopped, "Cannot resolve a timer that has not been stopped.");

    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot resolve a timer inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot resolve a timer inside a compute pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot resolve a timer inside a transfer pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot resolve a timer inside an acceleration structure pass");

    timerQuery->timerState = TimerState::eUnresolved;

    markResourceAsUsed(timerQuery);
}

void qhwi::VkCommandList::resetTimerQueryPool(TimerQuery* timerQuery)
{
    QHWI_ASSERT_MSG(m_inRenderPass == false, "Cannot reset a timer inside a render pass");
    QHWI_ASSERT_MSG(m_inComputePass == false, "Cannot reset a timer inside a compute pass");
    QHWI_ASSERT_MSG(m_inTransferPass == false, "Cannot reset a timer inside a transfer pass");
    QHWI_ASSERT_MSG(m_inAccelerationStructurePass == false, "Cannot reset a timer inside an acceleration structure pass");

    m_commandBuffer->resetQueryPool(m_device->getTimerQueryPool(), timerQuery->beginQueryIndex, 2); // Query count = 2 {beginQuery, endQuery}
    timerQuery->timerState = TimerState::eReset;
}

qhwi::VkFence::VkFence(VkDevice* device, FenceState initialValue, const char* name, HeapFlags heapFlags)
    : m_device(device)
{
    QHWI_ASSERT(device);
    auto vkDevice = device->get();
    vk::ExportFenceCreateInfo exportFenceCreateInfo(s_ExternalFenceHandleTypeFlags);
    vk::FenceCreateInfo fenceCreateInfo((initialValue == FenceState_Signaled) ? vk::FenceCreateFlagBits::eSignaled : vk::FenceCreateFlagBits(0));
    if (heapFlags & HeapFlag_Shared)
    {
        fenceCreateInfo.setPNext(&exportFenceCreateInfo);
    }
    m_fence = vkDevice.createFenceUnique(fenceCreateInfo);
    setName(m_device->get(), vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::eFence, (uint64_t)(::VkFence)m_fence.get(), name));
    if (heapFlags & HeapFlag_Shared)
    {
        m_sharedHandle = ::getSharedHandle(m_device->get(), m_fence.get());
    }
}

qhwi::Result qhwi::VkFence::wait()
{
    auto vkDevice = m_device->get();
    auto vkResult = vkDevice.waitForFences(1, &m_fence.get(), VK_TRUE, UINT64_MAX);
    if (vkResult != vk::Result::eSuccess)
    {
        QHWI_ERR("wait for fences failed");
    }
    return Result(ResultCode::eSuccess);
}

qhwi::FenceState qhwi::VkFence::getCurrentState()
{
    auto vkDevice = m_device->get();
    return (vkDevice.getFenceStatus(m_fence.get()) == vk::Result::eSuccess) ? FenceState_Signaled : FenceState_Unsignaled;
}

void qhwi::VkFence::reset()
{
    auto vkDevice = m_device->get();
    V(vkDevice.resetFences(1, &m_fence.get()));
}

qhwi::VkDevice::VkDevice(Context* context, vk::Instance instance, const VkPhysicalDevice& physicalDevice, const Description& desc)
    : Device(context)
    , m_desc(desc)
{
    init(context, instance, physicalDevice, desc);
}

qhwi::VkDevice::VkDevice(Context* context,
                         vk::Instance instance,
                         vk::PhysicalDevice physicalDevice,
                         vk::Device device,
                         const Description& desc,
                         const char** clientInstanceExtensions,
                         size_t clientInstanceExtensionsCount,
                         const char** clientDeviceExtensions,
                         size_t clientDeviceExtensionsCount)
    : Device(context)
    , m_desc(desc)
{
    InitFromHandleParams initFromHandleParams = { .device = device,
                                                  .instanceExtensions = { clientInstanceExtensions, clientInstanceExtensionsCount },
                                                  .deviceExtensions = { clientDeviceExtensions, clientDeviceExtensionsCount } };
    init(context, instance, physicalDevice, desc, &initFromHandleParams);
}

void qhwi::VkDevice::init(Context* context,
                          vk::Instance instance,
                          const VkPhysicalDevice& physicalDevice,
                          const Description& desc,
                          const InitFromHandleParams* initFromHandleParams)
{
    m_instance = instance;
    m_physicalDevice = physicalDevice;
    m_rayTracingPipelineProperties = {};
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    m_storageFactory = nullptr;
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

    // query features of physical device
    setFeature(Feature_ConservativeRasterization, supportsExtension(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME));
    const auto vkFeaturesInfo = getVkFeaturesInfo();
    setFeature(Feature_TextureCompressionAstc, vkFeaturesInfo.supportsTextureCompressionAstc());
    setFeature(Feature_TextureCompressionBc, vkFeaturesInfo.supportsTextureCompressionBc());
    setFeature(Feature_TextureCompressionEtc, vkFeaturesInfo.supportsTextureCompressionEtc());
    setFeature(Feature_RayQuery, vkFeaturesInfo.supportsRayQuery());
    setFeature(Feature_RayPipelines, vkFeaturesInfo.supportsRayPipelines());
    setFeature(Feature_RtTraceRaysIndirect, vkFeaturesInfo.supportsRayTraceMaintenance1Features());
    setFeature(Feature_RtInvocationReorder, vkFeaturesInfo.supportsRayTraceInvocationReorderFeature());
    setFeature(Feature_MeshShaders, vkFeaturesInfo.supportsMeshShaders());
    setFeature(Feature_HdrMetadata, supportsExtension(VK_EXT_HDR_METADATA_EXTENSION_NAME));
    setFeature(Feature_Barycentric, supportsExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME));
    setFeature(Feature_StorageQueue, supportsExtension(VK_EXT_MEMORY_DECOMPRESSION_EXTENSION_NAME));
    setFeature(Feature_OpacityMicromaps, vkFeaturesInfo.supportsOpacityMicromaps());
    setFeature(Feature_CustomResolve,
               vkFeaturesInfo.supportsCustomResolve() && vkFeaturesInfo.supportsDynamicRendering() && vkFeaturesInfo.supportsDynamicRenderingLocalRead() &&
                   supportsExtension(VK_EXT_CUSTOM_RESOLVE_EXTENSION_NAME));

    if (vkFeaturesInfo.supportsOpticalFlowNV())
    {
        vk::OpticalFlowImageFormatInfoNV oflowInputFormatInfo(vk::OpticalFlowUsageFlagBitsNV::eInput);
        m_opticalFlowInputFormats = m_physicalDevice.get().getOpticalFlowImageFormatsNV(oflowInputFormatInfo);
        vk::OpticalFlowImageFormatInfoNV oflowOutputFormatInfo(vk::OpticalFlowUsageFlagBitsNV::eOutput);
        m_opticalFlowOutputFormats = m_physicalDevice.get().getOpticalFlowImageFormatsNV(oflowOutputFormatInfo);
    }

    if (vkFeaturesInfo.supportsRayPipelines())
    {
        const auto& rayTracingPipelineProperties = getVkPropertiesInfo().rayTracingPipeline;

        m_rayTracingPipelineProperties = { .shaderGroupHandleSize = rayTracingPipelineProperties.shaderGroupHandleSize,
                                           .shaderGroupHandleAlignment = rayTracingPipelineProperties.shaderGroupHandleAlignment,
                                           .shaderGroupBaseAlignment = rayTracingPipelineProperties.shaderGroupBaseAlignment };
    }

    if (vkFeaturesInfo.supportsMemoryDecompression())
    {
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
        m_storageFactory = qre::MakeRefCountPtr<qhwi::compressedstorage::Factory>(new qhwi::compressedstorage::VkFactory(this));
#endif
    }

    if (!initFromHandleParams)
    {
        // set queue create infos
        m_defaultQueueFamilyIndex = findQueueFamilyIndex();
        QHWI_ASSERT(m_defaultQueueFamilyIndex != -1);
        m_opticalFlowQueueFamilyIndex = findOpticalFlowQueueFamilyIndex();
        m_dataGraphQueueFamilyIndex = findDataGraphQueueFamilyIndex();
        bool hasOpticalFlowQueue = (m_opticalFlowQueueFamilyIndex != -1);
        bool hasDataGraphQueue = (m_dataGraphQueueFamilyIndex != -1);
        bool hasDedicatedOpticalFlowQueue = (hasOpticalFlowQueue && (m_opticalFlowQueueFamilyIndex != m_defaultQueueFamilyIndex));
        bool hasDedicatedDataGraphQueue = (hasDataGraphQueue && (m_dataGraphQueueFamilyIndex != m_defaultQueueFamilyIndex));
        std::array queuePriorities = { 0.0f };
        constexpr uint32_t MaxQueueCreateInfos = 3;
        uint32_t queueCreateInfoCount = 0;
        std::array<vk::DeviceQueueCreateInfo, MaxQueueCreateInfos> queues;
        queues[queueCreateInfoCount++] = vk::DeviceQueueCreateInfo({}, m_defaultQueueFamilyIndex, 1, queuePriorities.data());
        if (hasDedicatedOpticalFlowQueue)
        {
            queues[queueCreateInfoCount++] = vk::DeviceQueueCreateInfo({}, m_opticalFlowQueueFamilyIndex, 1, queuePriorities.data());
        }
        if (hasDedicatedDataGraphQueue)
        {
            queues[queueCreateInfoCount++] = vk::DeviceQueueCreateInfo({}, m_dataGraphQueueFamilyIndex, 1, queuePriorities.data());
        }

        // set extensions
        auto& extensions = m_enabledDeviceExtensions;
        extensions = VkPhysicalDevice::getDefaultRequiredExtensions();
        if (!m_physicalDevice.supportsExtensions(extensions))
        {
            QHWI_FATAL("");
        }
        bool enableVkQcomTileMemoryHeap = desc.base.enableVkQcomTileMemoryHeap;

        auto optionalExtensions = VkPhysicalDevice::getDefaultOptionalExtensions();
        for (const char* ext : optionalExtensions)
        {
            if (m_physicalDevice.supportsExtension(ext))
            {
                extensions.push_back(ext);
            }
        }
        if (!contains(extensions, VK_KHR_ROBUSTNESS_2_EXTENSION_NAME) && m_physicalDevice.supportsExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME))
        {
            extensions.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        }
        // create the device
        vk::DeviceCreateInfo createInfo({}, uint32_t(queueCreateInfoCount), queues.data(), 0, nullptr, uint32_t(extensions.size()), extensions.data());
        const auto& memProps = m_physicalDevice.m_memoryProperties;
        bool supportsVkQcomTileMemoryHeap = vkFeaturesInfo.supportsTileMemoryHeapQCOM();

        if (enableVkQcomTileMemoryHeap && supportsVkQcomTileMemoryHeap)
        {
            // query for PGMEM
            for (uint32_t j = 0; j < memProps.memoryHeapCount; j++)
            {
                const auto& memHeap = memProps.memoryHeaps[j];
                if (VkMemoryHeapFlags(memHeap.flags) & VK_MEMORY_HEAP_TILE_MEMORY_BIT_QCOM)
                {
                    m_tileMemSize = memHeap.size;
                    m_tileMemHeapIndex = j;
                    break;
                }
            }
            if (!m_tileMemSize)
            {
                QHWI_ERR("tile memory size is 0");
            }

            for (uint32_t j = 0; j < memProps.memoryTypeCount; j++)
            {
                const auto& memType = memProps.memoryTypes[j];
                if (memType.heapIndex == m_tileMemHeapIndex)
                {
                    m_tileMemMemoryType = memType;
                    m_tileMemMemoryTypeIndex = j;
                    break;
                }
            }
        }

        createInfo.pNext = &m_physicalDevice.m_vkFeaturesInfo.features2;

        QHWI_LOGI("VK_QCOM_TILE_MEMORY_HEAP supported: %d enabled: %d tileMemSize: %lld", supportsVkQcomTileMemoryHeap, enableVkQcomTileMemoryHeap, m_tileMemSize);
        m_device = m_physicalDevice.get().createDeviceUnique(createInfo);
        if (enableVkQcomTileMemoryHeap && m_tileMemSize)
        {
            m_tileMem = m_device->allocateMemoryUnique(vk::MemoryAllocateInfo(m_tileMemSize, m_tileMemMemoryTypeIndex));
            m_fn.cmdBindTileMemoryQCOM = (PFN_vkCmdBindTileMemoryQCOM)m_device->getProcAddr("vkCmdBindTileMemoryQCOM");
            QHWI_ASSERT(m_fn.cmdBindTileMemoryQCOM);
        }
        m_ownsDeviceObject = true;

        m_debugUtilsInstanceExtensionEnabled = static_cast<VkContext*>(m_context)->debugUtilsEnabled();
    }
    else
    {
        m_device.reset(initFromHandleParams->device);
        m_ownsDeviceObject = false;

        m_debugUtilsInstanceExtensionEnabled = contains(initFromHandleParams->instanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        const auto& deviceExtensions = initFromHandleParams->deviceExtensions;
        m_enabledDeviceExtensions.resize(deviceExtensions.size());
        for (const char* s : deviceExtensions)
        {
            m_enabledDeviceExtensions.push_back(s);
        }
    }

    const auto& properties = getVkPropertiesInfo().properties2.properties;
    const char* vendorName = getVendorName(properties.vendorID);

    QHWI_LOGI("Initialized Vulkan %d.%d.%d HWI. Vendor: %s, Device ID: 0x%04x, Device Name: %s, Driver Version: %u",
              VK_VERSION_MAJOR(properties.apiVersion),
              VK_VERSION_MINOR(properties.apiVersion),
              VK_VERSION_PATCH(properties.apiVersion),
              vendorName,
              properties.deviceID,
              str(properties.deviceName.data()),
              properties.driverVersion);

    // populate limits data structure
    const auto& propsInfo = getVkPropertiesInfo();
    const auto& props = propsInfo.properties2.properties;
    m_limitsInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy;
    m_limitsInfo.maxTextureDimension1D = props.limits.maxImageDimension1D;
    m_limitsInfo.maxTextureDimension2D = props.limits.maxImageDimension2D;
    m_limitsInfo.maxTextureDimension3D = props.limits.maxImageDimension3D;
    m_limitsInfo.maxTextureDimensionCube = props.limits.maxImageDimensionCube;
    m_limitsInfo.maxRenderTargets = props.limits.maxColorAttachments;
    bool hasSubgroupSizeControl = supportsExtension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
    m_limitsInfo.minWaveSize = hasSubgroupSizeControl ? propsInfo.subgroupSizeControl.minSubgroupSize : propsInfo.subgroup.subgroupSize;
    m_limitsInfo.maxWaveSize = hasSubgroupSizeControl ? propsInfo.subgroupSizeControl.maxSubgroupSize : propsInfo.subgroup.subgroupSize;

    createInternalResources();

    if (m_physicalDevice.getVkFeaturesInfo().supportsTileShadingQCOM())
    {
        m_fn.cmdBeginPerTileExecutionQCOM = (PFN_vkCmdBeginPerTileExecutionQCOM)m_device->getProcAddr("vkCmdBeginPerTileExecutionQCOM");
        QHWI_ASSERT(m_fn.cmdBeginPerTileExecutionQCOM);
        m_fn.cmdEndPerTileExecutionQCOM = (PFN_vkCmdEndPerTileExecutionQCOM)m_device->getProcAddr("vkCmdEndPerTileExecutionQCOM");
        QHWI_ASSERT(m_fn.cmdEndPerTileExecutionQCOM);
        m_fn.cmdDispatchTileQCOM = (PFN_vkCmdDispatchTileQCOM)m_device->getProcAddr("vkCmdDispatchTileQCOM");
        QHWI_ASSERT(m_fn.cmdDispatchTileQCOM);
        m_fn.getFramebufferTilePropertiesQCOM = (PFN_vkGetFramebufferTilePropertiesQCOM)m_device->getProcAddr("vkGetFramebufferTilePropertiesQCOM");
        QHWI_ASSERT(m_fn.getFramebufferTilePropertiesQCOM);
    }

    if (m_physicalDevice.getVkFeaturesInfo().supportsQueuePerfHintQCOM())
    {
        m_fn.queueSetPerfHintQCOM = (PFN_vkQueueSetPerfHintQCOM)m_device->getProcAddr("vkQueueSetPerfHintQCOM");
        QHWI_ASSERT(m_fn.queueSetPerfHintQCOM);
    }

    VmaAllocatorCreateInfo info{};
    info.flags = vkFeaturesInfo.supportsDeviceAddress() ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0;
    info.device = m_device.get();
    info.preferredLargeHeapBlockSize = 0;
    info.pAllocationCallbacks = nullptr;
    info.pDeviceMemoryCallbacks = nullptr;
    info.physicalDevice = physicalDevice.get();
    info.pHeapSizeLimit = nullptr;
    info.pVulkanFunctions = static_cast<VkContext*>(m_context)->vulkanFunctions();
    info.instance = instance;
    info.vulkanApiVersion = MinVkApiVersion;
    info.pTypeExternalMemoryHandleTypes = nullptr;

    V(vmaCreateAllocator(&info, &m_vmaAllocator));

    // Create a memory pool dedicated for shared allocations
    // Find memoryTypeIndex for the pool.
    VkBufferCreateInfo sampleBufCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    sampleBufCreateInfo.size = 0x10000; // Doesn't matter.
    sampleBufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    sampleBufCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VmaAllocationCreateInfo sampleAllocCreateInfo = {};
    sampleAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    uint32_t memTypeIndex;
    vmaFindMemoryTypeIndexForBufferInfo(m_vmaAllocator, &sampleBufCreateInfo, &sampleAllocCreateInfo, &memTypeIndex);

    // Create a pool
    m_vmaMemoryAllocateInfo.handleTypes = (VkExternalMemoryHandleTypeFlagsKHR)s_ExternalMemoryHandleTypeFlags;
    m_vmaMemoryAllocateInfo.pNext = nullptr;

    VmaPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.memoryTypeIndex = memTypeIndex;
    poolCreateInfo.blockSize = 0;
    poolCreateInfo.maxBlockCount = 2;
    poolCreateInfo.pMemoryAllocateNext = &m_vmaMemoryAllocateInfo;

    V(vmaCreatePool(m_vmaAllocator, &poolCreateInfo, &m_sharedAllocationsPool));
}

bool qhwi::VkDevice::isExtensionEnabled(const char* name) const
{
    return contains(m_enabledDeviceExtensions, name);
}

qhwi::VkDevice::~VkDevice()
{
    // Detach the underlying object from the unique handle if qhwi doesn't own the device.
    // Otherwise, the unique handle would attempt to destroy the object.
    if (!m_ownsDeviceObject)
    {
        m_device.release();
    }
    m_uploadPool.destroy();
    vmaDestroyPool(m_vmaAllocator, m_sharedAllocationsPool);
    vmaDestroyAllocator(m_vmaAllocator);
}

vk::DeviceSize qhwi::VkDevice::allocateTileMemory(vk::DeviceSize size, vk::DeviceSize alignment)
{
    m_tileMemOffset = qre::AlignUp(m_tileMemOffset, alignment);
    vk::DeviceSize p = m_tileMemOffset;
    m_tileMemOffset += size;
    if (m_tileMemOffset > m_tileMemSize)
    {
        QHWI_ERR("out of tile memory");
    }
    return p;
}

void qhwi::VkDevice::freeTileMemory(vk::DeviceSize offset)
{
    //TODO
}

void qhwi::VkDevice::getFramebufferTilePropertiesQCOM(VkFramebuffer framebuffer, uint32_t* pPropertiesCount, VkTilePropertiesQCOM* pProperties)
{
    V(m_fn.getFramebufferTilePropertiesQCOM(m_device.get(), framebuffer, pPropertiesCount, pProperties));
}

VkTilePropertiesQCOM qhwi::VkDevice::getTilePropertiesQCOM(uint32_t numColorAttachments,
                                                           const ColorAttachmentDescription* colorAttachmentDescs,
                                                           const DepthStencilAttachmentDescription* depthStencilAttachmentDesc,
                                                           const MultiViewState* multiViewState,
                                                           const Rect2D* multiViewPerViewRenderAreasQCOM,
                                                           const uint32_t* colorAttachmentRemapping)
{
    if (multiViewPerViewRenderAreasQCOM && !getVkFeaturesInfo().supportsMultiViewPerViewRenderAreasQCOM())
    {
        // Ignore param if device doesn't support extension
        multiViewPerViewRenderAreasQCOM = nullptr;
    }

    vk::Extent3D extent = colorAttachmentDescs ? getExtent(colorAttachmentDescs[0]) : getExtent(*depthStencilAttachmentDesc);
    uint32_t numClearValues = 0;
    vk::ClearValue clearValues[qhwi::MaxAttachments];

    for (uint32_t j = 0; j < numColorAttachments; j++)
    {
        QHWI_ASSERT(numClearValues < qhwi::MaxAttachments);

        clearValues[numClearValues++].color = vk::ClearColorValue(colorAttachmentDescs[j].clearValue.rgba);
        if (colorAttachmentDescs[j].resolveTarget.texture != nullptr)
        {
            // clear values for resolve attachments do nothing, but keep the clearValues array the same
            // size as the attachment array
            clearValues[numClearValues++].color = vk::ClearColorValue(0, 0, 0, 0);
        }
    }

    if (depthStencilAttachmentDesc)
    {
        QHWI_ASSERT(numClearValues < qhwi::MaxAttachments);

        const auto& v = depthStencilAttachmentDesc->clearValue;
        clearValues[numClearValues++].depthStencil = vk::ClearDepthStencilValue(v.depth, v.stencil);
        if (depthStencilAttachmentDesc->resolveTarget.texture != nullptr)
        {
            clearValues[numClearValues++].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);
        }
    }

    qre::StaticVector<vk::RenderingAttachmentInfoKHR, qhwi::MaxColorAttachments> colorAttachments(numColorAttachments);
    for (uint32_t j = 0; j < numColorAttachments; j++)
    {
        const auto& colorAttachmentDesc = colorAttachmentDescs[j];
        auto& vkColorAttachment = colorAttachments[j];
        const auto& resolveTarget = colorAttachmentDesc.resolveTarget;
        bool hasResolveTarget = (resolveTarget.texture != nullptr);
        vkColorAttachment = vk::RenderingAttachmentInfoKHR()
                                .setImageView(vkImageView(colorAttachmentDesc.textureData))
                                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                .setLoadOp(qhwi::vk(colorAttachmentDesc.loadOp))
                                .setStoreOp(qhwi::vk(colorAttachmentDesc.storeOp))
                                .setClearValue(vk::ClearColorValue(colorAttachmentDescs[j].clearValue.rgba));
        if (hasResolveTarget)
        {
            vkColorAttachment.setResolveImageView(vkImageView(resolveTarget))
                .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setResolveMode(vk::ResolveModeFlagBitsKHR::eAverage);
        }
    }
    auto depthAttachment = vk::RenderingAttachmentInfoKHR();
    auto stencilAttachment = vk::RenderingAttachmentInfoKHR();

    bool supportsStencil = false;
    if (depthStencilAttachmentDesc)
    {
        const auto& clearValue = depthStencilAttachmentDesc->clearValue;
        auto imageView = vkImageView(depthStencilAttachmentDesc->textureData);
        auto vkTexture = static_cast<qhwi::VkTexture*>(depthStencilAttachmentDesc->textureData.texture);
        supportsStencil = qhwi::supportsStencil(vkTexture->format());
        depthAttachment.setImageView(imageView)
            .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setLoadOp(qhwi::vk(depthStencilAttachmentDesc->depthLoadOp))
            .setStoreOp(qhwi::vk(depthStencilAttachmentDesc->depthStoreOp))
            .setClearValue(vk::ClearDepthStencilValue(clearValue.depth, clearValue.stencil))
            .setResolveMode(qhwi::vk(depthStencilAttachmentDesc->depthResolveMode));

        if (supportsStencil)
        {
            stencilAttachment.setImageView(imageView)
                .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                .setLoadOp(qhwi::vk(depthStencilAttachmentDesc->stencilLoadOp))
                .setStoreOp(qhwi::vk(depthStencilAttachmentDesc->stencilStoreOp))
                .setClearValue(vk::ClearDepthStencilValue(clearValue.depth, clearValue.stencil))
                .setResolveMode(qhwi::vk(depthStencilAttachmentDesc->stencilResolveMode));
        }
    }
    uint32_t numViews = multiViewState ? multiViewState->numViews : 0;
    bool multiView = (numViews != 0);
    uint32_t viewMask = 0;
    uint32_t layerCount = 1;
    if (multiView)
    {
        viewMask = (1 << numViews) - 1;
    }

    vk::RenderingInfoKHR renderingInfo = vk::RenderingInfoKHR()
                                             .setRenderArea(vk::Rect2D({ 0, 0 }, { extent.width, extent.height }))
                                             .setViewMask(viewMask)
                                             .setLayerCount(layerCount)
                                             .setColorAttachmentCount(numColorAttachments)
                                             .setPColorAttachments(colorAttachments.data())
                                             .setPDepthAttachment(depthStencilAttachmentDesc ? &depthAttachment : nullptr)
                                             .setPStencilAttachment(supportsStencil ? &stencilAttachment : nullptr);

    vk::MultiviewPerViewRenderAreasRenderPassBeginInfoQCOM perViewRenderAreasRpBeginInfoQCOM;
    vk::Rect2D vkMultiViewPerViewRenderAreasQCOM[qhwi::MaxViews];
    VkTileMemorySizeInfoQCOM vkTileMemorySizeInfoQCOM{};

    const void** ppNext = &renderingInfo.pNext;

    if (multiViewPerViewRenderAreasQCOM)
    {
        for (uint32_t j = 0; j < numViews; j++)
        {
            vkMultiViewPerViewRenderAreasQCOM[j] = qhwi::vk(multiViewPerViewRenderAreasQCOM[j]);
        }
        perViewRenderAreasRpBeginInfoQCOM = vk::MultiviewPerViewRenderAreasRenderPassBeginInfoQCOM(numViews, vkMultiViewPerViewRenderAreasQCOM);
        *ppNext = &perViewRenderAreasRpBeginInfoQCOM;
        ppNext = &perViewRenderAreasRpBeginInfoQCOM.pNext;
    }

    if (m_tileMem.get())
    {
        vkTileMemorySizeInfoQCOM.sType = VK_STRUCTURE_TYPE_TILE_MEMORY_SIZE_INFO_QCOM;
        vkTileMemorySizeInfoQCOM.size = m_tileMemSize;
        *ppNext = &vkTileMemorySizeInfoQCOM;
        ppNext = &vkTileMemorySizeInfoQCOM.pNext;
    }

    return m_device->getDynamicRenderingTilePropertiesQCOM(renderingInfo);
}

bool qhwi::VkPhysicalDevice::supportsExtensions(std::span<const char* const> extensions) const
{
    for (const char* name : extensions)
    {
        if (!supportsExtension(name))
        {
            return false;
        }
    }
    return true;
}

void qhwi::VkFeaturesInfo::init(const VkPhysicalDevice* physicalDevice)
{
    rtInvocationReorderFeatures.setRayTracingInvocationReorder(vk::True);

    qhwi::VkStructChain(features2, physicalDevice)
        .link(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, accelerationStructure)
        .link(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, bufferAddress)
        .link(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, descriptorIndexing)
        .link(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, fragmentShadingRate)
        .link(VK_QCOM_IMAGE_PROCESSING_EXTENSION_NAME, imageProcessingQCOM)
        .link(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, indexTypeUint8)
        .link(VK_EXT_MESH_SHADER_EXTENSION_NAME, meshShader)
        .link(VK_KHR_MULTIVIEW_EXTENSION_NAME, multiView)
        .link(VK_QCOM_MULTIVIEW_PER_VIEW_RENDER_AREAS_EXTENSION_NAME, multiViewPerViewRenderAreasQCOM)
        .link(VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_EXTENSION_NAME, multiViewPerViewViewportsQCOM)
        .link(VK_NV_OPTICAL_FLOW_EXTENSION_NAME, opticalFlowNV)
        .link(VK_EXT_MEMORY_DECOMPRESSION_EXTENSION_NAME, memoryDecompressionFeatures)
        .link(VK_KHR_RAY_QUERY_EXTENSION_NAME, rayQuery)
        .link(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, rayPipeline)
        .linkAliased(VK_KHR_ROBUSTNESS_2_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, robustness2FeaturesEXT)
        .link(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME, rtInvocationReorderFeatures)
        .link(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME, rtMaintenance1Features)
        .link(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, shaderFloat16Int8)
        .link(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, subgroupSizeControl)
        .link(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, synchronization2)
        .link(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, timelineSemaphore)
        .link(VK_QCOM_TILE_SHADING_EXTENSION_NAME, tileShadingFeatures)
        .link(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, graphicsPipelineLibraryFeatures)
        .link(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, dynamicRenderingFeatures)
        .link(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME, dynamicRenderingLocalReadFeatures)
        .link(VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME, dynamicRenderingUnusedAttachmentsFeatures)
        .link(VK_QCOM_TILE_MEMORY_HEAP_EXTENSION_NAME, tileMemoryHeapQCOM)
        .link(VK_ARM_TENSORS_EXTENSION_NAME, tensorFeaturesARM)
        .link(VK_ARM_DATA_GRAPH_EXTENSION_NAME, dataGraphFeaturesARM)
        .link(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, barycentricFeatures)
        .link(VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME, pipelineCreationCacheControl)
        .link(VK_EXT_OPACITY_MICROMAP_EXTENSION_NAME, opacityMicromap)
        .link(VK_QCOM_QUEUE_PERF_HINT_EXTENSION_NAME, queuePerfHintFeaturesQCOM)
        .link(VK_QCOM_ELAPSED_TIMER_QUERY_EXTENSION_NAME, elapsedTimerQueryFeaturesQCOM)
        .link(VK_EXT_CUSTOM_RESOLVE_EXTENSION_NAME, customResolveFeatures);
}

void qhwi::VkPhysicalDevice::init(vk::PhysicalDevice physicalDevice)
{
    m_physicalDevice = physicalDevice;
    m_supportedExtensions = m_physicalDevice.enumerateDeviceExtensionProperties();
    m_vkFeaturesInfo.init(this);
    detectProperties();
    m_physicalDevice.getFeatures2(&m_vkFeaturesInfo.features2);
    m_supportsSynchronization2 = supportsExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    m_supportsSubgroupSizeControl = supportsExtension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
    m_supportsSamplerFilterMinMax = supportsExtension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
    m_supportsBarycentric = supportsExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    m_supportsMemoryDecompression = supportsExtension(VK_EXT_MEMORY_DECOMPRESSION_EXTENSION_NAME);
    m_supportsCalibratedTimestamps = supportsExtension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
    m_supportsShaderExecutionReorder = supportsExtension(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
}

void qhwi::VkPropertiesInfo::init(const qhwi::VkPhysicalDevice* physicalDevice)
{
    VkStructChain(properties2, physicalDevice)
        .link(VK_QCOM_IMAGE_PROCESSING_EXTENSION_NAME, imageProcessingQCOM)
        .link(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, accelerationStructure)
        .link(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, depthStencilResolve)
        .link(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, conservativeRasterization)
        .link(subgroup)
        .link(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, subgroupSizeControl)
        .link(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, graphicsPipelineLibrary)
        .link(rayTracingPipeline)
        .link(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME, rtInvocationReorderProperties)
        .link(VK_QCOM_TILE_MEMORY_HEAP_EXTENSION_NAME, tileMemoryHeapQCOM)
        .link(VK_EXT_MEMORY_DECOMPRESSION_EXTENSION_NAME, memoryDecompressionProperties)
        .link(VK_QCOM_QUEUE_PERF_HINT_EXTENSION_NAME, queuePerfHintPropertiesQCOM);

    physicalDevice->get().getProperties2(&properties2);

    QHWI_LOGI("physicalDeviceProperties device: %s apiVersion: %x.%x.%x driverVersion: %x",
              str(properties2.properties.deviceName),
              VK_VERSION_MAJOR(properties2.properties.apiVersion),
              VK_VERSION_MINOR(properties2.properties.apiVersion),
              VK_VERSION_PATCH(properties2.properties.apiVersion),
              properties2.properties.driverVersion);
}

void qhwi::VkPhysicalDevice::detectProperties()
{
    m_vkPropertiesInfo.init(this);
    m_queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();
    m_memoryProperties = m_physicalDevice.getMemoryProperties();
}

void qhwi::VkDevice::createInternalResources()
{
    int32_t defaultQueueFamilyIndex = findQueueFamilyIndex();
    QHWI_ASSERT(defaultQueueFamilyIndex != -1);
    m_commandPool[0] = m_device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, defaultQueueFamilyIndex));
    m_defaultCommandPool = m_commandPool[0].get();
    if (getVkFeaturesInfo().supportsOpticalFlowNV())
    {
        bool hasDedicatedOpticalFlowQueue = (m_opticalFlowQueueFamilyIndex != m_defaultQueueFamilyIndex);
        if (hasDedicatedOpticalFlowQueue)
        {
            m_commandPool[1] = m_device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_opticalFlowQueueFamilyIndex));
            m_opticalFlowCommandPool = m_commandPool[1].get();
        }
        else
        {
            m_opticalFlowCommandPool = m_commandPool[0].get();
        }
    }
    m_pipelineCache = m_device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
    createDescriptorPool();

    m_timerQueryHeap.init(this);
    m_imageViewCache.init(this);
}

vk::DescriptorPool qhwi::VkDevice::createDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> descriptorPoolSizes = {
        { vk::DescriptorType::eUniformBuffer, MaxCbvDescriptors },
        { vk::DescriptorType::eCombinedImageSampler, MaxSamplerDescriptors },
        { vk::DescriptorType::eStorageImage, MaxSrvDescriptors },
        { vk::DescriptorType::eSampledImage, MaxSrvDescriptors },
        { vk::DescriptorType::eSampler, MaxSrvDescriptors },
        { vk::DescriptorType::eStorageTexelBuffer, MaxSrvDescriptors },
        { vk::DescriptorType::eUniformTexelBuffer, MaxSrvDescriptors },
        { vk::DescriptorType::eStorageBuffer, MaxUavDescriptors },
        { vk::DescriptorType::eInputAttachment, MaxSrvDescriptors },
    };

    if (getVkFeaturesInfo().supportsAccelerationStructures())
    {
        descriptorPoolSizes.push_back({ vk::DescriptorType::eAccelerationStructureKHR, MaxSrvDescriptors });
    }

    m_descriptorPool.push_back(m_device->createDescriptorPoolUnique(
        vk::DescriptorPoolCreateInfo({ //TODO: expose an API to the user to create descriptor pools
                                       vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
                                       MaxDescriptorSets,
                                       static_cast<uint32_t>(descriptorPoolSizes.size()),
                                       descriptorPoolSizes.data() })));

    return m_descriptorPool.back().get();
}

qhwi::GraphicsBackend qhwi::VkDevice::getGraphicsBackend() const
{
    return qhwi::GraphicsBackend::eVulkan;
}

qhwi::VkContext* qhwi::VkDevice::getVkContext()
{
    return static_cast<VkContext*>(m_context);
}

int32_t qhwi::VkPhysicalDevice::findQueueFamilyIndex(vk::SurfaceKHR surface, vk::QueueFlags queueFlags, bool needsPresentSupport) const
{
    if (!(queueFlags & vk::QueueFlagBits::eGraphics))
    {
        needsPresentSupport = false;
    }
    for (uint32_t j = 0; j < m_queueFamilyProperties.size(); j++)
    {
        if (needsPresentSupport && surface)
        {
            bool supportsPresent = m_physicalDevice.getSurfaceSupportKHR(j, surface);
            if (!supportsPresent)
            {
                continue;
            }
        }
        const auto& p = m_queueFamilyProperties[j];
        vk::QueueFlags pQueueFlags = p.queueFlags;
        if (pQueueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))
        {
            // reporting transfer bit capability is optional when queue has capability graphics or compute
            // see: https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkQueueFlagBits.html
            pQueueFlags |= vk::QueueFlagBits::eTransfer;
        }
        if ((pQueueFlags & queueFlags) == queueFlags)
        {
            return j;
        }
    }
    return -1;
}

int32_t qhwi::VkPhysicalDevice::getMemoryType(uint32_t supportedTypesBitmask, vk::MemoryPropertyFlags memoryPropertyFlags)
{
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++)
    {
        if ((supportedTypesBitmask & 1) == 1)
        {
            const auto& vkMemoryType = m_memoryProperties.memoryTypes[i];
            if ((vkMemoryType.propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
            {
                return i;
            }
        }
        supportedTypesBitmask >>= 1;
    }
    return -1;
}

bool qhwi::VkPhysicalDevice::supportsExtension(const char* name) const
{
    QHWI_ASSERT(name);
    for (const auto& ext : m_supportedExtensions)
    {
        if (streq(ext.extensionName, name))
        {
            QHWI_LOGI("device extension supported: %s", name);
            return true;
        }
    }
    QHWI_LOGW("device extension not supported: %s", name);
    return false;
}

bool qhwi::VkDevice::isTimerQueryResultReady(TimerQuery* timerQuery)
{
    uint64_t timestamp[2];
    constexpr uint32_t queryCount = 2; // { BeginQuery, EndQuery }
    vk::Result result = m_device->getQueryPoolResults(m_timerQueryHeap.getTimerQueryHeap(),
                                                      timerQuery->beginQueryIndex,
                                                      queryCount,
                                                      sizeof(uint64_t) * queryCount,
                                                      timestamp,
                                                      sizeof(uint64_t),
                                                      vk::QueryResultFlagBits::e64);

    return result == vk::Result::eSuccess;
}

bool qhwi::VkDevice::getTimerQueryTimeInSec(TimerQuery* timerQuery, CommandQueue* commandQueue, bool blockUntilReady, float& timeInSec)
{
    QHWI_ASSERT(timerQuery->timerState == TimerState::eUnresolved);
    uint64_t timestamp[2];

    constexpr uint32_t queryCount = 2; // { BeginQuery, EndQuery }
    vk::QueryResultFlagBits queryResultFlagBits = (blockUntilReady == true) ? vk::QueryResultFlagBits::eWait : vk::QueryResultFlagBits();
    vk::Result result = m_device->getQueryPoolResults(m_timerQueryHeap.getTimerQueryHeap(),
                                                      timerQuery->beginQueryIndex,
                                                      queryCount,
                                                      sizeof(uint64_t) * queryCount,
                                                      timestamp,
                                                      sizeof(uint64_t),
                                                      queryResultFlagBits | vk::QueryResultFlagBits::e64);

    if (result != vk::Result::eSuccess)
    {
        return false;
    }

    timeInSec = float(timestamp[1] - timestamp[0]) * 1e-9f * m_physicalDevice.getLimits().timestampPeriod;
    timerQuery->timerState = TimerState::eResolved;

    return true;
}

bool qhwi::VkDevice::supportsCalibratedTimestamps() const
{
    // Check that physical device supports ext and that it's enabled
    bool physicalDeviceSupports = m_physicalDevice.supportsCalibratedTimestamps();
    bool extensionEnabled = isExtensionEnabled(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);

    return physicalDeviceSupports && extensionEnabled;
}

void qhwi::VkTimerQueryHeap::init(Device* device, const char*)
{
    auto vkDevice = static_cast<VkDevice*>(device)->get();

    queryHeap = vkDevice.createQueryPoolUnique(vk::QueryPoolCreateInfo({ {}, vk::QueryType::eTimestamp, MaxTimerQueryHeapSize, {}, nullptr }));

    freeList.Init(MaxTimerQueries);
}

size_t qhwi::VkDevice::getConstantBufferOffsetAlignment() const
{
    return m_physicalDevice.getLimits().minUniformBufferOffsetAlignment;
}

size_t qhwi::VkDevice::getMinStorageBufferOffsetAlignment() const
{
    return m_physicalDevice.getLimits().minStorageBufferOffsetAlignment;
}

qhwi::FormatSupportMask qhwi::VkDevice::queryFormatSupport(qhwi::Format format) const
{
    vk::Format vkFormat = vk(format);
    vk::FormatProperties formatProperties{};

    formatProperties = m_physicalDevice.get().getFormatProperties(vkFormat);

    FormatSupportMask result = FormatSupport_None;

    if (formatProperties.bufferFeatures)
    {
        result |= FormatSupport_Buffer;
    }
    if ((format == qhwi::Format::eR32Uint) || (format == qhwi::Format::eR16Uint) || ((format == qhwi::Format::eR8Uint) && getVkFeaturesInfo().supportsUint8Index()))
    {
        result |= FormatSupport_IndexBuffer;
    }

    const auto& optimalTilingFeatures = formatProperties.optimalTilingFeatures;
    if (optimalTilingFeatures & vk::FormatFeatureFlagBits::eVertexBuffer)
    {
        result |= FormatSupport_VertexBuffer;
    }
    if (optimalTilingFeatures & vk::FormatFeatureFlagBits::eStorageImage)
    {
        result |= FormatSupport_StorageImage;
    }
    if (optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
    {
        result |= FormatSupport_DepthStencilAttachment;
    }
    if (optimalTilingFeatures & vk::FormatFeatureFlagBits::eColorAttachment)
    {
        result |= FormatSupport_ColorAttachment;
    }
    if (optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)
    {
        result |= FormatSupport_SampledImage;
    }

    if (contains(m_opticalFlowInputFormats, vkFormat))
    {
        result |= FormatSupport_OpticalFlowImage;
    }
    if (contains(m_opticalFlowOutputFormats, vkFormat))
    {
        result |= FormatSupport_OpticalFlowVector;
    }

    return result;
}

qre::RefCountPtr<qhwi::ComputePipeline> qhwi::VkDevice::createComputePipeline(const ComputePipeline::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::ComputePipeline>(new VkComputePipeline(this, desc));
}

qhwi::VkComputePipeline::Description::Description(const ComputePipeline::Description& desc)
    : base(desc)
{
    vk.computePipelineCreateInfo = vk::ComputePipelineCreateInfo({}, qhwi::vkPipelineShaderStageCreateInfo(desc.computeShader.Get(), ShaderStage_Compute));
}

qhwi::VkComputePipeline::VkComputePipeline(VkDevice* device, const Description& desc)
    : m_device(device)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkComputePipiline::VkComputePipeline", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device);
    m_layout = desc.base.layout;
    auto vkDevice = m_device->get();
    auto cpCreateInfo = desc.vk.computePipelineCreateInfo;
    auto vkPipelineLayout = static_cast<VkPipelineLayout*>(desc.base.layout.Get());
    cpCreateInfo.setLayout(vkPipelineLayout->get());
    auto result = vkDevice.createComputePipelineUnique(m_device->getPipelineCache(), cpCreateInfo);
    if (result.result != vk::Result::eSuccess)
    {
        QHWI_FATAL("create compute pipeline failed, result: %s", vk::to_string(result.result).c_str());
    }
    if (desc.base.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::ePipeline, (uint64_t)(VkPipeline)result.value.get(), desc.base.name));
    }
    m_computePipeline = std::move(result.value);
}

qhwi::VkRayTracingPipeline::Description::Description(const qhwi::RayTracingPipelineDesc& desc)
    : base(desc)
{
    QHWI_ASSERT(desc.shaders.size() > 0 && desc.shaderGroups.size() > 0);

    auto qhwiShaderStage = [](ShaderType type) -> ShaderStage
    {
        switch (type)
        {
        case ShaderType::eRayGeneration:
            return ShaderStage::ShaderStage_RayGeneration;
        case ShaderType::eAnyHit:
            return ShaderStage::ShaderStage_AnyHit;
        case ShaderType::eClosestHit:
            return ShaderStage::ShaderStage_ClosestHit;
        case ShaderType::eMiss:
            return ShaderStage_Miss;
        case ShaderType::eIntersection:
            return ShaderStage::ShaderStage_Intersection;
        case ShaderType::eCallable:
            return ShaderStage::ShaderStage_Callable;
        default:
            QHWI_FATAL("Invalid shader type");
        }
    };

    // Fill shader create infos
    for (const Shader* shader : base.shaders)
    {
        vk.shaderStageCreateInfo.emplace_back(vkPipelineShaderStageCreateInfo(shader, qhwiShaderStage(shader->desc()->shaderType)));
    }

    // Fill shader group create infos
    for (const RayTracingPipelineShaderGroup& shaderGroup : base.shaderGroups)
    {
        uint32_t generalShader = VK_SHADER_UNUSED_KHR;
        uint32_t closestHitShader = VK_SHADER_UNUSED_KHR;
        uint32_t anyHitShader = VK_SHADER_UNUSED_KHR;
        uint32_t intersectionShader = VK_SHADER_UNUSED_KHR;

        if (shaderGroup.type == RayTracingShaderGroupType::eGeneral)
        {
            QHWI_ASSERT(shaderGroup.generalShader != ShaderUnused);
            generalShader = shaderGroup.generalShader;
        }
        else
        {
            closestHitShader = (shaderGroup.closestHitShader == ShaderUnused) ? VK_SHADER_UNUSED_KHR : shaderGroup.closestHitShader;
            anyHitShader = (shaderGroup.anyHitShader == ShaderUnused) ? VK_SHADER_UNUSED_KHR : shaderGroup.anyHitShader;
            intersectionShader = (shaderGroup.intersectionShader == ShaderUnused) ? VK_SHADER_UNUSED_KHR : shaderGroup.intersectionShader;
        }

        vk::RayTracingShaderGroupCreateInfoKHR vkShaderGroupCreateInfo = vk::RayTracingShaderGroupCreateInfoKHR();
        vkShaderGroupCreateInfo.type = qhwi::vk(shaderGroup.type);
        vkShaderGroupCreateInfo.generalShader = generalShader;
        vkShaderGroupCreateInfo.closestHitShader = closestHitShader;
        vkShaderGroupCreateInfo.anyHitShader = anyHitShader;
        vkShaderGroupCreateInfo.intersectionShader = intersectionShader;

        vk.shaderGroupCreateInfo.emplace_back(std::move(vkShaderGroupCreateInfo));
    }

    vk.rayTracingPipelineCreateInfo = vk::RayTracingPipelineCreateInfoKHR({},
                                                                          static_cast<uint32_t>(vk.shaderStageCreateInfo.size()),
                                                                          vk.shaderStageCreateInfo.data(),
                                                                          static_cast<uint32_t>(vk.shaderGroupCreateInfo.size()),
                                                                          vk.shaderGroupCreateInfo.data(),
                                                                          base.maxRayRecursionDepth);
}

qhwi::VkRayTracingPipeline::VkRayTracingPipeline(VkDevice* device, const qhwi::VkRayTracingPipeline::Description& desc)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkRayTracingPipeline::VkRayTracingPipeline", QHWI_COLOR_RESOURCE_CREATE);
    QHWI_ASSERT(device != nullptr);
    m_layout = desc.base.layout;

    vk::Device vkDevice = device->get();
    auto rayTracingPipelineCreateInfo = desc.vk.rayTracingPipelineCreateInfo;
    rayTracingPipelineCreateInfo.setLayout(static_cast<VkPipelineLayout*>(m_layout.Get())->get());
    auto result = vkDevice.createRayTracingPipelineKHRUnique(VK_NULL_HANDLE, device->getPipelineCache(), rayTracingPipelineCreateInfo);

    if (result.result != vk::Result::eSuccess)
    {
        QHWI_FATAL("create ray tracing pipeline failed, result: %s", vk::to_string(result.result).c_str());
    }

    if (desc.base.name)
    {
        setName(vkDevice, vk::DebugUtilsObjectNameInfoEXT(vk::ObjectType::ePipeline, (uint64_t)(VkPipeline)result.value.get(), desc.base.name));
    }

    m_rayTracingPipeline = std::move(result.value);
}

qre::RefCountPtr<qhwi::RayTracingPipeline> qhwi::VkDevice::createRayTracingPipeline(const qhwi::RayTracingPipeline::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::RayTracingPipeline>(new qhwi::VkRayTracingPipeline(this, desc));
}

qre::RefCountPtr<qhwi::PipelineLayout> qhwi::VkDevice::createPipelineLayout(const PipelineLayout::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::PipelineLayout>(new VkPipelineLayout(this, desc));
}

qre::RefCountPtr<qhwi::GraphicsPipeline> qhwi::VkDevice::createGraphicsPipeline(const GraphicsPipeline::Description& desc)
{
    return createGraphicsPipeline(VkGraphicsPipeline::Description(desc));
}

qre::RefCountPtr<qhwi::GraphicsPipeline> qhwi::VkDevice::createGraphicsPipeline(const VkGraphicsPipeline::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::GraphicsPipeline>(new VkGraphicsPipeline(this, desc));
}

qre::RefCountPtr<qhwi::Buffer> qhwi::VkDevice::createBuffer(const Buffer::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Buffer>(new VkBuffer(this, desc));
}

qre::RefCountPtr<qhwi::Texture> qhwi::VkDevice::createTexture(const Texture::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Texture>(new VkTexture(this, desc));
}

qre::RefCountPtr<qhwi::Texture> qhwi::VkDevice::createTextureFromVkHandle(const Texture::Description& desc, VkImage image, VkImageView imageView)
{
    return qre::MakeRefCountPtr<qhwi::Texture>(new VkTexture(this, desc, image, imageView));
}

qre::RefCountPtr<qhwi::Sampler> qhwi::VkDevice::createSampler(const Sampler::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Sampler>(new VkSampler(this, desc));
}

qre::RefCountPtr<qhwi::DescriptorSet> qhwi::VkDevice::createDescriptorSet(const DescriptorSet::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::DescriptorSet>(new VkDescriptorSet(this, desc));
}

qre::RefCountPtr<qhwi::DescriptorSetLayout> qhwi::VkDevice::createDescriptorSetLayout(const DescriptorSetLayout::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::DescriptorSetLayout>(new VkDescriptorSetLayout(this, desc));
}

void qhwi::toVkBufferInfos(const DescriptorBufferInfo* bufferInfos, vk::DescriptorBufferInfo* vkBufferInfos, uint32_t count)
{
    for (uint32_t k = 0; k < count; k++)
    {
        const DescriptorBufferInfo& bufferInfo = bufferInfos[k];
        const auto vkBuffer = static_cast<VkBuffer*>(bufferInfo.buffer.Get());

        if (!vkBuffer)
        {
            // create null descriptor
            vkBufferInfos[k] = vk::DescriptorBufferInfo(nullptr, 0, vk::WholeSize);
            continue;
        }
        QHWI_ASSERT(bufferInfo.offset < vkBuffer->size());
        uint64_t range = bufferInfo.range;
        if (range == 0)
        {
            range = vk::WholeSize;
        }
        vkBufferInfos[k] = vk::DescriptorBufferInfo(vkBuffer->get(), bufferInfo.offset, range);
    }
}

void qhwi::toVkBufferViews(const DescriptorBufferInfo* bufferInfos, vk::BufferView* vkBufferViews, uint32_t count)
{
    for (uint32_t k = 0; k < count; k++)
    {
        const DescriptorBufferInfo& bufferInfo = bufferInfos[k];
        const auto vkBuffer = static_cast<VkBuffer*>(bufferInfo.buffer.Get());
        QHWI_ASSERT(vkBuffer);
        QHWI_ASSERT(bufferInfo.offset < vkBuffer->size());

        auto vkDevice = vkBuffer->getDevice()->get();

        uint64_t range = bufferInfo.range;
        if (range == 0)
        {
            range = vk::WholeSize;
        }
        vkBufferViews[k] =
            vkDevice.createBufferView(vk::BufferViewCreateInfo().setBuffer(vkBuffer->get()).setOffset(bufferInfo.offset).setRange(range).setFormat(vk(vkBuffer->format())));
    }
}

void qhwi::toVkImageInfos(const DescriptorTextureInfo* textureInfos, vk::DescriptorImageInfo* vkImageInfos, vk::ImageView* imageViews, vk::ImageLayout imageLayout, uint32_t count)
{
    QHWI_ASSERT(vkImageInfos);
    vk::ImageView* vkImageViews = imageViews;
    for (uint32_t k = 0; k < count; k++)
    {
        const DescriptorTextureInfo& textureInfo = textureInfos[k];
        vk::DescriptorImageInfo& vkImageInfo = vkImageInfos[k];
        const auto& texture = textureInfo.texture;
        if (!texture)
        {
            // create null descriptor
            vkImageInfo = vk::DescriptorImageInfo();
            continue;
        }
        const auto vkTexture = static_cast<VkTexture*>(texture.Get());
        QHWI_ASSERT(vkTexture);
        vk::ImageView& vkImageView = vkImageViews[k];
        vk::ImageViewType vkImageViewType = (textureInfo.imageViewType == TextureType::eUndefined) ? qhwi::vk(texture->desc()->type) : qhwi::vk(textureInfo.imageViewType);
        if (textureInfo.subresourceRange.isEntireResource())
        {
            vkImageView = vkTexture->getImageView(vkImageViewType);
        }
        else
        {
            vk::ImageSubresourceRange subresourceRange = qhwi::vkImageSubresourceRange(textureInfo.subresourceRange, vkTexture->getImageAspectFlags());
            vkImageView = vkTexture->getImageView(vkImageViewType, &subresourceRange);
        }
        vkImageInfo = vk::DescriptorImageInfo({}, vkImageView, imageLayout);
    }
}

void qhwi::toVkImageInfos(const DescriptorSamplerInfo* samplerInfos, vk::DescriptorImageInfo* vkImageInfos, uint32_t count)
{
    QHWI_ASSERT(vkImageInfos);
    for (uint32_t k = 0; k < count; k++)
    {
        const DescriptorSamplerInfo& samplerInfo = samplerInfos[k];
        vk::DescriptorImageInfo& vkImageInfo = vkImageInfos[k];
        const auto& sampler = samplerInfo.sampler;
        if (!sampler)
        {
            // create null descriptor
            vkImageInfo = vk::DescriptorImageInfo();
            continue;
        }
        const auto vkSampler = static_cast<VkSampler*>(sampler.Get());
        QHWI_ASSERT(vkSampler);
        vkImageInfo = vk::DescriptorImageInfo(vkSampler->get(), {}, {});
    }
}

void qhwi::toVkAccelStructInfos(const DescriptorAccelerationStructureInfo* accelStructInfos, vk::WriteDescriptorSetAccelerationStructureKHR* vkAccelStructInfos, uint32_t count)
{
    QHWI_ASSERT(vkAccelStructInfos);
    for (uint32_t k = 0; k < count; k++)
    {
        const DescriptorAccelerationStructureInfo& accelStructInfo = accelStructInfos[k];
        VkWriteDescriptorSetAccelerationStructureKHR& vkAccelStructInfo = vkAccelStructInfos[k];
        const auto vkAccelStruct = static_cast<VkAccelerationStructure*>(accelStructInfo.accelerationStructure.Get());
        vkAccelStructInfo = vk::WriteDescriptorSetAccelerationStructureKHR(1u, vkAccelStruct->getAccelerationStructure());
    }
}

void qhwi::VkDevice::updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSetWrite* writes, uint32_t numWrites)
{
    Device::updateDescriptorSet(descriptorSet, writes, numWrites);
    QHWI_ASSERT(descriptorSet->desc());
    QHWI_ASSERT(numWrites < MaxDescriptorSetWrites);
    auto dsDesc = descriptorSet->desc();
    auto vkDescSet = static_cast<VkDescriptorSet*>(descriptorSet);
    auto vkDsDesc = vkDescSet->vkDesc();
    QHWI_ASSERT(dsDesc->layout && dsDesc->layout->desc());
    qre::StaticVector<vk::WriteDescriptorSet, MaxDescriptorSetWrites> descriptorSetWrites;
    auto vkLayoutDesc = static_cast<VkDescriptorSetLayout*>(dsDesc->layout.Get())->vkDesc();
    QHWI_ASSERT(vkLayoutDesc);

    for (uint32_t i = 0; i < numWrites; ++i)
    {
        auto& dsWrite = writes[i];
        VkDescriptorSetBinding* vkBindingData = vkDsDesc->findVkDescriptorSetBinding(dsWrite.type, dsWrite.shaderRegister);
        QHWI_ASSERT(vkBindingData);
        uint32_t count = dsWrite.count;
        vkBindingData->destroy();

        auto vkDsWrite = vk::WriteDescriptorSet(vkDescSet->get(),
                                                (vkLayoutDesc->base.getBindingSetOffset(dsWrite.type) + dsWrite.shaderRegister),
                                                0,
                                                count,
                                                qhwi::vk(dsWrite.type),
                                                nullptr,
                                                nullptr,
                                                nullptr);

        if ((dsWrite.type == DescriptorType::eConstantBuffer) || (dsWrite.type == DescriptorType::eRWStructuredBuffer) || (dsWrite.type == DescriptorType::eStructuredBuffer) ||
            (dsWrite.type == DescriptorType::eByteAddressBuffer) || (dsWrite.type == DescriptorType::eRWByteAddressBuffer))
        {
            vkBindingData->initAsBufferInfos(count);
            qhwi::toVkBufferInfos(dsWrite.bufferInfos, vkBindingData->bufferInfos, count);
            vkDsWrite.pBufferInfo = vkBindingData->bufferInfos;
        }
        else if (dsWrite.type == DescriptorType::eTexture || dsWrite.type == DescriptorType::eInputAttachment || dsWrite.type == DescriptorType::eRWTexture)
        {
            vkBindingData->initAsImageInfos(count, true);
            vk::ImageLayout imageLayout;
            if (dsWrite.type == DescriptorType::eInputAttachment && supportsDynamicRenderingLocalRead())
            {
                imageLayout = vk::ImageLayout::eRenderingLocalReadKHR;
            }
            else
            {
                imageLayout = (dsWrite.type == DescriptorType::eRWTexture) ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
            }
            qhwi::toVkImageInfos(dsWrite.textureInfos, vkBindingData->imageInfos, vkBindingData->imageViews, imageLayout, count);
            vkDsWrite.pImageInfo = vkBindingData->imageInfos;
        }
        else if (dsWrite.type == DescriptorType::eAccelerationStructure)
        {
            vkBindingData->initAsAccelStructInfos(count);
            qhwi::toVkAccelStructInfos(dsWrite.accelStructInfos, vkBindingData->accelStructInfos, count);
            vkDsWrite.pNext = vkBindingData->accelStructInfos;
        }
        else if (dsWrite.type == DescriptorType::eSampler)
        {
            vkBindingData->initAsImageInfos(count);
            qhwi::toVkImageInfos(dsWrite.samplerInfos, vkBindingData->imageInfos, count);
            vkDsWrite.pImageInfo = vkBindingData->imageInfos;
        }

        descriptorSetWrites.push_back(vkDsWrite);
    }

    QHWI_ASSERT(numWrites == descriptorSetWrites.size());
    m_device->updateDescriptorSets(uint32_t(descriptorSetWrites.size()), descriptorSetWrites.data(), 0, nullptr);
}

qre::RefCountPtr<qhwi::Shader> qhwi::VkDevice::createShader(const Shader::Description& desc, const void* byteCode, size_t length)
{
    return qre::MakeRefCountPtr<qhwi::Shader>(new VkShader(this, desc, byteCode, length));
}

qre::RefCountPtr<qhwi::CommandList> qhwi::VkDevice::createCommandList(CommandQueue* cmdQueue, const CommandList::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::CommandList>(new VkCommandList(this, desc));
}

qre::RefCountPtr<qhwi::CommandQueue> qhwi::VkDevice::createCommandQueue(const CommandQueue::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::CommandQueue>(new VkCommandQueue(this, desc));
}

void qhwi::VkDevice::setWindowSurface(const ext::WindowSurface& windowSurface)
{
    m_windowSurface.init(this, windowSurface);
}

qhwi::ext::Swapchain* qhwi::VkDevice::createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc)
{
    QHWI_ASSERT(commandQueue);
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkDevice::createSwapchain", QHWI_COLOR_RESOURCE_CREATE);
    return new ext::VkSwapchain(this, desc);
}

qhwi::ext::Swapchain* qhwi::VkDevice::createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc, const vk::SwapchainCreateInfoKHR& createInfo)
{
    QHWI_ASSERT(commandQueue);
    QHWI_PROFILE_SCOPE_NAMED_COLOR("VkDevice::createSwapchain", QHWI_COLOR_RESOURCE_CREATE);
    return new ext::VkSwapchain(this, desc, createInfo);
}

qre::RefCountPtr<qhwi::Fence> qhwi::VkDevice::createFence(FenceState initialState, const char* name)
{
    return qre::MakeRefCountPtr<qhwi::Fence>(new VkFence(this, initialState, name));
}

qre::RefCountPtr<qhwi::AccelerationStructure> qhwi::VkDevice::createAccelerationStructure(const AccelerationStructure::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::AccelerationStructure>(new VkAccelerationStructure(this, desc));
}

size_t qhwi::VkDevice::getBottomLevelScratchSize(size_t descriptorCount, AccelerationStructureBuildMode, AccelerationStructureBuildFlags)
{
    return (sizeof(vk::AccelerationStructureGeometryKHR) + sizeof(vk::AccelerationStructureBuildRangeInfoKHR)) * descriptorCount;
}

qhwi::AccelerationStructureBuildInfo qhwi::VkDevice::getAccelerationStructureBuildInfo(const BottomLevelDescriptor* bottomLevelDescriptors,
                                                                                       size_t count,
                                                                                       void* scratchMemory,
                                                                                       AccelerationStructureBuildMode buildMode,
                                                                                       AccelerationStructureBuildFlags flags)
{
    if (!supportsRayTracing())
    {
        QHWI_FATAL("The device does not support ray tracing.");
    }

    if (count == 0)
    {
        return AccelerationStructureBuildInfo();
    }

    vk::AccelerationStructureGeometryKHR* descriptors = reinterpret_cast<vk::AccelerationStructureGeometryKHR*>(scratchMemory);
    uint32_t* maxPrimitiveCounts = reinterpret_cast<uint32_t*>(descriptors + count);

    for (size_t i = 0; i < count; i++)
    {
        vk::AccelerationStructureGeometryKHR& vkDescriptor(descriptors[i]);
        const BottomLevelDescriptor& inputDescriptor(bottomLevelDescriptors[i]);

        vkDescriptor = vk::AccelerationStructureGeometryKHR();
        vkDescriptor.geometryType = vk::GeometryTypeKHR::eTriangles;
        vkDescriptor.flags = vk::GeometryFlagBitsKHR::eOpaque;
        vkDescriptor.geometry.triangles.transformData = vk::DeviceOrHostAddressConstKHR();
        vkDescriptor.geometry.triangles.maxVertex = (inputDescriptor.triangles.vertexCount == 0) ? 0 : inputDescriptor.triangles.vertexCount - 1;
        vkDescriptor.geometry.triangles.vertexFormat = vk(inputDescriptor.triangles.vertexFormat);
        vkDescriptor.geometry.triangles.vertexData = static_cast<VkBuffer*>(inputDescriptor.triangles.vertexBuffer)->getDeviceAddress() + inputDescriptor.triangles.vertexOffset;
        vkDescriptor.geometry.triangles.vertexStride = inputDescriptor.triangles.vertexStride;

        if (inputDescriptor.triangles.indexBuffer)
        {
            vkDescriptor.geometry.triangles.indexType = vk(inputDescriptor.triangles.indexFormat);
            vkDescriptor.geometry.triangles.indexData = static_cast<VkBuffer*>(inputDescriptor.triangles.indexBuffer)->getDeviceAddress() + inputDescriptor.triangles.indexOffset;

            maxPrimitiveCounts[i] = inputDescriptor.triangles.indexCount / 3;
        }
        else
        {
            maxPrimitiveCounts[i] = inputDescriptor.triangles.vertexCount / 3;
        }
    }

    vk::AccelerationStructureBuildGeometryInfoKHR buildInput;
    buildInput.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    buildInput.mode = qhwi::vk(buildMode);
    buildInput.flags = qhwi::vkBuildAccelerationStructureFlags(flags);
    buildInput.pGeometries = descriptors;
    buildInput.geometryCount = uint32_t(count);

    vk::AccelerationStructureBuildSizesInfoKHR prebuildInfo;
    prebuildInfo = m_device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                                   buildInput,
                                                                   vk::ArrayProxyNoTemporaries(uint32_t(count), maxPrimitiveCounts));

    qhwi::AccelerationStructureBuildInfo buildInfo;

    buildInfo.accelerationStructureSize = prebuildInfo.accelerationStructureSize;
    buildInfo.buildScratchSize = prebuildInfo.buildScratchSize;
    buildInfo.updateScratchSize = prebuildInfo.updateScratchSize;

    return buildInfo;
}

qre::RefCountPtr<qhwi::Buffer> qhwi::VkDevice::createTopLevelInstanceBuffer(const TopLevelDescriptor* topLevelDescriptors, AccelerationStructure** blasArray, size_t count)
{
    QHWI_ASSERT(&blasArray);
    if (count == 0)
    {
        return qre::RefCountPtr<Buffer>();
    }

    Buffer::Description instanceBufferDesc;
    instanceBufferDesc.heapType = HeapType::eUpload;
    instanceBufferDesc.usage = BufferUsage_UnorderedAccess | BufferUsage_AccelerationStructureBuildSource;
    instanceBufferDesc.size = sizeof(vk::AccelerationStructureInstanceKHR) * count;
    instanceBufferDesc.name = "InstanceBuffer";

    qre::RefCountPtr<Buffer> instanceBuffer = createBuffer(instanceBufferDesc);
    QHWI_ASSERT(instanceBuffer.Get());

    vk::AccelerationStructureInstanceKHR* instanceDescriptors = (vk::AccelerationStructureInstanceKHR*)instanceBuffer->map();
    QHWI_ASSERT(instanceDescriptors);

    for (size_t i = 0; i < count; i++)
    {
        vk::AccelerationStructureInstanceKHR& vkDescriptor(instanceDescriptors[i]);
        const TopLevelDescriptor& inputDescriptor(topLevelDescriptors[i]);
        VkAccelerationStructure* blas = static_cast<VkAccelerationStructure*>(blasArray[inputDescriptor.blasIndex]);
        QHWI_ASSERT(blas);

        vkDescriptor.accelerationStructureReference = blas->getDeviceAddress();
        vkDescriptor.instanceCustomIndex = inputDescriptor.instanceID;
        vkDescriptor.instanceShaderBindingTableRecordOffset = inputDescriptor.instanceContributionToHitGroupIndex;
        vkDescriptor.mask = inputDescriptor.instanceMask;
        vkDescriptor.flags = inputDescriptor.flags;
        memcpy(vkDescriptor.transform.matrix.data(), inputDescriptor.transform, 12 * sizeof(float));
    }

    instanceBuffer->unmap();

    return instanceBuffer;
}

qhwi::AccelerationStructureBuildInfo qhwi::VkDevice::getAccelerationStructureBuildInfo(size_t instanceCount,
                                                                                       AccelerationStructureBuildMode buildMode,
                                                                                       AccelerationStructureBuildFlags flags)
{
    if (!supportsRayTracing())
    {
        QHWI_FATAL("The device does not support ray tracing.");
    }

    if (instanceCount == 0)
    {
        return AccelerationStructureBuildInfo();
    }

    vk::AccelerationStructureGeometryKHR descriptor;
    descriptor.geometryType = vk::GeometryTypeKHR::eInstances;
    descriptor.geometry.instances = vk::AccelerationStructureGeometryInstancesDataKHR();
    descriptor.geometry.instances.arrayOfPointers = false;
    descriptor.geometry.instances.data = vk::DeviceOrHostAddressConstKHR();

    vk::AccelerationStructureBuildGeometryInfoKHR buildInput;
    buildInput.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    buildInput.mode = qhwi::vk(buildMode);
    buildInput.flags = qhwi::vkBuildAccelerationStructureFlags(flags);
    buildInput.pGeometries = &descriptor;
    buildInput.geometryCount = 1;

    vk::AccelerationStructureBuildSizesInfoKHR prebuildInfo;
    uint32_t maxPrimitiveCounts[] = { uint32_t(instanceCount) };
    prebuildInfo = m_device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInput, maxPrimitiveCounts);

    qhwi::AccelerationStructureBuildInfo buildInfo;

    buildInfo.accelerationStructureSize = prebuildInfo.accelerationStructureSize;
    buildInfo.buildScratchSize = prebuildInfo.buildScratchSize;
    buildInfo.updateScratchSize = prebuildInfo.updateScratchSize;

    return buildInfo;
}

qhwi::ResultCode qhwi::VkDevice::getRayTracingShaderGroupHandles(const RayTracingPipeline* pipeline, uint32_t startGroup, uint32_t groupCount, size_t dstSize, void* dst) const
{
    QHWI_ASSERT(dstSize >= 0 && dst != nullptr);

    const VkRayTracingPipeline* vkPipeline = static_cast<const VkRayTracingPipeline*>(pipeline);

    vk::Result result = m_device->getRayTracingShaderGroupHandlesKHR(vkPipeline->get(), startGroup, groupCount, dstSize, dst);

    if (result != vk::Result::eSuccess)
    {
        QHWI_LOGW("Unable to get shader group handles");
        return ResultCode::eErrorOther;
    }

    return ResultCode::eSuccess;
}

qre::RefCountPtr<qhwi::TimerQuery> qhwi::VkDevice::createTimerQuery()
{
    int32_t startTimerIndex = m_timerQueryHeap.allocate() * 2;
    int32_t endTimerIndex = startTimerIndex + 1;

    return qre::MakeRefCountPtr<qhwi::TimerQuery>((startTimerIndex >= 0) ? new VkTimerQuery(&m_timerQueryHeap, startTimerIndex, endTimerIndex) : nullptr);
}

std::vector<vk::PhysicalDevice> qhwi::VkContext::enumeratePhysicalDevices1(const EnumeratePhysicalDeviceParams& params)
{
    auto devices = m_vkInstance->enumeratePhysicalDevices();
    // filter out devices that don't support required extensions
    auto it = devices.begin();
    const auto& meetsRequirementsCb = params.meetsRequirementsCb;
    if (meetsRequirementsCb)
    {
        while (it != devices.end())
        {
            qhwi::VkPhysicalDevice physicalDevice(*it);
            if (!meetsRequirementsCb(physicalDevice))
            {
                it = devices.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    QHWI_ASSERT(!devices.empty());
    const auto& compareOp = params.compareOp;
    if (compareOp)
    {
        std::sort(devices.begin(), devices.end(), compareOp);
    }
    return devices;
}

qhwi::VkContext::EnumeratePhysicalDeviceParams::EnumeratePhysicalDeviceParams()
{
    meetsRequirementsCb = [](VkPhysicalDevice& device) -> bool
    {
        auto requiredExtensions = VkPhysicalDevice::getDefaultRequiredExtensions();

        if (!device.supportsExtensions(requiredExtensions))
        {
            return false;
        }

        const auto& f = device.getVkFeaturesInfo();
        if (f.descriptorIndexing.shaderStorageBufferArrayNonUniformIndexing == 0)
        {
            return false;
        }
        if (f.timelineSemaphore.timelineSemaphore == 0)
        {
            return false;
        }
        return true;
    };

    compareOp = [](const vk::PhysicalDevice& v0, const vk::PhysicalDevice& v1) -> bool
    {
        auto t0 = rankDeviceType(v0.getProperties().deviceType);
        auto t1 = rankDeviceType(v1.getProperties().deviceType);
        return t0 < t1;
    };
}

int qhwi::VkContext::EnumeratePhysicalDeviceParams::rankDeviceType(vk::PhysicalDeviceType type)
{
    switch (type)
    {
    case vk::PhysicalDeviceType::eDiscreteGpu:
        return 0;
    case vk::PhysicalDeviceType::eIntegratedGpu:
        return 1;
    default:
        return 2;
    };
}

std::vector<qhwi::VkPhysicalDevice> qhwi::VkContext::enumeratePhysicalDevices(const EnumeratePhysicalDeviceParams& params)
{
    auto devices = enumeratePhysicalDevices1(params);
    std::vector<qhwi::VkPhysicalDevice> qhwiVkPhysicalDevices(devices.size());
    for (uint32_t j = 0; j < devices.size(); j++)
    {
        qhwiVkPhysicalDevices[j].init(devices[j]);
    }
    return qhwiVkPhysicalDevices;
}

qre::RefCountPtr<qhwi::Device> qhwi::VkContext::createDevice(const Device::Description& desc)
{
    auto devices = enumeratePhysicalDevices();
    return createDevice(devices[0], desc);
}

qre::RefCountPtr<qhwi::Device> qhwi::VkContext::createDevice(VkPhysicalDevice physicalDevice, const Device::Description& desc)
{
    return qre::MakeRefCountPtr<qhwi::Device>(new qhwi::VkDevice(this, m_vkInstance.get(), physicalDevice, desc));
}

bool qhwi::VkContext::supportsInstanceExtension(const std::span<const vk::ExtensionProperties>& extensions, const char* name)
{
    for (uint32_t j = 0; j < extensions.size(); j++)
    {
        if (0 == strcmp(extensions[j].extensionName, name))
        {
            return true;
        }
    }
    return false;
}

bool qhwi::VkContext::supportsInstanceExtension(const char* name) const
{
    return supportsInstanceExtension(m_supportedInstanceExtensions, name);
}

bool qhwi::VkContext::supportsInstanceExtensions(const std::span<const vk::ExtensionProperties>& extensions, const std::span<const char* const>& names)
{
    for (const char* name : names)
    {
        if (!supportsInstanceExtension(extensions, name))
        {
            return false;
        }
    }
    return true;
}

bool qhwi::VkContext::isInstanceExtensionEnabled(const char* name) const
{
    return contains(m_enabledInstanceExtensions, name);
}

bool qhwi::VkContext::supportsInstanceLayer(const std::span<const vk::LayerProperties>& layers, const char* name)
{
    for (uint32_t j = 0; j < layers.size(); j++)
    {
        if (0 == strcmp(layers[j].layerName, name))
        {
            return true;
        }
    }
    return false;
}

bool qhwi::VkContext::supportsInstanceLayer(const char* name) const
{
    return supportsInstanceLayer(m_supportedInstanceLayers, name);
}

bool qhwi::VkContext::supportsInstanceLayers(const std::span<const vk::LayerProperties>& layers, const std::span<const char* const>& names)
{
    for (const char* name : names)
    {
        if (!supportsInstanceLayer(layers, name))
        {
            return false;
        }
    }
    return true;
}

bool qhwi::VkContext::isInstanceLayerEnabled(const char* name) const
{
    return contains(m_enabledInstanceLayers, name);
}

qhwi::VkContext::VkContext(const Description& desc)
    : m_desc(desc)
{
    static const vk::detail::DynamicLoader dl;
    const PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    vk::ApplicationInfo appInfo("", 1, "", 1, MinVkApiVersion);

    uint32_t numSupportedInstanceExtensions = 0;
    V(vk::enumerateInstanceExtensionProperties(nullptr, &numSupportedInstanceExtensions, nullptr));
    if (numSupportedInstanceExtensions > MaxVkInstanceExtensions)
    {
        QHWI_FATAL("# supported extensions (%d) > max instance extensions (%d)", numSupportedInstanceExtensions, MaxVkInstanceExtensions);
    }
    m_supportedInstanceExtensions.resize(numSupportedInstanceExtensions);
    V(vk::enumerateInstanceExtensionProperties(nullptr, &numSupportedInstanceExtensions, m_supportedInstanceExtensions.data()));

    uint32_t numSupportedInstanceLayers = 0;
    V(vk::enumerateInstanceLayerProperties(&numSupportedInstanceLayers, nullptr));
    if (numSupportedInstanceLayers > MaxVkInstanceLayers)
    {
        QHWI_FATAL("# supported instance layers (%d) > max instance layers (%d)", numSupportedInstanceLayers, MaxVkInstanceLayers);
    }
    m_supportedInstanceLayers.resize(numSupportedInstanceLayers);
    V(vk::enumerateInstanceLayerProperties(&numSupportedInstanceLayers, m_supportedInstanceLayers.data()));

    qre::StaticVector<const char*, MaxVkInstanceLayers> layers;
    auto& extensions = m_enabledInstanceExtensions;
    extensions = VkContext::getDefaultRequiredInstanceExtensions();
    auto optionalInstanceExtensions = VkContext::getDefaultOptionalInstanceExtensions();
    for (const char* ext : optionalInstanceExtensions)
    {
        if (supportsInstanceExtension(ext))
        {
            extensions.push_back(ext);
        }
    }
    bool debug = m_desc.base.debug;
    const char* VK_LAYER_KHRONOS_VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
    const char* VK_LAYER_API_DUMP_NAME = "VK_LAYER_LUNARG_api_dump";
    if (debug && !supportsInstanceLayer(VK_LAYER_KHRONOS_VALIDATION_LAYER_NAME))
    {
        QHWI_LOGW("VALIDATION LAYER NOT FOUND.  DISABLING VALIDATION");
        debug = false;
    }
    vk::ValidationFeaturesEXT validationFeatures;
    std::array<vk::ValidationFeatureEnableEXT, 2> validationFeaturesEnable = { vk::ValidationFeatureEnableEXT::eDebugPrintf,
                                                                               vk::ValidationFeatureEnableEXT::eSynchronizationValidation };
    std::array<vk::ValidationFeatureDisableEXT, 1> validationFeaturesDisable = { vk::ValidationFeatureDisableEXT::eUniqueHandles };
    if (debug)
    {
        layers.push_back(VK_LAYER_KHRONOS_VALIDATION_LAYER_NAME);
        validationFeatures = vk::ValidationFeaturesEXT(validationFeaturesEnable, validationFeaturesDisable);
    }
    bool apiDump = m_desc.base.apiDump;
    if (apiDump && !supportsInstanceLayer(VK_LAYER_API_DUMP_NAME))
    {
        QHWI_LOGW("API DUMP LAYER NOT FOUND.  DISABLING API DUMP");
        apiDump = false;
    }
    if (apiDump)
    {
        layers.push_back(VK_LAYER_API_DUMP_NAME);
    }
    m_debugUtilsEnabled = contains(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    vk::InstanceCreateInfo createInfo({}, &appInfo, uint32_t(layers.size()), layers.data(), uint32_t(extensions.size()), extensions.data(), debug ? &validationFeatures : nullptr);
    m_vkInstance = vk::createInstanceUnique(createInfo);
    QHWI_ASSERT(m_vkInstance);
    for (uint32_t j = 0; j < extensions.size(); j++)
    {
        QHWI_LOGI("Vulkan instance extension: %s enabled", extensions[j]);
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_vkInstance);

    //if (debug)
    {
        m_vkDebugUtilsMessenger.init(m_vkInstance.get(), m_desc.vk.debugUtilsMessengerCreateInfo);
    }
    m_vulkanFunctions.vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    m_vulkanFunctions.vkGetDeviceProcAddr = dl.getProcAddress<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
}

qhwi::VkContext::Description::Description(const Context::Description& desc)
    : base(desc)
{
    vk.debugUtilsMessengerCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT(vk::DebugUtilsMessengerCreateFlagsEXT(),
                                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                                                                            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
                                           .setPfnUserCallback(debugUtilsMessengerCallback);
}

void qhwi::VkDebugUtilsMessenger::init(vk::Instance vkInstance, const vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
{
    m_vkDebugUtilsMessenger = vkInstance.createDebugUtilsMessengerEXTUnique(createInfo, nullptr);
}

vk::PushConstantRange qhwi::vk(const PushConstantRange& p)
{
    return vk::PushConstantRange(qhwi::vkShaderStageFlags(p.shaderStages), p.offset, p.size);
}

vk::UniqueRenderPass qhwi::VkRenderPassFallback::createRenderPass(qhwi::VkDevice* device, const RenderPassDesc& p)
{
    bool hasDepthStencil = (p.depthStencilAttachment.format != vk::Format::eUndefined);
    uint32_t numColorAttachments = uint32_t(p.colorAttachments.size());

    vk::AttachmentDescription2KHR attachments[MaxAttachments];
    vk::AttachmentReference2KHR colorAttachmentRefs[MaxColorAttachments];
    vk::AttachmentReference2KHR colorResolveAttachmentRefs[MaxColorAttachments];
    vk::AttachmentReference2KHR depthStencilAttachmentRef;
    vk::AttachmentReference2KHR depthStencilResolveAttachmentRef;
    vk::SubpassDescriptionDepthStencilResolveKHR depthStencilResolve;

    uint32_t numAttachments = 0;
    for (uint32_t j = 0; j < numColorAttachments; ++j)
    {
        uint32_t attachmentIndex = numAttachments++;
        vk::AttachmentDescription2KHR& colorAttachmentDesc = attachments[attachmentIndex];
        colorAttachmentDesc.format = p.colorAttachments[j].format;
        colorAttachmentDesc.samples = p.sampleCount;
        colorAttachmentDesc.loadOp = p.colorAttachments[j].loadOp;
        colorAttachmentDesc.storeOp = p.colorAttachments[j].storeOp;
        colorAttachmentDesc.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachmentDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

        colorAttachmentRefs[j] = vk::AttachmentReference2KHR(attachmentIndex, colorAttachmentDesc.finalLayout);

        if (p.colorAttachments[j].resolveMode != vk::ResolveModeFlagBits::eNone)
        {
            attachmentIndex = numAttachments++;
            vk::AttachmentDescription2KHR& resolveAttachmentDesc = attachments[attachmentIndex];
            resolveAttachmentDesc.format = colorAttachmentDesc.format;
            resolveAttachmentDesc.samples = vk::SampleCountFlagBits::e1;
            resolveAttachmentDesc.loadOp = vk::AttachmentLoadOp::eDontCare;
            resolveAttachmentDesc.storeOp = vk::AttachmentStoreOp::eDontCare;
            resolveAttachmentDesc.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
            resolveAttachmentDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

            colorResolveAttachmentRefs[j] = vk::AttachmentReference2KHR(attachmentIndex, colorAttachmentDesc.finalLayout);
        }
        else
        {
            colorResolveAttachmentRefs[j] = vk::AttachmentReference2KHR(VK_ATTACHMENT_UNUSED);
        }
    }

    constexpr uint32_t numSubpasses = 1;
    vk::SubpassDescription2KHR subpasses[numSubpasses];

    subpasses[0].flags |= p.customResolve ? vk::SubpassDescriptionFlagBits::eCustomResolveEXT : static_cast<vk::SubpassDescriptionFlagBits>(0);
    subpasses[0].colorAttachmentCount = numColorAttachments;
    subpasses[0].pColorAttachments = colorAttachmentRefs;
    subpasses[0].pDepthStencilAttachment = hasDepthStencil ? &depthStencilAttachmentRef : nullptr;
    subpasses[0].pResolveAttachments = colorResolveAttachmentRefs;

    uint32_t numDependencies = 1;
    vk::SubpassDependency2KHR dependencies[2];

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eNone;
    dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    if (hasDepthStencil)
    {
        uint32_t attachmentIndex = numAttachments++;
        vk::AttachmentDescription2KHR& vkAttachmentDesc = attachments[attachmentIndex];

        vkAttachmentDesc.format = p.depthStencilAttachment.format;
        vkAttachmentDesc.samples = p.sampleCount;
        vkAttachmentDesc.loadOp = p.depthStencilAttachment.loadOp;
        vkAttachmentDesc.storeOp = p.depthStencilAttachment.storeOp;
        vkAttachmentDesc.stencilLoadOp = p.stencilLoadOp;
        vkAttachmentDesc.stencilStoreOp = p.stencilStoreOp;
        vkAttachmentDesc.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        vkAttachmentDesc.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        depthStencilAttachmentRef = vk::AttachmentReference2KHR(attachmentIndex, vkAttachmentDesc.finalLayout);

        ++numDependencies;
        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].dstSubpass = 0;
        dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
        dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
        dependencies[1].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        dependencies[1].dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

        vk::ResolveModeFlagBits depthResolveMode = p.depthStencilAttachment.resolveMode;
        vk::ResolveModeFlagBits stencilResolveMode = p.stencilResolveMode;
        if (depthResolveMode != vk::ResolveModeFlagBits::eNone || stencilResolveMode != vk::ResolveModeFlagBits::eNone)
        {
            attachmentIndex = numAttachments++;
            depthStencilResolveAttachmentRef = vk::AttachmentReference2KHR(attachmentIndex, vkAttachmentDesc.finalLayout);

            depthStencilResolve.depthResolveMode = depthResolveMode;
            depthStencilResolve.stencilResolveMode = stencilResolveMode;
            depthStencilResolve.pDepthStencilResolveAttachment = &depthStencilResolveAttachmentRef;

            subpasses[0].setPNext(&depthStencilResolve);
        }
    }

    uint32_t correlationMask = 0;
    bool multiView = (p.numViews != 0);
    if (multiView)
    {
        uint32_t viewMask = (1 << p.numViews) - 1;
        correlationMask = viewMask;
        subpasses[0].viewMask = viewMask;
    }
    auto rpCreateInfo = vk::RenderPassCreateInfo2KHR({},
                                                     numAttachments,
                                                     attachments,
                                                     numSubpasses,
                                                     subpasses,
                                                     numDependencies,
                                                     dependencies,
                                                     multiView ? 1 : 0,
                                                     multiView ? &correlationMask : nullptr);
    return device->get().createRenderPass2KHRUnique(rpCreateInfo);
}

vk::UniqueFramebuffer qhwi::VkRenderPassFallback::createFramebuffer(vk::Device vkDevice, const FramebufferDesc& p)
{
    QHWI_ASSERT(p.colorAttachments != nullptr || p.depthStencilAttachment != nullptr);

    uint32_t numAttachments = 0;
    vk::ImageView attachments[MaxAttachments];
    for (uint32_t j = 0; j < p.numColorAttachments; j++)
    {
        attachments[numAttachments++] = vkImageView(p.colorAttachments[j].textureData);
        if (p.colorAttachments[j].resolveTarget.texture != nullptr)
        {
            attachments[numAttachments++] = vkImageView(p.colorAttachments[j].resolveTarget);
        }
    }
    if (p.depthStencilAttachment)
    {
        attachments[numAttachments++] = vkImageView(p.depthStencilAttachment->textureData);
        if (p.depthStencilAttachment->depthResolveMode != ResolveMode::eNone || p.depthStencilAttachment->stencilResolveMode != ResolveMode::eNone)
        {
            attachments[numAttachments++] = vkImageView(p.depthStencilAttachment->resolveTarget);
        }
    }

    uint32_t width, height;
    if (p.depthStencilAttachment)
    {
        width = p.depthStencilAttachment->textureData.texture->desc()->w;
        height = p.depthStencilAttachment->textureData.texture->desc()->h;
    }
    else
    {
        width = p.colorAttachments[0].textureData.texture->desc()->w;
        height = p.colorAttachments[0].textureData.texture->desc()->h;
    }

    return vkDevice.createFramebufferUnique(vk::FramebufferCreateInfo({}, p.renderPass, numAttachments, attachments, width, height, 1));
}

vk::Extent3D qhwi::getExtent(const ColorAttachmentDescription& desc)
{
    return static_cast<VkTexture*>(desc.textureData.texture)->extent();
}

vk::Extent3D qhwi::getExtent(const DepthStencilAttachmentDescription& desc)
{
    return static_cast<VkTexture*>(desc.textureData.texture)->extent();
}

vk::ImageView qhwi::vkImageView(const TextureAttachmentData& textureData)
{
    VkTexture* vkTexture = static_cast<VkTexture*>(textureData.texture);

    SubresourceRange r = textureData.getSubresourceRange();
    r.clamp(vkTexture);
    vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange(vkImageAspectFlagsFromFormat(textureData.texture->desc()->format),
                                                                           r.start.mipSlice,
                                                                           r.end.mipSlice - r.start.mipSlice + 1,
                                                                           r.start.arraySlice,
                                                                           r.end.arraySlice - r.start.arraySlice + 1);

    vk::ImageViewType imageViewType = textureData.texture->desc()->type == TextureType::eCube ? vk::ImageViewType::e2DArray : vk(textureData.texture->desc()->type);

    return vkTexture->getImageView(imageViewType, &subresourceRange);
}

bool qhwi::supportsDepth(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32Sfloat:
    case vk::Format::eD32SfloatS8Uint:
        return true;
    default:
        return false;
    }
}

bool qhwi::supportsStencil(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
    case vk::Format::eS8Uint:
        return true;
    default:
        return false;
    }
}

vk::ImageAspectFlags qhwi::vkImageAspectFlagsFromFormat(qhwi::Format format)
{
    switch (format)
    {
    case Format::eD24UnormS8Uint:
    case Format::eD32FloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    case Format::eD16Unorm:
    case Format::eD32Float:
        return vk::ImageAspectFlagBits::eDepth;
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

bool qhwi::isReadOnlyAccess(vk::AccessFlags2KHR flags)
{
    const vk::AccessFlags2KHR mask = vk::AccessFlagBits2KHR::eShaderWrite | vk::AccessFlagBits2KHR::eColorAttachmentWrite | vk::AccessFlagBits2KHR::eDepthStencilAttachmentWrite |
                                     vk::AccessFlagBits2KHR::eTransferWrite | vk::AccessFlagBits2KHR::eHostWrite | vk::AccessFlagBits2KHR::eMemoryWrite;
    return ((flags & mask) == (vk::AccessFlags2KHR)0);
}

uint64_t qhwi::hash_util::Fnv1a64Continue(uint64_t& hash, const vk::ImageViewCreateInfo& v)
{
    qre::Fnv1a64Continue(hash, v.components.r);
    qre::Fnv1a64Continue(hash, v.components.g);
    qre::Fnv1a64Continue(hash, v.components.b);
    qre::Fnv1a64Continue(hash, v.components.a);
    qre::Fnv1a64Continue(hash, v.subresourceRange.aspectMask);
    qre::Fnv1a64Continue(hash, v.subresourceRange.baseArrayLayer);
    qre::Fnv1a64Continue(hash, v.subresourceRange.baseMipLevel);
    qre::Fnv1a64Continue(hash, v.subresourceRange.layerCount);
    qre::Fnv1a64Continue(hash, v.subresourceRange.levelCount);
    qre::Fnv1a64Continue(hash, v.flags);
    qre::Fnv1a64Continue(hash, v.format);
    qre::Fnv1a64Continue(hash, v.image);
    qre::Fnv1a64Continue(hash, v.viewType);
    qre::Fnv1a64Continue(hash, v.sType);

    return hash;
}

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
qre::RefCountPtr<qhwi::compressedstorage::File> qhwi::VkDevice::createStorageFile()
{
    return qre::MakeRefCountPtr<qhwi::compressedstorage::File>(new qhwi::compressedstorage::VkFile());
}

qhwi::ResultCode qhwi::compressedstorage::VkFactory::openFile(const char* fileToLoad, File* file) const
{
    QHWI_ASSERT_MSG(file != nullptr, "Invalid file");
    return static_cast<VkFile*>(file)->map(fileToLoad) ? qhwi::ResultCode::eSuccess : qhwi::ResultCode::eErrorOther;
}

qre::RefCountPtr<qhwi::compressedstorage::StorageStatusArray> qhwi::compressedstorage::VkFactory::createStatusArray(uint32_t capacity, const char* name) const
{
    return qre::MakeRefCountPtr<StorageStatusArray>(new VkStorageStatusArray());
};

bool qhwi::compressedstorage::VkFile::map(const char* fileToLoad)
{
    QHWI_ASSERT_MSG(fileToLoad != nullptr, "Invalid file");

    size_t size = 0;
    void* data = qre::OsFileMap(fileToLoad, &size);

    if (data == nullptr)
    {
        QHWI_LOGW("Failed to map file: %s", fileToLoad);
        return false;
    }

    m_fileMap = { static_cast<unsigned char*>(data), size };

    if (size == 0)
    {
        QHWI_LOGW("File is empty: %s", fileToLoad);
        m_fileMap = qre::UniqueFileMap<unsigned char>();
        return false;
    }

    return true;
}

void qhwi::compressedstorage::VkStorageQueue::init()
{
    QHWI_ASSERT_MSG(m_device != nullptr, "Device cannot be nullptr");

    m_commandQueue = qre::MakeRefCountPtr<qhwi::CommandQueue>(new VkCommandQueue(m_device, CommandQueue::Description()));
}

void qhwi::compressedstorage::VkStorageQueue::submit()
{
    if (m_isRecording)
    {
        m_isRecording = false;
        m_commandList->end();
        m_commandQueue->executeCommandList(m_commandList, m_fence);
        m_numEnqueuedRequests = 0;

        // Process requests requiring memcpy to host memory
        for (const VkRequest& request : m_requests)
        {
            if (!request.copyToHostMemory)
            {
                continue;
            }

            m_commandQueue->waitForIdle();

            QHWI_ASSERT(request.destinationType == StorageRequestDestinationType::eMemory);
            QHWI_ASSERT(request.transferBuffer != nullptr);
            const DestinationMemory& dst = std::get<DestinationMemory>(request.destination);
            QHWI_ASSERT(dst.size >= request.uncompressedSize);
            const uint8_t* transferSrc = static_cast<const uint8_t*>(request.transferBuffer->map());
            memcpy(dst.destinationMemoryPtr, transferSrc, request.uncompressedSize);
            request.transferBuffer->unmap();
        }
    }
}

void qhwi::compressedstorage::VkStorageQueue::enqueueRequest(const Request& request)
{
    QHWI_ASSERT_MSG(m_commandQueue != nullptr, "Command queue cannot be nullptr");

    if (!m_isRecording)
    {
        m_commandList = m_device->createCommandList(nullptr, {});
        m_commandList->begin();
        m_isRecording = true;
        m_numEnqueuedRequests = 0;
        m_requests.clear();
    }

    std::span<const uint8_t> data;
    VkRequest& vkRequest = m_requests.emplace_back(request);

    switch (vkRequest.sourceType)
    {
    case qhwi::compressedstorage::StorageRequestSourceType::eFile:
    {
        const SourceFile& srcFile = std::get<SourceFile>(vkRequest.source);
        FileInfo fileSize;
        srcFile.file->getFileInformation(fileSize);
        QHWI_ASSERT(srcFile.offset + srcFile.size <= fileSize.sizeInBytes);
        data = { static_cast<const VkFile*>(srcFile.file)->data() + srcFile.offset, srcFile.size };
    }
    break;

    case qhwi::compressedstorage::StorageRequestSourceType::eMemory:
    {
        const SourceMemory& srcMemory = std::get<SourceMemory>(vkRequest.source);
        data = { static_cast<const uint8_t*>(srcMemory.sourceMemoryPtr), srcMemory.size };
    }
    break;

    default:
        QHWI_FATAL("Invalid request source type");
    }

    switch (vkRequest.destinationType)
    {
    case qhwi::compressedstorage::StorageRequestDestinationType::eMemory:
        enqueueMemToMem(data, std::get<qhwi::compressedstorage::DestinationMemory>(vkRequest.destination), vkRequest);
        break;

    case qhwi::compressedstorage::StorageRequestDestinationType::eBuffer:
        enqueueMemToBuffer(data, std::get<qhwi::compressedstorage::DestinationBuffer>(vkRequest.destination), vkRequest);
        break;

    case qhwi::compressedstorage::StorageRequestDestinationType::eTextureRegion:
        enqueueMemToTextureRegion(data, std::get<qhwi::compressedstorage::DestinationTextureRegion>(vkRequest.destination), vkRequest);
        break;

    case qhwi::compressedstorage::StorageRequestDestinationType::eMutipleSubresources:
        enqueueMemToMultipleSubresources(data, std::get<qhwi::compressedstorage::DestinationMultipleSubresources>(vkRequest.destination), vkRequest);
        break;

    case qhwi::compressedstorage::StorageRequestDestinationType::eTiles:
        QHWI_TODO();
        break;

    default:
        QHWI_FATAL("Invalid request destination type");
    }

    ++m_numEnqueuedRequests;
}

void qhwi::compressedstorage::VkStorageQueue::readTileStreamHeader(std::span<const uint8_t> tileStream,
                                                                   uint32_t& numTiles,
                                                                   const uint32_t*& tileOffsetStart,
                                                                   std::span<const uint8_t>& stream) const
{
    constexpr size_t TileStreamSize = sizeof(GDeflate::TileStream);

    numTiles = reinterpret_cast<const GDeflate::TileStream*>(tileStream.data())->numTiles;
    QHWI_ASSERT(numTiles > 0);
    tileOffsetStart = reinterpret_cast<const uint32_t*>(tileStream.data() + TileStreamSize);
    stream = { reinterpret_cast<const uint8_t*>(tileOffsetStart + numTiles), tileStream.size() - (TileStreamSize + sizeof(uint32_t) * numTiles) };
}

void qhwi::compressedstorage::VkStorageQueue::enqueueMemToMem(std::span<const uint8_t> src, const DestinationMemory& dst, VkRequest& vkRequest)
{
    switch (vkRequest.compressionFormat)
    {
    case CompressionFormat::eNone:
    {
        size_t srcSize = src.size();
        QHWI_ASSERT_MSG(srcSize == vkRequest.uncompressedSize, "Source size and uncompressed size must be same with no compression");
        QHWI_ASSERT(dst.size >= vkRequest.uncompressedSize);
        QHWI_ASSERT(dst.size >= srcSize);
        memcpy(dst.destinationMemoryPtr, src.data(), srcSize);
    }
    break;

    case CompressionFormat::eGDeflate:
    {
        uint32_t numTiles;
        std::span<const uint8_t> srcStream;
        const uint32_t* tileOffsetStart;

        readTileStreamHeader(src, numTiles, tileOffsetStart, srcStream);

        vkRequest.copyToHostMemory = true;
        vkRequest.parameterBuffer = m_device->createBuffer(qhwi::Buffer::Description()
                                                               .setHeapType(HeapType::eDefault)
                                                               .setUsage(BufferUsage_UnorderedAccess | BufferUsage_IndirectBuffer)
                                                               .setSize(sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT)));

        vkRequest.intermediateSrc = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(srcStream.size()));

        vkRequest.intermediateDst = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(vkRequest.uncompressedSize));

        vkRequest.transferBuffer = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDownload).setSize(vkRequest.uncompressedSize));

        const size_t paramBufferSize = sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT);
        std::unique_ptr<uint8_t[]> paramBuffer = std::make_unique<uint8_t[]>(paramBufferSize);

        uint8_t* paramBufferPtr = paramBuffer.get();
        *reinterpret_cast<VkDeviceSize*>(paramBufferPtr) = numTiles;
        paramBufferPtr += sizeof(VkDeviceSize);

        for (uint32_t tileIndex = 0; tileIndex < numTiles; ++tileIndex)
        {
            uint32_t tileOffset = (tileIndex > 0) ? tileOffsetStart[tileIndex] : 0;

            vk::DecompressMemoryRegionEXT& region = reinterpret_cast<vk::DecompressMemoryRegionEXT*>(paramBufferPtr)[tileIndex];
            region = vk::DecompressMemoryRegionEXT();
            region.compressedSize = (tileIndex < (numTiles - 1)) ? tileOffsetStart[tileIndex + 1] - tileOffset : tileOffsetStart[0];
            region.decompressedSize = (tileIndex < (numTiles - 1)) ?
                                          GDeflate::kDefaultTileSize :
                                          static_cast<uint32_t>(GDeflate::kDefaultTileSize - static_cast<uint64_t>(tileIndex) * GDeflate::kDefaultTileSize);
            ;
            region.dstAddress = vkRequest.intermediateDst.Get()->getDeviceAddress() + tileIndex * GDeflate::kDefaultTileSize;
            region.srcAddress = vkRequest.intermediateSrc.Get()->getDeviceAddress() + ((tileIndex == 0) ? 0 : tileOffsetStart[tileIndex]);
        }

        m_commandList->beginTransferPass();

        // Update source buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.intermediateSrc, 0, srcStream.data(), srcStream.size());
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_Transfer_Dst, .dst = BufferState_ComputeShader_StorageRead });

        // Update parameter buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.parameterBuffer, 0, paramBuffer.get(), paramBufferSize);
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_Transfer_Dst, .dst = BufferState_DispatchIndirect_CommandBufferRead });

        m_commandList->endTransferPass();

        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateDst, .src = BufferState_NoAccess, .dst = BufferState_ComputeShader_StorageWrite });

        static_cast<VkCommandList*>(m_commandList.Get())
            ->get()
            .decompressMemoryIndirectCountEXT(vk::MemoryDecompressionMethodFlagBitsEXT::eGdeflate10,
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress() + sizeof(VkDeviceSize),
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress(),
                                              numTiles,
                                              sizeof(vk::DecompressMemoryRegionEXT));

        // Copy to host visible buffer
        m_commandList->beginTransferPass();
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateDst, .src = BufferState_ComputeShader_StorageWrite, .dst = BufferState_Transfer_Src });
        m_commandList->addBufferBarrier({ .buffer = vkRequest.transferBuffer, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->copyBuffer(vkRequest.transferBuffer, vkRequest.intermediateDst);
        m_commandList->addBufferBarrier({ .buffer = vkRequest.transferBuffer, .src = BufferState_Transfer_Dst, .dst = BufferState_Transfer_Src });
        m_commandList->endTransferPass();
    }
    break;

    default:
        QHWI_FATAL("Invalid compression format");
    }
}

void qhwi::compressedstorage::VkStorageQueue::enqueueMemToBuffer(std::span<const uint8_t> src, const DestinationBuffer& dst, VkRequest& vkRequest)
{
    QHWI_ASSERT(dst.size >= vkRequest.uncompressedSize);

    switch (vkRequest.compressionFormat)
    {
    case CompressionFormat::eNone:
    {
        size_t srcSize = src.size();
        QHWI_ASSERT_MSG(srcSize == vkRequest.uncompressedSize, "Source size and uncompressed size must be same with no compression");

        m_commandList->beginTransferPass();
        m_commandList->updateBuffer(dst.buffer, static_cast<uint32_t>(dst.offset), src.data(), src.size());
        m_commandList->endTransferPass();
    }
    break;

    case CompressionFormat::eGDeflate:
    {
        uint32_t numTiles;
        std::span<const uint8_t> srcStream;
        const uint32_t* tileOffsetStart;

        readTileStreamHeader(src, numTiles, tileOffsetStart, srcStream);

        vkRequest.parameterBuffer = m_device->createBuffer(qhwi::Buffer::Description()
                                                               .setHeapType(HeapType::eDefault)
                                                               .setUsage(BufferUsage_UnorderedAccess | BufferUsage_IndirectBuffer)
                                                               .setSize(sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT)));

        vkRequest.intermediateSrc = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(srcStream.size()));

        const size_t paramBufferSize = sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT);
        std::unique_ptr<uint8_t[]> paramBuffer = std::make_unique<uint8_t[]>(paramBufferSize);

        uint8_t* paramBufferPtr = paramBuffer.get();
        *reinterpret_cast<VkDeviceSize*>(paramBufferPtr) = numTiles;
        paramBufferPtr += sizeof(VkDeviceSize);

        for (uint32_t tileIndex = 0; tileIndex < numTiles; ++tileIndex)
        {
            uint32_t tileOffset = (tileIndex > 0) ? tileOffsetStart[tileIndex] : 0;

            vk::DecompressMemoryRegionEXT& region = reinterpret_cast<vk::DecompressMemoryRegionEXT*>(paramBufferPtr)[tileIndex];
            region = vk::DecompressMemoryRegionEXT();
            region.compressedSize = (tileIndex < (numTiles - 1)) ? tileOffsetStart[tileIndex + 1] - tileOffset : tileOffsetStart[0];
            region.decompressedSize = GDeflate::kDefaultTileSize;
            region.dstAddress = dst.buffer->getDeviceAddress() + dst.offset + tileIndex * GDeflate::kDefaultTileSize;
            region.srcAddress = vkRequest.intermediateSrc.Get()->getDeviceAddress() + ((tileIndex == 0) ? 0 : tileOffsetStart[tileIndex]);
        }

        m_commandList->beginTransferPass();

        // Update source buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.intermediateSrc, 0, srcStream.data(), srcStream.size());
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_Transfer_Dst, .dst = BufferState_ComputeShader_StorageRead });

        // Update parameter buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.parameterBuffer, 0, paramBuffer.get(), paramBufferSize);
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_Transfer_Dst, .dst = BufferState_DispatchIndirect_CommandBufferRead });

        m_commandList->endTransferPass();

        static_cast<VkCommandList*>(m_commandList.Get())
            ->get()
            .decompressMemoryIndirectCountEXT(vk::MemoryDecompressionMethodFlagBitsEXT::eGdeflate10,
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress() + sizeof(VkDeviceSize),
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress(),
                                              numTiles,
                                              sizeof(vk::DecompressMemoryRegionEXT));
    }
    break;

    default:
        QHWI_FATAL("Invalid compression format");
    }
}

void qhwi::compressedstorage::VkStorageQueue::enqueueMemToTextureRegion(std::span<const uint8_t> src, const DestinationTextureRegion& dst, VkRequest& vkRequest)
{
    QHWI_ASSERT(dst.texture != nullptr);
    QHWI_ASSERT(dst.origin.x == 0 && dst.origin.y == 0 && dst.origin.z == 0);

    const uint16_t mipLevels = static_cast<uint16_t>(dst.texture->desc()->mipLevels);
    const uint16_t arraySlice = static_cast<uint16_t>(dst.subresourceIndex / mipLevels);
    const uint16_t mipLevel = static_cast<uint16_t>(dst.subresourceIndex % mipLevels);
    const uint32_t mipWidth = std::max(1u, dst.texture->desc()->w >> mipLevel);
    const uint32_t mipHeight = std::max(1u, dst.texture->desc()->h >> mipLevel);
    const uint32_t mipDepth = std::max(1u, dst.texture->desc()->d >> mipLevel);
    const uint32_t srcRowPitch = qre::AlignUp(FormatUtil::getRowPitch(dst.texture->desc()->format, mipWidth), s_RowPitchAlignment);
    const uint32_t srcDepthPitch = srcRowPitch * mipHeight;
    const FormatUtil::FormatInfo formatInfo = FormatUtil::c_FormatInfo[(uint32_t)dst.texture->desc()->format];

    switch (vkRequest.compressionFormat)
    {
    case CompressionFormat::eNone:
    {
        m_commandList->updateTexture(dst.texture, arraySlice, mipLevel, src.data(), srcRowPitch, srcDepthPitch);
    }
    break;

    case CompressionFormat::eGDeflate:
    {
        uint32_t numTiles;
        std::span<const uint8_t> srcStream;
        const uint32_t* tileOffsetStart;

        readTileStreamHeader(src, numTiles, tileOffsetStart, srcStream);

        vkRequest.parameterBuffer = m_device->createBuffer(qhwi::Buffer::Description()
                                                               .setHeapType(HeapType::eDefault)
                                                               .setUsage(BufferUsage_UnorderedAccess | BufferUsage_IndirectBuffer)
                                                               .setSize(sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT)));

        vkRequest.intermediateSrc = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(srcStream.size()));

        vkRequest.intermediateDst = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(vkRequest.uncompressedSize));

        const size_t paramBufferSize = sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT);
        std::unique_ptr<uint8_t[]> paramBuffer = std::make_unique<uint8_t[]>(paramBufferSize);

        uint8_t* paramBufferPtr = paramBuffer.get();
        *reinterpret_cast<VkDeviceSize*>(paramBufferPtr) = numTiles;
        paramBufferPtr += sizeof(VkDeviceSize);

        for (uint32_t tileIndex = 0; tileIndex < numTiles; ++tileIndex)
        {
            uint32_t tileOffset = (tileIndex > 0) ? tileOffsetStart[tileIndex] : 0;

            vk::DecompressMemoryRegionEXT& region = reinterpret_cast<vk::DecompressMemoryRegionEXT*>(paramBufferPtr)[tileIndex];
            region = vk::DecompressMemoryRegionEXT();
            region.compressedSize = (tileIndex < (numTiles - 1)) ? tileOffsetStart[tileIndex + 1] - tileOffset : tileOffsetStart[0];
            region.decompressedSize = GDeflate::kDefaultTileSize;
            region.dstAddress = vkRequest.intermediateDst->getDeviceAddress() + tileIndex * GDeflate::kDefaultTileSize;
            region.srcAddress = vkRequest.intermediateSrc.Get()->getDeviceAddress() + ((tileIndex == 0) ? 0 : tileOffsetStart[tileIndex]);
        }

        m_commandList->beginTransferPass();

        // Update source buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.intermediateSrc, 0, srcStream.data(), srcStream.size());
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_Transfer_Dst, .dst = BufferState_ComputeShader_StorageRead });

        // Update parameter buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.parameterBuffer, 0, paramBuffer.get(), paramBufferSize);
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_Transfer_Dst, .dst = BufferState_DispatchIndirect_CommandBufferRead });

        m_commandList->endTransferPass();

        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateDst, .src = BufferState_NoAccess, .dst = BufferState_ComputeShader_StorageWrite });

        static_cast<VkCommandList*>(m_commandList.Get())
            ->get()
            .decompressMemoryIndirectCountEXT(vk::MemoryDecompressionMethodFlagBitsEXT::eGdeflate10,
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress() + sizeof(VkDeviceSize),
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress(),
                                              numTiles,
                                              sizeof(vk::DecompressMemoryRegionEXT));

        m_commandList->beginTransferPass();
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateDst, .src = BufferState_ComputeShader_StorageWrite, .dst = BufferState_Transfer_Src });

        const size_t srcRowPitchUnaligned = (qre::AlignUp(mipWidth, formatInfo.blockWidth) / formatInfo.blockWidth) * static_cast<size_t>(formatInfo.bytesPerBlock);
        const size_t srcRowPitchAligned = qre::AlignUp(srcRowPitchUnaligned, s_RowPitchAlignment);

        vk::ImageSubresourceLayers subresourceLayers = vk::ImageSubresourceLayers()
                                                           .setBaseArrayLayer(arraySlice)
                                                           .setLayerCount(1)
                                                           .setMipLevel(mipLevel)
                                                           .setAspectMask(static_cast<VkTexture*>(dst.texture)->getImageAspectFlags());

        const size_t rowPitchInTexels = srcRowPitchUnaligned == srcRowPitchAligned ? 0 : srcRowPitchAligned * formatInfo.blockWidth / formatInfo.bytesPerBlock;

        vk::BufferImageCopy bufferImageCopy = vk::BufferImageCopy()
                                                  .setBufferOffset(0)
                                                  .setBufferRowLength(static_cast<uint32_t>(rowPitchInTexels))
                                                  .setBufferImageHeight(mipHeight * formatInfo.blockHeight)
                                                  .setImageSubresource(subresourceLayers)
                                                  .setImageOffset(vk::Offset3D().setX(0).setY(0).setZ(0))
                                                  .setImageExtent(vk::Extent3D().setWidth(mipWidth).setHeight(mipHeight).setDepth(mipDepth));

        vk::Buffer vkIntermediateDstBuffer = static_cast<VkBuffer*>(vkRequest.intermediateDst.Get())->get();
        vk::Image vkDstTexture = static_cast<VkTexture* const>(dst.texture)->getImage();
        vk::CommandBuffer vkCommandBuffer = static_cast<VkCommandList*>(m_commandList.Get())->get();

        vkCommandBuffer.copyBufferToImage(vkIntermediateDstBuffer, vkDstTexture, vk::ImageLayout::eTransferDstOptimal, 1, &bufferImageCopy);

        m_commandList->endTransferPass();
    }
    break;

    default:
        QHWI_FATAL("Invalid compression format");
    }
}

void qhwi::compressedstorage::VkStorageQueue::enqueueMemToMultipleSubresources(std::span<const uint8_t> src, const DestinationMultipleSubresources& dst, VkRequest& vkRequest)
{
    QHWI_ASSERT(dst.texture != nullptr);
    QHWI_ASSERT(dst.firstSubresource == 0);

    switch (vkRequest.compressionFormat)
    {
    case CompressionFormat::eNone:
    {
        const TextureDesc* desc = dst.texture->desc();
        const uint8_t* dataPtr = src.data();
        const FormatUtil::FormatInfo formatInfo = FormatUtil::c_FormatInfo[static_cast<uint32_t>(desc->format)];
        uint32_t mipWidth = desc->w, mipHeight = desc->h, mipDepth = desc->d, mipLevels = desc->mipLevels, arraySize = desc->arrayLayers;
        auto regions = std::vector<qhwi::UpdateTextureRegion>(mipLevels * arraySize);
        auto regionsIt = regions.data();

        for (unsigned int l = 0; l < mipLevels; ++l)
        {
            size_t rowPitch = qre::AlignUp((qre::AlignUp(mipWidth, formatInfo.blockWidth) / formatInfo.blockWidth) * size_t(formatInfo.bytesPerBlock), s_RowPitchAlignment);
            size_t depthPitch = qre::AlignUp((qre::AlignUp(mipHeight, formatInfo.blockHeight) / formatInfo.blockHeight) * rowPitch, s_DataPlacementAlignment);

            for (unsigned int a = 0; a < arraySize; ++a)
            {
                *regionsIt++ = { .arraySlice = a, .mipLevel = l, .srcData = dataPtr, .srcRowPitch = rowPitch, .srcDepthPitch = depthPitch };
                dataPtr += depthPitch * mipDepth;
            }

            mipWidth = qre::math::max(mipWidth >> 1u, 1u);
            mipHeight = qre::math::max(mipHeight >> 1u, 1u);
            mipDepth = qre::math::max(mipDepth >> 1u, 1u);
        }

        m_commandList->beginTransferPass();
        m_commandList->updateTexture(dst.texture, { regions.data(), regions.size() });
        m_commandList->endTransferPass();
    }
    break;

    case CompressionFormat::eGDeflate:
    {
        uint32_t numTiles;
        std::span<const uint8_t> srcStream;
        const uint32_t* tileOffsetStart;

        readTileStreamHeader(src, numTiles, tileOffsetStart, srcStream);

        vkRequest.parameterBuffer = m_device->createBuffer(qhwi::Buffer::Description()
                                                               .setHeapType(HeapType::eDefault)
                                                               .setUsage(BufferUsage_UnorderedAccess | BufferUsage_IndirectBuffer)
                                                               .setSize(sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT)));
        vkRequest.intermediateSrc = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(srcStream.size()));
        vkRequest.intermediateDst = m_device->createBuffer(qhwi::Buffer::Description().setHeapType(HeapType::eDefault).setSize(vkRequest.uncompressedSize));

        const size_t paramBufferSize = sizeof(VkDeviceSize) + numTiles * sizeof(vk::DecompressMemoryRegionEXT);
        std::unique_ptr<uint8_t[]> paramBuffer = std::make_unique<uint8_t[]>(paramBufferSize);

        uint8_t* paramBufferPtr = paramBuffer.get();
        *reinterpret_cast<VkDeviceSize*>(paramBufferPtr) = numTiles;
        paramBufferPtr += sizeof(VkDeviceSize);

        // Prepare argument buffer
        for (uint32_t tileIndex = 0; tileIndex < numTiles; ++tileIndex)
        {
            uint32_t tileOffset = (tileIndex > 0) ? tileOffsetStart[tileIndex] : 0;

            vk::DecompressMemoryRegionEXT& region = reinterpret_cast<vk::DecompressMemoryRegionEXT*>(paramBufferPtr)[tileIndex];
            region = vk::DecompressMemoryRegionEXT();
            region.compressedSize = (tileIndex < (numTiles - 1)) ? tileOffsetStart[tileIndex + 1] - tileOffset : tileOffsetStart[0];
            region.decompressedSize = GDeflate::kDefaultTileSize;
            region.dstAddress = vkRequest.intermediateDst->getDeviceAddress() + tileIndex * GDeflate::kDefaultTileSize;
            region.srcAddress = vkRequest.intermediateSrc.Get()->getDeviceAddress() + ((tileIndex == 0) ? 0 : tileOffsetStart[tileIndex]);
        }

        m_commandList->beginTransferPass();

        // Update source buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.intermediateSrc, 0, srcStream.data(), srcStream.size());
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateSrc, .src = BufferState_Transfer_Dst, .dst = BufferState_ComputeShader_StorageRead });

        // Update parameter buffer
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_NoAccess, .dst = BufferState_Transfer_Dst });
        m_commandList->updateBuffer(vkRequest.parameterBuffer, 0, paramBuffer.get(), paramBufferSize);
        m_commandList->addBufferBarrier({ .buffer = vkRequest.parameterBuffer, .src = BufferState_Transfer_Dst, .dst = BufferState_DispatchIndirect_CommandBufferRead });

        m_commandList->endTransferPass();

        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateDst, .src = BufferState_NoAccess, .dst = BufferState_ComputeShader_StorageWrite });
        static_cast<VkCommandList*>(m_commandList.Get())
            ->get()
            .decompressMemoryIndirectCountEXT(vk::MemoryDecompressionMethodFlagBitsEXT::eGdeflate10,
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress() + sizeof(VkDeviceSize),
                                              vkRequest.parameterBuffer.Get()->getDeviceAddress(),
                                              numTiles,
                                              sizeof(vk::DecompressMemoryRegionEXT));
        m_commandList->addBufferBarrier({ .buffer = vkRequest.intermediateDst, .src = BufferState_ComputeShader_StorageWrite, .dst = BufferState_Transfer_Src });

        // Copy decompressed data from buffer to texture
        const TextureDesc* desc = dst.texture->desc();
        const FormatUtil::FormatInfo formatInfo = FormatUtil::c_FormatInfo[static_cast<uint32_t>(desc->format)];
        uint32_t mipWidth = desc->w, mipHeight = desc->h, mipDepth = desc->d, mipLevels = desc->mipLevels, arraySize = desc->arrayLayers;
        std::vector<vk::BufferImageCopy> regions;
        regions.reserve(mipLevels);
        size_t srcLevelOffset = 0;

        m_commandList->beginTransferPass();

        for (uint32_t l = 0; l < mipLevels; ++l)
        {
            const size_t srcRowPitchUnaligned = (qre::AlignUp(mipWidth, formatInfo.blockWidth) / formatInfo.blockWidth) * size_t(formatInfo.bytesPerBlock);
            const size_t srcRowPitchAligned = qre::AlignUp(srcRowPitchUnaligned, s_RowPitchAlignment);
            const size_t depthPitch = qre::AlignUp((qre::AlignUp(mipHeight, formatInfo.blockHeight) / formatInfo.blockHeight) * srcRowPitchAligned, s_DataPlacementAlignment);

            vk::ImageSubresourceLayers subresourceLayers = vk::ImageSubresourceLayers().setBaseArrayLayer(0).setLayerCount(arraySize).setMipLevel(l).setAspectMask(
                static_cast<VkTexture*>(dst.texture)->getImageAspectFlags());

            const size_t rowPitchInTexels = srcRowPitchUnaligned == srcRowPitchAligned ? 0 : srcRowPitchAligned * formatInfo.blockWidth / formatInfo.bytesPerBlock;

            regions.emplace_back(vk::BufferImageCopy()
                                     .setBufferOffset(srcLevelOffset)
                                     .setBufferRowLength(static_cast<uint32_t>(rowPitchInTexels))
                                     .setBufferImageHeight(mipHeight * formatInfo.blockHeight)
                                     .setImageSubresource(subresourceLayers)
                                     .setImageOffset(vk::Offset3D().setX(0).setY(0).setZ(0))
                                     .setImageExtent(vk::Extent3D().setWidth(mipWidth).setHeight(mipHeight).setDepth(mipDepth)));

            srcLevelOffset += mipDepth * depthPitch;

            mipWidth = qre::math::max(mipWidth >> 1u, 1u);
            mipHeight = qre::math::max(mipHeight >> 1u, 1u);
            mipDepth = qre::math::max(mipDepth >> 1u, 1u);
        }

        vk::Buffer vkIntermediateDstBuffer = static_cast<VkBuffer*>(vkRequest.intermediateDst.Get())->get();
        vk::Image vkDstTexture = static_cast<VkTexture* const>(dst.texture)->getImage();
        vk::CommandBuffer vkCommandBuffer = static_cast<VkCommandList*>(m_commandList.Get())->get();

        vkCommandBuffer.copyBufferToImage(vkIntermediateDstBuffer, vkDstTexture, vk::ImageLayout::eTransferDstOptimal, static_cast<uint32_t>(regions.size()), regions.data());

        m_commandList->endTransferPass();
    }
    break;

    default:
        QHWI_FATAL("Invalid compression format");
    }
}
#endif // QHWI_ENABLE_COMPRESSED_STORAGE
