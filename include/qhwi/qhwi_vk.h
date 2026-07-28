//
//
//  qhwi_vk.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_VK_H__
#define __QHWI_VK_H__

#include <unordered_map>

#include <qhwi/qhwi.h>
#include <qhwi/qhwi_vk_config.h>

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#if defined(__clang__) || defined(__GNUC__)
//https://developer.apple.com/forums/thread/119299
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wnullability-completeness"
#    pragma clang diagnostic ignored "-Wunused-variable"
#    pragma clang diagnostic ignored "-Wunused-private-field"
#    include "vk_mem_alloc.h"
#    pragma clang diagnostic pop
#else
#    pragma warning(push)
#    pragma warning(disable : 4189)
#    pragma warning(disable : 4127)
#    pragma warning(disable : 4324)
#    include "vk_mem_alloc.h"
#    pragma warning(pop)
#endif

#include <qhwi/qhwi_vk_enums.h>
#include <qhwi/qhwi_vk_render_pass_encoder.h>
#include <qre/memory/StaticQueue.h>

namespace qhwi
{
const int MaxVkDeviceLayers = 1024;
const int MaxVkDeviceExtensions = 1024;
const int MaxVkInstanceLayers = 1024;
const int MaxVkInstanceExtensions = 1024;
const int MaxVkQueueFamilyProperties = 32;
const int MaxVkImageViews = MaxShaderResourceViews;
const int MaxVkRenderPasses = 1024;
const uint32_t MinVkApiVersion = VK_API_VERSION_1_1;
#if defined(QHWI_PLATFORM_WINDOWS)
using VkSharedHandleType = HANDLE;
constexpr VkSharedHandleType VkNullSharedHandle = nullptr;
#else
using VkSharedHandleType = int;
constexpr VkSharedHandleType VkNullSharedHandle = -1;
#endif

class VkContext;
class VkDevice;
class VkCommandList;
class VkTexture;

namespace ext
{
class VkSurface final
{
public:
    VkSurface() {}

    void init(vk::Instance instance, const WindowSurface& surface);
    void init(VkDevice* device, const WindowSurface& surface);
    ~VkSurface();

    vk::SurfaceKHR get() const { return m_surface.get(); }

    uint32_t width() const { return m_windowSurface.width; }

    uint32_t height() const { return m_windowSurface.height; }

    const WindowSurface& windowSurface() const { return m_windowSurface; }

private:
    WindowSurface m_windowSurface;
    vk::UniqueSurfaceKHR m_surface;
};

namespace VkSwapchainUtil
{
vk::Format chooseSurfaceFormat(const std::span<const vk::SurfaceFormatKHR>& supportedSurfaceFormats, const std::span<const vk::Format>& candidateSurfaceFormats);
vk::ColorSpaceKHR chooseColorSpace(const std::span<const vk::ColorSpaceKHR>& supportedColorSpaces, const std::span<const vk::ColorSpaceKHR>& candidateColorSpaces);
qre::StaticVector<vk::ColorSpaceKHR, MaxColorSpaces> getSupportedColorSpacesForFormat(const std::span<const vk::SurfaceFormatKHR>& supportedSurfaceFormats, vk::Format format);
vk::CompositeAlphaFlagBitsKHR chooseCompositeAlpha(const vk::SurfaceCapabilitiesKHR& surfaceCaps);
vk::PresentModeKHR choosePresentMode(std::span<const vk::PresentModeKHR> supportedPresentModes, std::span<const vk::PresentModeKHR> candidatePresentModes);
vk::HdrMetadataEXT hdrMetadata(DisplayGamut displayGamut);
void initSwapchainCreateInfo(VkDevice* device, const Swapchain::Description& desc, vk::SwapchainCreateInfoKHR& createInfo);

static constexpr std::array<vk::PresentModeKHR, 1> DefaultCandidatePresentModes_VsyncOn = { vk::PresentModeKHR::eFifo };
static constexpr std::array<vk::PresentModeKHR, 3> DefaultCandidatePresentModes_VsyncOff = { vk::PresentModeKHR::eImmediate,
                                                                                             vk::PresentModeKHR::eFifoRelaxed,
                                                                                             vk::PresentModeKHR::eMailbox };

inline vk::PresentModeKHR choosePresentMode(std::span<const vk::PresentModeKHR> supportedPresentModes, bool vsync)
{
    return vsync ? choosePresentMode(supportedPresentModes, DefaultCandidatePresentModes_VsyncOn) : choosePresentMode(supportedPresentModes, DefaultCandidatePresentModes_VsyncOff);
}
}; // namespace VkSwapchainUtil

class VkSwapchain : public Swapchain
{
public:
    VkSwapchain() {}

    VkSwapchain(VkDevice* device, const Description& desc) { init(device, desc); }

    VkSwapchain(VkDevice* device, const Description& desc, const vk::SwapchainCreateInfoKHR& createInfo) { init(device, desc, createInfo); }

    ~VkSwapchain() { destroy(); }

    void destroy() override;

    void init(VkDevice* device, const Description& desc);
    void init(VkDevice* device, const Description& desc, const vk::SwapchainCreateInfoKHR& createInfo);
    void resize(const WindowSurface& surface) override;
    void resize(int width, int height) override;
    void acquireNextImage() override;
    void presentImage(CommandQueue* queue) override;

    Texture* getCurrentImage() const override { return getColorImage(m_currentImageIndex); }

    int getNumImages() const override { return m_numSwapchainImages; }

    vk::Semaphore getRenderCompleteSemaphore() const { return m_renderCompleteSemaphore[getCurrentImageIndex()].get(); }

    vk::Semaphore getRenderCompleteSemaphore(int index) const { return m_renderCompleteSemaphore[index].get(); }

    vk::Semaphore getImageAcquiredSemaphore() const { return m_imageAcquiredSemaphore.get(); }

    Format getFormat() const override;

    constexpr bool frameRendered() const { return m_frameRendered; }

    void setFrameRendered(bool v) { m_frameRendered = v; }

    const vk::SwapchainKHR& get() const { return m_swapchain.get(); }

    qhwi::Texture* getColorImage(uint32_t index) const;

    void setHdrMetadata(vk::Device vkDevice, const vk::HdrMetadataEXT& hdrMetadata) { vkDevice.setHdrMetadataEXT(m_swapchain.get(), hdrMetadata); }

protected:
    virtual void createSwapchainImages(const char* name, bool autoBarrier);
    void createSemaphores(const char* name = "");
    VkDevice* m_device = nullptr;
    VkSurface* m_surface = nullptr;

    uint32_t m_numSwapchainImages = 0;
    qre::RefCountPtr<qhwi::VkTexture> m_colorSwapchainTextures[MaxSwapchainImages];

    vk::UniqueSemaphore m_renderCompleteSemaphore[MaxSwapchainImages];

    qre::StaticQueue<vk::UniqueSemaphore, MaxSwapchainImages> m_recycledImageAcquiredSemaphores;
    vk::UniqueSemaphore m_imageAcquiredSemaphore;

    vk::UniqueSwapchainKHR m_swapchain;
    bool m_frameRendered = false;
    vk::Format m_colorFormat = vk::Format::eUndefined;
    vk::ColorSpaceKHR m_colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    bool m_vsync = false;
    bool m_autoBarrier = false;
};
} // namespace ext

inline qhwi::ext::VkSwapchain* vk(qhwi::ext::Swapchain* p)
{
    return static_cast<qhwi::ext::VkSwapchain*>(p);
}

class VkImageViewCache final
{
public:
    void init(VkDevice* device);
    vk::ImageView get(const VkTexture* texture, vk::ImageViewType imageViewType = vk::ImageViewType::e2D, const vk::ImageSubresourceRange* subresourceRange = nullptr);

    void onDestroyImage(vk::Image image)
    {
        std::erase_if(m_cache, [=](auto& kv) { return kv.first.image == image; });
    }

private:
    struct Entry
    {
        vk::ImageSubresourceRange subresourceRange;
        vk::ImageViewCreateInfo imageViewCreateInfo;
        vk::UniqueImageView imageView;
    };

    struct VkImageViewCreateInfoHash
    {
        size_t operator()(const vk::ImageViewCreateInfo&) const;
    };

    std::unordered_map<vk::ImageViewCreateInfo, Entry, VkImageViewCreateInfoHash> m_cache;
    VkDevice* m_device = nullptr;
};

class VkTexture final : public Texture
{
public:
    VkTexture(VkDevice* device, const Description& desc, vk::Image image = VK_NULL_HANDLE, vk::ImageView defaultImageView = VK_NULL_HANDLE);
    ~VkTexture();
    void updateFromHandle(Handle handle) override;

    vk::Image getImage() const { return m_image; }

    vk::ImageView getImageView(vk::ImageViewType imageViewType, vk::ImageSubresourceRange* subresourceRange = nullptr) const;

    vk::ImageView getDefaultImageView() const { return m_defaultImageView.get(); }

    constexpr vk::ImageAspectFlags getImageAspectFlags() { return m_defaultImageViewCreateInfo.subresourceRange.aspectMask; }

    vk::Format format() const { return m_format; }

    vk::Extent3D extent() const { return m_extent; }

    vk::ImageViewCreateInfo defaultImageViewCreateInfo() const { return m_defaultImageViewCreateInfo; };

    uint32_t pitch(uint16_t mipLevel) const override;

    VkSharedHandleType getSharedHandle() const { return m_sharedHandle; }

    VmaAllocation getVmaAllocation() const { return m_VmaAllocation; }

private:
    void init(const Description& desc,
              vk::ImageCreateInfo& imageCreateInfo,
              vk::OpticalFlowImageFormatInfoNV& opticalFlowImageFormatInfoNV,
              vk::ExternalMemoryImageCreateInfo& externalMemoryImageCreateInfo);
    uint32_t calculatePitch(Format format, uint32_t w) const;
    VkDevice* m_device = nullptr;
    vk::DeviceMemory m_tileMemory{};
    vk::DeviceSize m_tileMemoryOffset = 0;
    VkTileMemoryRequirementsQCOM m_tileMemReqs{};
    vk::UniqueImageView m_defaultImageView;
    bool m_ownsImage = false;
    bool m_ownsDefaultImageView = false;
    vk::ImageLayout m_currentImageLayout;
    uint32_t m_numSubresources = 0;
    vk::Format m_format;
    vk::Extent3D m_extent;
    vk::ImageViewCreateInfo m_defaultImageViewCreateInfo;
    VkSharedHandleType m_sharedHandle = VkNullSharedHandle;
    VmaAllocation m_VmaAllocation{};
    vk::Image m_image{};
};

inline qhwi::VkTexture* vk(qhwi::Texture* p)
{
    return static_cast<qhwi::VkTexture*>(p);
}

class VkSampler final : public Sampler
{
public:
    VkSampler(VkDevice* device, const Description& desc);

    vk::Sampler get() const { return m_sampler.get(); }

private:
    vk::SamplerCreateInfo initCreateInfo(const Description& desc);
    VkDevice* m_device = nullptr;
    vk::UniqueSampler m_sampler;
};

inline qhwi::VkSampler* vk(qhwi::Sampler* p)
{
    return static_cast<qhwi::VkSampler*>(p);
}

class VkShader final : public Shader
{
public:
    VkShader(VkDevice* device, const Description& desc, const void* byteCode, size_t length);

    const vk::ShaderModule& get() const { return m_shaderModule.get(); }

    struct ByteCode
    {
        void initAsCopy(const void* srcData, size_t srcSize)
        {
            data = malloc(srcSize);
            memcpy(data, srcData, srcSize);
            size = srcSize;
        }

        ~ByteCode() { free(data); }

        void* data = nullptr;
        size_t size = 0;
    };

    const ByteCode& byteCode() { return m_byteCode; }

private:
    VkDevice* m_device = nullptr;
    vk::UniqueShaderModule m_shaderModule;
    //store a copy of shader bytecode when shader validation enabled
    ByteCode m_byteCode;
};

inline qhwi::VkShader* vk(qhwi::Shader* p)
{
    return static_cast<qhwi::VkShader*>(p);
}

struct VkPipelineColorBlendStateCreateInfo
{
    uint32_t numAttachments = 0;
    vk::PipelineColorBlendAttachmentState attachmentBlendStates[MaxAttachments];
    std::array<float, 4> blendConstants{};
    vk::PipelineColorBlendStateCreateInfo createInfo;
};

struct VkPipelineVertexInputStateCreateInfo
{
    uint32_t numVertexInputBindingDescriptions = 0;
    vk::VertexInputBindingDescription vertexInputBindingDescriptions[MaxVertexShaderInputBindings];
    uint32_t numVertexInputAttributeDescriptions = 0;
    vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[MaxVertexShaderInputAttributes];
    vk::PipelineVertexInputStateCreateInfo createInfo;
};

class VkPipelineLayout final : public PipelineLayout
{
public:
    struct Description
    {
        Description(const PipelineLayout::Description& desc);
        PipelineLayout::Description base;

        struct
        {
            uint32_t numDescriptorSetLayouts = 0;
            vk::DescriptorSetLayout descriptorSetLayouts[MaxDescriptorSetsPerPipeline];
            uint32_t numPushConstantRanges = 0;
            vk::PushConstantRange pushConstantRanges[MaxPushConstantRanges];
            vk::PipelineLayoutCreateInfo createInfo;
        } vk;
    };

    VkPipelineLayout(VkDevice* device, const Description& desc);

    vk::PipelineLayout get() { return m_pipelineLayout.get(); }

private:
    VkDevice* m_device = nullptr;
    vk::UniquePipelineLayout m_pipelineLayout;
};

using VkPipelineLayoutDesc = VkPipelineLayout::Description;

inline qhwi::VkPipelineLayout* vk(qhwi::PipelineLayout* p)
{
    return static_cast<qhwi::VkPipelineLayout*>(p);
}

class VkGraphicsPipeline final : public GraphicsPipeline
{
public:
    struct Description
    {
        Description(const GraphicsPipeline::Description& desc);

        Description& setRenderPass(vk::RenderPass r)
        {
            vk.createInfo.renderPass = r;
            return *this;
        }

        Description& setSubpass(uint32_t s)
        {
            vk.createInfo.subpass = s;
            return *this;
        }

        Description& setRenderingAttachmentLocationInfo(const vk::RenderingAttachmentLocationInfoKHR& v)
        {
            vk.renderingAttachmentLocationInfo = v;
            vk.pipelineRenderingCreateInfo.setPNext(&vk.renderingAttachmentLocationInfo);
            return *this;
        }

        GraphicsPipeline::Description base;

        struct
        {
            uint32_t numVertexInputBindingDescriptions = 0;
            vk::VertexInputBindingDescription vertexInputBindingDescriptions[MaxVertexShaderInputBindings];
            uint32_t numShaderStages = 0;
            vk::PipelineShaderStageCreateInfo shaderStages[MaxShaderStages];
            VkPipelineColorBlendStateCreateInfo blendState;
            vk::PipelineRasterizationStateCreateInfo rasterizerState;
            vk::PipelineMultisampleStateCreateInfo multisampleState;
            vk::PipelineDepthStencilStateCreateInfo depthStencilState;
            VkPipelineVertexInputStateCreateInfo vertexInputState;
            vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
            vk::PipelineViewportStateCreateInfo viewportState;
            uint32_t numDynamicStates = 0;
            vk::DynamicState dynamicStates[MaxPipelineDynamicStates] = {};
            vk::PipelineDynamicStateCreateInfo dynamicState;
            qre::StaticVector<vk::Format, MaxColorAttachments> colorAttachmentFormats;
            qre::StaticVector<vk::Format, MaxColorAttachments> customResolveAttachmentFormats;
            vk::Format depthFormat = vk::Format::eUndefined;
            vk::Format stencilFormat = vk::Format::eUndefined;
            vk::GraphicsPipelineCreateInfo createInfo;
            vk::PipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo;
            vk::RenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo;
            vk::CustomResolveCreateInfoEXT customResolveCreateInfo;

            vk::GraphicsPipelineLibraryCreateInfoEXT graphicsPipelineLibraryCreateInfo;
            vk::PipelineLibraryCreateInfoKHR pipelineLibraryCreateInfo;
            std::vector<vk::Pipeline> graphicPipelineLibraries;
            bool enablePipelineCache = true;
        } vk;

        // This function will get called automatically when the pipeline is created
        // The user can call this function manually if they wish to override some vk params
        void init();

    private:
        bool initOnce = true;
    };

    VkGraphicsPipeline(VkDevice* device, const Description& desc);

    vk::Pipeline get() { return m_graphicsPipeline.get(); }

private:
    bool supportsDynamicRenderingLocalRead() const;
    VkDevice* m_device = nullptr;
    vk::UniquePipeline m_graphicsPipeline;
    vk::UniqueRenderPass m_vkRenderPass;
};

using VkGraphicsPipelineDesc = VkGraphicsPipeline::Description;

inline qhwi::VkGraphicsPipeline* vk(qhwi::GraphicsPipeline* p)
{
    return static_cast<qhwi::VkGraphicsPipeline*>(p);
}

class VkComputePipeline final : public ComputePipeline
{
public:
    struct Description
    {
        Description(const ComputePipeline::Description& desc);
        ComputePipeline::Description base;

        struct
        {
            uint32_t numShaderStages = 0;
            vk::PipelineShaderStageCreateInfo shaderStages[MaxShaderStages];

            uint32_t numDescriptorSetLayouts = 0;
            vk::DescriptorSetLayout descriptorSetLayouts[MaxDescriptorSetsPerPipeline];
            vk::PipelineLayoutCreateInfo layout;
            vk::ComputePipelineCreateInfo computePipelineCreateInfo;
        } vk;
    };

    VkComputePipeline(VkDevice* device, const Description& desc);

    vk::Pipeline get() { return m_computePipeline.get(); }

#ifdef QHWI_ENABLE_COMPUTE_PIPELINE_VALIDATION
    struct
    {
        uint32_t numThreadsPerThreadGroup[3] = { 0, 0, 0 };
    } m_validationData;
#endif
private:
    VkDevice* m_device = nullptr;
    vk::UniquePipeline m_computePipeline;
};

using VkComputePipelineDesc = VkComputePipeline::Description;

class VkRayTracingPipeline final : public RayTracingPipeline
{
public:
    struct Description
    {
        Description(const RayTracingPipelineDesc& desc);

        struct
        {
            qre::StaticVector<vk::PipelineShaderStageCreateInfo, MaxRayPipelineShaderStages> shaderStageCreateInfo;
            qre::StaticVector<vk::RayTracingShaderGroupCreateInfoKHR, MaxRayPipelineShaderGroups> shaderGroupCreateInfo;
            vk::RayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo;
            vk::PipelineLayoutCreateInfo layout;
        } vk;

        RayTracingPipelineDesc base;
    };

    VkRayTracingPipeline(VkDevice* device, const Description& desc);

    const vk::Pipeline& get() const { return m_rayTracingPipeline.get(); };

private:
    vk::UniquePipeline m_rayTracingPipeline;
};

using VkRayTracingPipelineDesc = VkRayTracingPipeline::Description;

inline qhwi::VkComputePipeline* vk(qhwi::ComputePipeline* p)
{
    return static_cast<qhwi::VkComputePipeline*>(p);
}

class VkBuffer final : public Buffer
{
public:
    struct Description
    {
        Description(const Buffer::Description& desc);
        Buffer::Description base;

        struct
        {
            vk::BufferCreateInfo createInfo{};
            vk::ExternalMemoryBufferCreateInfo externalMemoryBufferCreateInfo{};
        } vk;
    };

    VkBuffer(VkDevice* device, const Description& desc, const void* data = nullptr);
    ~VkBuffer();

    void* map(uint64_t offset = 0, uint64_t size = 0) override;
    void unmap() override;

    vk::Buffer get() { return m_buffer; }

    VkDevice* getDevice() { return m_device; }

    uint64_t getDeviceAddress() const override;

    VkSharedHandleType getSharedHandle() const { return m_sharedHandle; }

    VmaAllocation getVmaAllocation() const { return m_VmaAllocation; }

private:
    Description m_desc;
    VkDevice* m_device = nullptr;
    vk::DeviceAddress m_deviceAddress = 0;
    VkSharedHandleType m_sharedHandle = VkNullSharedHandle;
    vk::DeviceSize m_tileMemoryOffset = 0;
    VkTileMemoryRequirementsQCOM m_tileMemReqs{};
    VmaAllocation m_VmaAllocation{};
    ::VkBuffer m_buffer;
};

using VkBufferDesc = VkBuffer::Description;

inline qhwi::VkBuffer* vk(qhwi::Buffer* p)
{
    return static_cast<qhwi::VkBuffer*>(p);
}

class VkAccelerationStructure final : public AccelerationStructure
{
public:
    struct Description
    {
        Description(const AccelerationStructure::Description& desc);
        AccelerationStructure::Description base;
    };

    VkAccelerationStructure(VkDevice* device, const Description& desc);

    ~VkAccelerationStructure() { delete m_buffer; }

    AccelerationStructure::Description* desc() override { return &m_desc.base; }

    const AccelerationStructure::Description* desc() const override { return &m_desc.base; }

    qhwi::Buffer* buffer() const override { return m_buffer; }

    vk::DeviceAddress getDeviceAddress() const { return m_buffer->getDeviceAddress(); }

    vk::AccelerationStructureKHR* getAccelerationStructure() { return &m_accelerationStructure.get(); }

private:
    Description m_desc;
    VkDevice* m_device;
    VkBuffer* m_buffer;
    vk::UniqueAccelerationStructureKHR m_accelerationStructure;
};

inline qhwi::VkAccelerationStructure* vk(qhwi::AccelerationStructure* p)
{
    return static_cast<qhwi::VkAccelerationStructure*>(p);
}

class VkDescriptorSetLayout final : public DescriptorSetLayout
{
public:
    struct Description
    {
        Description(const DescriptorSetLayout::Description& desc);
        DescriptorSetLayout::Description base;

        struct
        {
            uint32_t numBindings = 0;
            vk::DescriptorSetLayoutBinding bindings[MaxDescriptorSetLayoutBindings];
            uint32_t numBindingFlags = 0;
            vk::DescriptorBindingFlagsEXT bindingFlags[MaxDescriptorSetLayoutBindings];
            vk::DescriptorSetLayoutCreateInfo createInfo;
            vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo;
        } vk;
    };

    VkDescriptorSetLayout(VkDevice* device, const DescriptorSetLayout::Description& desc);

    constexpr DescriptorSetLayout::Description* desc() override { return &m_desc.base; }

    constexpr const DescriptorSetLayout::Description* desc() const override { return &m_desc.base; }

    constexpr Description* vkDesc() { return &m_desc; }

    vk::DescriptorSetLayout get() { return m_descriptorSetLayout.get(); }

private:
    VkDevice* m_device = nullptr;
    Description m_desc;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
};

using VkDescriptorSetLayoutDesc = VkDescriptorSetLayout::Description;

inline qhwi::VkDescriptorSetLayout* vk(qhwi::DescriptorSetLayout* p)
{
    return static_cast<qhwi::VkDescriptorSetLayout*>(p);
}

struct VkDescriptorSetBinding
{
    void initAsBufferInfos(uint32_t infoCount)
    {
        bufferInfos = new vk::DescriptorBufferInfo[infoCount];
        count = infoCount;
    }

    void initAsBufferViews(uint32_t infoCount)
    {
        bufferViews = new vk::BufferView[infoCount];
        count = infoCount;
    }

    void initAsImageInfos(uint32_t infoCount, bool initImageViews = false)
    {
        imageInfos = new vk::DescriptorImageInfo[infoCount];
        if (initImageViews)
        {
            imageViews = new vk::ImageView[infoCount];
        }
        count = infoCount;
    }

    void initAsAccelStructInfos(uint32_t infoCount)
    {
        accelStructInfos = new vk::WriteDescriptorSetAccelerationStructureKHR[infoCount];
        count = infoCount;
    }

    ~VkDescriptorSetBinding() { destroy(); }

    void destroy()
    {
        delete[] accelStructInfos;
        delete[] imageViews;
        delete[] bufferViews;
        delete[] imageInfos;
        delete[] bufferInfos;
    }

    void onDestroy(const vk::Device& device)
    {
        if (bufferViews)
        {
            for (uint32_t j = 0; j < count; j++)
            {
                device.destroyBufferView(bufferViews[j]);
            }
        }
    }

    vk::DescriptorBufferInfo* bufferInfos = nullptr;
    vk::DescriptorImageInfo* imageInfos = nullptr;
    vk::BufferView* bufferViews = nullptr;
    vk::ImageView* imageViews = nullptr;
    vk::WriteDescriptorSetAccelerationStructureKHR* accelStructInfos = nullptr;
    uint32_t count = 0;
};

class VkDescriptorSet final : public DescriptorSet
{
public:
    struct Description
    {
        Description(const DescriptorSet::Description& desc);
        DescriptorSet::Description base;

        struct
        {
            int numDescriptors = 0;
            VkDescriptorSetBinding bindings[MaxDescriptorSetLayoutBindings];
            vk::DescriptorSetLayout layout;
            vk::DescriptorSetAllocateInfo allocateInfo;
            uint32_t variableDescriptorCount = 0;
            vk::DescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocateInfo;
        } vk;

        VkDescriptorSetBinding* findVkDescriptorSetBinding(DescriptorType type, uint32_t shaderRegister)
        {
            for (uint32_t i = 0; i < base.bindings.size(); ++i)
            {
                if ((base.bindings[i]->type() == type) && (base.bindings[i]->shaderRegister() == shaderRegister))
                {
                    return &vk.bindings[i];
                }
            }
            return nullptr;
        }
    };

    VkDescriptorSet(VkDevice* device, const DescriptorSet::Description& desc);
    ~VkDescriptorSet();

    constexpr DescriptorSet::Description* desc() override { return &m_desc.base; }

    const DescriptorSet::Description* desc() const override { return &m_desc.base; }

    constexpr Description* vkDesc() { return &m_desc; }

    constexpr vk::DescriptorSet get() { return m_descriptorSet; }

private:
    bool supportsDynamicRenderingLocalRead() const;
    VkDevice* m_device = nullptr;
    Description m_desc;
    vk::DescriptorSet m_descriptorSet;
    vk::DescriptorPool m_descriptorPool;
};

using VkDescriptorSetDesc = VkDescriptorSet::Description;

inline qhwi::VkDescriptorSet* vk(qhwi::DescriptorSet* p)
{
    return static_cast<qhwi::VkDescriptorSet*>(p);
}

struct CopyTextureRegion2
{
    // Parameters for copying from a source to destination texture
    Subresource* dstSubresource = nullptr;
    uvec3 dstOrigin;
    Subresource* srcSubresource = nullptr;
    uvec3 srcOrigin;
    Extent3D dstExtent;
    Extent3D srcExtent;
    Filter filter;
};

class VkCommandList final : public CommandList
{
public:
    friend class VkRenderPassEncoder;
    friend struct VkBeginRenderPassOp;
    friend struct VkEndRenderPassOp;
    friend struct VkBeginCustomResolveOp;

    struct Description
    {
        Description(const CommandList::Description& desc);
        CommandList::Description base;

        struct
        {
            vk::CommandBufferLevel level;
        } vk;
    };

    VkCommandList(VkDevice* device, const Description& desc);
    ~VkCommandList();

    CommandList::Description* desc() override { return &m_desc.base; }

    const CommandList::Description* desc() const override { return &m_desc.base; }

    void beginMarker(const char* name) override;
    void endMarker() override;

    // Begin recording
    void begin() override;
    // End recording
    void end() override;
    void beginRenderPass(uint32_t numColorAttachments,
                         const ColorAttachmentDescription* colorAttachments,
                         const DepthStencilAttachmentDescription* depthStencilAttachment,
                         const MultiViewState* multiViewState,
                         const Rect2D* multiViewPerViewRenderAreasQCOM = nullptr,
                         const uint32_t* colorAttachmentRemapping = nullptr,
                         const TileShadingInfoQCOM* tileShadingInfoQCOM = nullptr,
                         bool customResolve = false) override;
    void endRenderPass() override;

    void beginVkRendering(const vk::RenderingInfo& rInfo);
    void beginVkRenderPass(const vk::RenderPassBeginInfo& rpBeginInfo, vk::SubpassContents subpassContents = vk::SubpassContents::eInline);
    void endVkRenderPass();
    void endVkRendering();
    void nextSubpass(vk::SubpassContents subpassContents = vk::SubpassContents::eInline);
    void setRenderingAttachmentLocations(const vk::RenderingAttachmentLocationInfoKHR* pLocationInfo);

    void beginComputePass() override;
    void endComputePass() override;

    void beginTransferPass() override;
    void endTransferPass() override;

    void beginAccelerationStructurePass() override;
    void endAccelerationStructurePass() override;

    void dispatch(uint32_t numThreadGroupsX,
                  uint32_t numThreadGroupsY,
                  uint32_t numThreadGroupsZ,
                  uint32_t numThreadsPerThreadgroupX,
                  uint32_t numThreadsPerThreadgroupY,
                  uint32_t numThreadsPerThreadgroupZ) override;
    void dispatchMesh(const DispatchMeshParams& p) override;
    void draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0) override;
    void drawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0) override;
    void dispatchRays(const StridedDeviceAddressRegion& raygenShaderBindingTable,
                      const StridedDeviceAddressRegion& missShaderBindingTable,
                      const StridedDeviceAddressRegion& hitShaderBindingTable,
                      const StridedDeviceAddressRegion& callableShaderBindingTable,
                      uint32_t width,
                      uint32_t height,
                      uint32_t depth) override;

    // Indirect dispatch and draw calls
    void dispatchRaysIndirect(Buffer* buffer) override;
    void dispatchIndirect(uint32_t numThreadsPerThreadgroupX,
                          uint32_t numThreadsPerThreadgroupY,
                          uint32_t numThreadsPerThreadgroupZ,
                          Buffer* buffer,
                          size_t offset = 0,
                          uint32_t dispatchCount = 1) override;
    void drawIndirect(Buffer* buffer, size_t offset = 0, uint32_t drawCount = 1) override;
    void drawIndexedIndirect(Buffer* buffer, size_t offset = 0, uint32_t drawCount = 1) override;

    void bindVertexBuffer(Buffer* buffer, uint32_t binding, size_t offset = 0, size_t stride = 0) override;
    void bindIndexBuffer(Buffer* buffer, IndexFormat indexFormat = IndexFormat::eUint32, size_t offset = 0) override;
    void pushConstants(PipelineLayout* pipelineLayout, PipelineBindPoint pipelineBindPoint, const void* data, uint32_t index = 0) override;
    void bindDescriptorSet(DescriptorSet* descriptorSet,
                           PipelineBindPoint pipelineBindPoint,
                           PipelineLayout* pipelineLayout,
                           uint32_t numDynamicOffsets = 0,
                           const uint32_t* dynamicOffsets = nullptr) override;

    void clear(Buffer* dst, uint32_t value = 0) override;
    void clear(Texture* dst, Color value = Color()) override;

    void updateBuffer(Buffer* dst, std::span<const UpdateBufferRegion> regions) override;
    void updateTexture(Texture* dst, std::span<const UpdateTextureRegion> regions) override;

    void copyBuffer(Buffer* dst, Buffer* src, std::span<const CopyBufferRegion> regions) override;
    void copyBufferToTexture(Texture* dst, Buffer* src, std::span<const CopyBufferToTextureRegion> regions) override;
    void copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion> regions) override;
    void copyTextureToBuffer(Buffer* dst, Texture* src, std::span<const CopyTextureToBufferRegion> regions) override;
    void copyTexture(Texture* dst, Texture* src, std::span<const CopyTextureRegion2> regions, Filter filter);

    void copyTexture(Texture* dst, Texture* src, const CopyTextureRegion2& region = {}, Filter filter = Filter::eNearest) { copyTexture(dst, src, { &region, 1 }, filter); }

    void bindComputePipeline(ComputePipeline* pipeline) override;
    void bindGraphicsPipeline(GraphicsPipeline* pipeline) override;
    void bindRayTracingPipeline(RayTracingPipeline* pipeline) override;

    void setViewports(uint32_t numViewports, const Viewport* viewports) override;
    void setScissors(uint32_t numScissorRects, const ScissorRect* scissorRects) override;
    void setStencilReference(uint32_t reference) override;

    void addBarriers(uint32_t numBufferBarriers,
                     const BufferBarrier* bufferBarriers,
                     uint32_t numTextureBarriers,
                     const TextureBarrier* textureBarriers,
                     uint32_t numGlobalBarriers = 0,
                     const GlobalBarrier* globalBarriers = nullptr,
                     DependencyFlags flags = 0) override;
    void addImageMemoryBarrier(const VkImageMemoryBarrier2KHR& b, VkDependencyFlags dependencyFlags = 0);
    void addBufferMemoryBarrier(const VkBufferMemoryBarrier2KHR& b, VkDependencyFlags dependencyFlags = 0);
    void addMemoryBarrier(const VkMemoryBarrier2KHR& b, VkDependencyFlags dependencyFlags = 0);

    void addBarriers(const vk::DependencyInfoKHR& dependencyInfo);

    void buildBottomLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                               Buffer* scratchBuffer,
                                               BottomLevelDescriptor* bottomLevelDescriptors,
                                               size_t descriptorCount,
                                               void* scratchMemory,
                                               AccelerationStructureBuildMode buildMode,
                                               AccelerationStructureBuildFlags flags) override;
    void buildTopLevelAccelerationStructure(AccelerationStructure* accelerationStructure,
                                            Buffer* instanceBuffer,
                                            Buffer* scratchBuffer,
                                            size_t instanceCount,
                                            AccelerationStructure** blasArray,
                                            size_t blasCount,
                                            AccelerationStructureBuildMode buildMode,
                                            AccelerationStructureBuildFlags flags) override;
    void beginTimerQuery(TimerQuery* timerQuery) override;
    void endTimerQuery(TimerQuery* timerQuery) override;
    void resolveTimerQueryData(TimerQuery* timerQuery) override;
    void resetTimerQueryPool(TimerQuery* timerQuery) override;

    vk::CommandBuffer get() { return m_commandBuffer.get(); }

    Device* device() override { return (Device*)m_device; }

    void setState(Buffer* buffer, BufferState nextState) override;
    void setState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange = EntireResource) override;
    void setRenderingAttachmentLocations(std::span<const uint32_t> colorAttachmentRemapping);

    void beginCustomResolve() override;

    void dispatchTileQCOM(const VkDispatchTileInfoQCOM* pDispatchTileInfo = nullptr);
    void beginPerTileExecutionQCOM(const VkPerTileBeginInfoQCOM* pPerTileBeginInfo = nullptr);
    void endPerTileExecutionQCOM(const VkPerTileEndInfoQCOM* pPerTileEndInfo = nullptr);

private:
    void beginProfilingPass();
    void endProfilingPass();
    void beginRenderPass1(uint32_t numColorAttachments,
                          const ColorAttachmentDescription* colorAttachments,
                          const DepthStencilAttachmentDescription* depthStencilAttachment = nullptr,
                          const MultiViewState* multiViewState = nullptr,
                          const Rect2D* multiViewPerViewRenderAreasQCOM = nullptr,
                          const uint32_t* colorAttachmentRemapping = nullptr,
                          const TileShadingInfoQCOM* tileShadingInfoQCOM = nullptr,
                          bool customResolve = false);
    void endRenderPass1();
    void beginCustomResolve1();
    void bindTileMemoryQCOM(const VkTileMemoryBindInfoQCOM* pTileMemoryBindInfo);
    VkDevice* m_device = nullptr;
    Description m_desc;
    vk::UniqueCommandBuffer m_commandBuffer;

    // Defer render pass commands state
    bool defer() { return m_deferState == DeferState::eRecord; }
    enum class DeferState
    {
        eOff,
        eRecord,
        ePlayback
    };
    DeferState m_deferState = DeferState::eOff;
    VkRenderPassEncoder m_rp;
    MultiViewState m_multiViewState;
    std::queue<vk::UniqueRenderPass> m_vkRenderPasses;
    std::queue<vk::UniqueFramebuffer> m_vkFramebuffers;
    bool m_inTileShadingPass = false;
};

inline qhwi::VkCommandList* vk(qhwi::CommandList* p)
{
    return static_cast<qhwi::VkCommandList*>(p);
}

class VkCommandQueue final : public CommandQueue
{
public:
    struct Description
    {
        Description(const CommandQueue::Description& desc, int queueIndex = 0)
            : base(desc)
        {
            vk.queueIndex = queueIndex;
        }

        CommandQueue::Description base;

        struct
        {
            int queueIndex = 0;
            int queueFamilyIndex = 0;
        } vk;
    };

    VkCommandQueue(VkDevice* device, const Description& desc);

    ~VkCommandQueue();

    // Return submission ID
    uint64_t submit(uint32_t numCommandLists, CommandList** commandLists, Fence* fence = nullptr, ext::Swapchain* swapchain = nullptr) override;

    void waitForIdle() override { m_queue.waitIdle(); }

    CommandQueue::Description* desc() override { return &m_desc.base; }

    const CommandQueue::Description* desc() const override { return &m_desc.base; }

    void* getNativeQueue() override { return &m_queue; }

    constexpr Description* vkDesc() { return &m_desc; }

    constexpr vk::Queue get() { return m_queue; }

    void setExtraSignalSemaphoreInfo(vk::SemaphoreSubmitInfo s) { m_extraSignalSemaphoreSubmitInfo = s; }

    void setExtraWaitSemaphoreInfo(vk::SemaphoreSubmitInfo s) { m_extraWaitSemaphoreSubmitInfo = s; }

#ifdef QHWI_TRACY_GPU_BACKEND_VULKAN
    auto getTracyContext() { return m_tracyCtx; }

    vk::CommandBuffer tracySetupCommandBuffer();
#endif

protected:
    void updateLastCompletedID() override;

    Description m_desc;
    VkDevice* m_device = nullptr;
    vk::Queue m_queue;
    vk::UniqueSemaphore m_submissionIDSemaphore;
    vk::SemaphoreSubmitInfo m_extraWaitSemaphoreSubmitInfo{};
    vk::SemaphoreSubmitInfo m_extraSignalSemaphoreSubmitInfo{};

#ifdef QHWI_TRACY_GPU_BACKEND_VULKAN
private:
    TracyVkCtx m_tracyCtx = nullptr;
    vk::UniqueCommandPool m_tracySetupPool;
    vk::UniqueCommandBuffer m_tracySetupCommandBuffer;
#endif
};

inline qhwi::VkCommandQueue* vk(qhwi::CommandQueue* p)
{
    return static_cast<qhwi::VkCommandQueue*>(p);
}

class VkFence final : public Fence
{
public:
    VkFence(VkDevice* device, FenceState initialValue = FenceState_Unsignaled, const char* name = "", HeapFlags heapFlags = HeapFlag_None);
    Result wait() override;
    FenceState getCurrentState() override;
    void reset() override;

    vk::Fence get() { return m_fence.get(); }

    VkSharedHandleType getSharedHandle() const { return m_sharedHandle; }

private:
    VkDevice* m_device = nullptr;
    vk::UniqueFence m_fence;
    VkSharedHandleType m_sharedHandle = VkNullSharedHandle;
};

inline qhwi::VkFence* vk(qhwi::Fence* p)
{
    return static_cast<qhwi::VkFence*>(p);
}

namespace VkRenderPassFallback
{
struct AttachmentInfo
{
    vk::Format format = vk::Format::eUndefined;
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;
};

struct RenderPassDesc
{
    qre::StaticVector<AttachmentInfo, MaxRenderTargetViews> colorAttachments;
    AttachmentInfo depthStencilAttachment;
    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
    vk::AttachmentLoadOp stencilLoadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp stencilStoreOp = vk::AttachmentStoreOp::eStore;
    vk::ResolveModeFlagBits stencilResolveMode = vk::ResolveModeFlagBits::eNone;
    uint32_t numViews = 1;
    bool customResolve = false;
};

vk::UniqueRenderPass createRenderPass(qhwi::VkDevice* device, const RenderPassDesc& desc);

struct FramebufferDesc
{
    vk::RenderPass renderPass;
    uint32_t numColorAttachments = 0;
    const ColorAttachmentDescription* colorAttachments = nullptr;
    const DepthStencilAttachmentDescription* depthStencilAttachment = nullptr;
};

vk::UniqueFramebuffer createFramebuffer(vk::Device vkDevice, const FramebufferDesc& desc);
}; // namespace VkRenderPassFallback

class VkTimerQueryHeap final : public QueryHeap
{
public:
    void init(Device*, const char* name = "") override;

    vk::QueryPool getTimerQueryHeap() { return queryHeap.get(); };

private:
    vk::UniqueQueryPool queryHeap;
};

inline qhwi::VkTimerQueryHeap* vk(qhwi::QueryHeap* p)
{
    return static_cast<qhwi::VkTimerQueryHeap*>(p);
}

class VkTimerQuery final : public TimerQuery
{
public:
    VkTimerQuery(VkTimerQueryHeap* pQueryHeap, uint32_t beginIndex, uint32_t endIndex)
        : TimerQuery(pQueryHeap, beginIndex, endIndex)
    {
    }
};

inline qhwi::VkTimerQuery* vk(qhwi::TimerQuery* p)
{
    return static_cast<qhwi::VkTimerQuery*>(p);
}

class VkPhysicalDevice;

// Individual feature structs, used for querying support and enabling of individual features
struct VkFeaturesInfo
{
    void init(const VkPhysicalDevice* physicalDevice);

    bool supportsUint8Index() const { return indexTypeUint8.indexTypeUint8 == VK_TRUE; }

    bool supportsDeviceAddress() const { return bufferAddress.bufferDeviceAddress == VK_TRUE; }

    bool supportsRayQuery() const { return rayQuery.rayQuery == VK_TRUE; }

    bool supportsRayPipelines() const { return rayPipeline.rayTracingPipeline == VK_TRUE; }

    bool supportsAccelerationStructures() const { return accelerationStructure.accelerationStructure == VK_TRUE; }

    bool supportsRayTraceMaintenance1Features() const { return rtMaintenance1Features.rayTracingMaintenance1 == VK_TRUE; }

    bool supportsMeshShaders() const { return (meshShader.meshShader == VK_TRUE && meshShader.taskShader == VK_TRUE); }

    bool supportsRayTraceInvocationReorderFeature() const { return rtInvocationReorderFeatures.rayTracingInvocationReorder == VK_TRUE; }

    bool supportsImageProcessingQCOM() const
    {
        return (imageProcessingQCOM.textureBlockMatch == VK_TRUE) && bool(imageProcessingQCOM.textureBoxFilter == VK_TRUE) &&
               bool(imageProcessingQCOM.textureSampleWeighted == VK_TRUE);
    }

    bool supportsMultiView() const { return (multiView.multiview == VK_TRUE); }

    bool supportsMultiViewPerViewViewportsQCOM() const { return (multiViewPerViewViewportsQCOM.multiviewPerViewViewports == VK_TRUE); }

    bool supportsMultiViewPerViewRenderAreasQCOM() const { return (multiViewPerViewRenderAreasQCOM.multiviewPerViewRenderAreas == VK_TRUE); }

    bool supportsOpticalFlowNV() const { return (opticalFlowNV.opticalFlow == VK_TRUE); }

    bool supportsDynamicRendering() const { return (dynamicRenderingFeatures.dynamicRendering == VK_TRUE); }

    bool supportsDynamicRenderingLocalRead() const { return (dynamicRenderingLocalReadFeatures.dynamicRenderingLocalRead == VK_TRUE); }

    bool supportsDynamicRenderingUnusedAttachments() const { return (dynamicRenderingUnusedAttachmentsFeatures.dynamicRenderingUnusedAttachments == VK_TRUE); }

    bool supportsShaderFloat16() const { return shaderFloat16Int8.shaderFloat16 != 0; }

    bool supportsShaderInt8() const { return shaderFloat16Int8.shaderInt8 != 0; }

    bool supportsTextureCompressionAstc() const { return features2.features.textureCompressionASTC_LDR; }

    bool supportsTextureCompressionBc() const { return features2.features.textureCompressionBC; }

    bool supportsTextureCompressionEtc() const { return features2.features.textureCompressionETC2; }

    bool supportsTileShadingQCOM() const { return tileShadingFeatures.tileShading; }

    bool supportsGraphicsPipelineLibrary() const { return graphicsPipelineLibraryFeatures.graphicsPipelineLibrary; }

    bool supportsTileMemoryHeapQCOM() const { return tileMemoryHeapQCOM.tileMemoryHeap; };

    bool supportsMemoryDecompression() const { return memoryDecompressionFeatures.memoryDecompression; };

    bool supportsOpacityMicromaps() const { return opacityMicromap.micromap == VK_TRUE; }

    bool supportsCustomResolve() const { return customResolveFeatures.customResolve == VK_TRUE; }

    vk::PhysicalDeviceFeatures2KHR features2{};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
    vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR bufferAddress{};
    vk::PhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing{};
    vk::PhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRate{};
    vk::PhysicalDeviceImageProcessingFeaturesQCOM imageProcessingQCOM{};
    vk::PhysicalDeviceIndexTypeUint8FeaturesEXT indexTypeUint8{};
    vk::PhysicalDeviceMeshShaderFeaturesEXT meshShader{};
    vk::PhysicalDeviceMultiviewFeaturesKHR multiView{};
    vk::PhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM multiViewPerViewRenderAreasQCOM{};
    vk::PhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM multiViewPerViewViewportsQCOM{};
    vk::PhysicalDeviceOpticalFlowFeaturesNV opticalFlowNV{};
    vk::PhysicalDeviceRayQueryFeaturesKHR rayQuery{};
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayPipeline{};
    vk::PhysicalDeviceRobustness2FeaturesEXT robustness2FeaturesEXT{};
    vk::PhysicalDeviceShaderFloat16Int8FeaturesKHR shaderFloat16Int8{};
    vk::PhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControl{};
    vk::PhysicalDeviceSynchronization2FeaturesKHR synchronization2{};
    vk::PhysicalDeviceTimelineSemaphoreFeatures timelineSemaphore{};
    VkPhysicalDeviceTileShadingFeaturesQCOM tileShadingFeatures{};
    vk::PhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures{};
    vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
    vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR dynamicRenderingLocalReadFeatures{};
    vk::PhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynamicRenderingUnusedAttachmentsFeatures{};
    VkPhysicalDeviceTileMemoryHeapFeaturesQCOM tileMemoryHeapQCOM{};
    VkPhysicalDeviceTensorFeaturesARM tensorFeaturesARM{};
    VkPhysicalDeviceDataGraphFeaturesARM dataGraphFeaturesARM{};
    vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures{};
    vk::PhysicalDeviceRayTracingMaintenance1FeaturesKHR rtMaintenance1Features{};
    vk::PhysicalDevicePipelineCreationCacheControlFeaturesEXT pipelineCreationCacheControl{};
    vk::PhysicalDeviceMemoryDecompressionFeaturesEXT memoryDecompressionFeatures{};
    vk::PhysicalDeviceRayTracingInvocationReorderFeaturesNV rtInvocationReorderFeatures = {};
    vk::PhysicalDeviceOpacityMicromapFeaturesEXT opacityMicromap = {};
    vk::PhysicalDeviceCustomResolveFeaturesEXT customResolveFeatures = {};
};

struct VkPropertiesInfo
{
    void init(const VkPhysicalDevice* physicalDevice);
    vk::PhysicalDeviceProperties2KHR properties2{};
    vk::PhysicalDeviceConservativeRasterizationPropertiesEXT conservativeRasterization{};
    vk::PhysicalDeviceDepthStencilResolvePropertiesKHR depthStencilResolve{};
    vk::PhysicalDeviceImageProcessingPropertiesQCOM imageProcessingQCOM{};
    vk::PhysicalDeviceSubgroupProperties subgroup{};
    vk::PhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControl{};
    vk::PhysicalDeviceGraphicsPipelineLibraryPropertiesEXT graphicsPipelineLibrary{};
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipeline{};
    vk::PhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructure{};
    vk::PhysicalDeviceTileMemoryHeapPropertiesQCOM tileMemoryHeapQCOM{};
    vk::PhysicalDeviceMemoryDecompressionPropertiesEXT memoryDecompressionProperties{};
    vk::PhysicalDeviceRayTracingInvocationReorderPropertiesNV rtInvocationReorderProperties{};
};

class VkPhysicalDevice final
{
public:
    friend class VkDevice;

    VkPhysicalDevice() {}

    VkPhysicalDevice(vk::PhysicalDevice physicalDevice) { init(physicalDevice); }

    void init(vk::PhysicalDevice physicalDevice);

    vk::PhysicalDevice get() const { return m_physicalDevice; }

    bool isUnifiedMemoryArchitecture() const { return (m_vkPropertiesInfo.properties2.properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu); }

    const vk::PhysicalDeviceFeatures2KHR& getFeatures2() const { return m_vkFeaturesInfo.features2; }

    const vk::PhysicalDeviceFeatures& getFeatures() const { return m_vkFeaturesInfo.features2.features; }

    const vk::PhysicalDeviceProperties2KHR& getProperties2() const { return m_vkPropertiesInfo.properties2; }

    const vk::PhysicalDeviceProperties& getProperties() const { return m_vkPropertiesInfo.properties2.properties; }

    const vk::PhysicalDeviceLimits& getLimits() const { return m_vkPropertiesInfo.properties2.properties.limits; }

    int32_t getMemoryType(uint32_t supportedTypesBitmask, vk::MemoryPropertyFlags memoryPropertyFlags);
    int32_t findQueueFamilyIndex(vk::SurfaceKHR surface, vk::QueueFlags queueFlags, bool needsPresentSupport) const;

    void getVkSurfaceFormats(vk::SurfaceKHR vkSurface, uint32_t* numSurfaceFormatsPtr, vk::SurfaceFormatKHR* surfaceFormats = nullptr) const;

    const VkPropertiesInfo& getVkPropertiesInfo() const { return m_vkPropertiesInfo; }

    const vk::PhysicalDeviceAccelerationStructurePropertiesKHR getAccelerationStructureProperties() const { return m_vkPropertiesInfo.accelerationStructure; }

    bool supportsDepthResolveMode(ResolveMode mode) const
    {
        return mode == ResolveMode::eNone || (m_vkPropertiesInfo.depthStencilResolve.supportedDepthResolveModes & qhwi::vk(mode)) != vk::ResolveModeFlagsKHR(0);
    }

    bool supportsStencilResolveMode(ResolveMode mode) const
    {
        return mode == ResolveMode::eNone || (m_vkPropertiesInfo.depthStencilResolve.supportedStencilResolveModes & qhwi::vk(mode)) != vk::ResolveModeFlagsKHR(0);
    }

    bool supportsExtension(const char* name) const;

    const vk::PhysicalDeviceImageProcessingPropertiesQCOM& getImageProcessingPropertiesQCOM() const { return m_vkPropertiesInfo.imageProcessingQCOM; };

    const vk::PhysicalDeviceConservativeRasterizationPropertiesEXT& getConservativeRasterizationPropertiesEXT() const { return m_vkPropertiesInfo.conservativeRasterization; };

    const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR& getRayTracingPipelinesPropertiesKHR() const { return m_vkPropertiesInfo.rayTracingPipeline; };

    const vk::PhysicalDeviceMemoryProperties getMemoryProperties() const { return m_memoryProperties; }

    // Extensions we want to enable
    static qre::StaticVector<const char*, MaxVkDeviceExtensions> getDefaultRequiredExtensions()
    {
        return { VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                 VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                 VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                 VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                 VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME };
    }

    static qre::StaticVector<const char*, MaxVkDeviceExtensions> getDefaultOptionalExtensions()
    {
        return {
#if defined(QHWI_PLATFORM_ANDROID)
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
#endif
            VK_ARM_TENSORS_EXTENSION_NAME,
            VK_ARM_DATA_GRAPH_EXTENSION_NAME,
            VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
            VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
            VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME,
            VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
            VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_EXT_HDR_METADATA_EXTENSION_NAME,
            VK_EXT_MEMORY_DECOMPRESSION_EXTENSION_NAME,
            VK_EXT_MESH_SHADER_EXTENSION_NAME,
            VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME,
            VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
            VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
            VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME,
            VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
            VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
            VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME,
            VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
#if defined(QHWI_PLATFORM_WINDOWS)
            VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
#endif
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
#if defined(QHWI_PLATFORM_WINDOWS)
            VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
#else
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
#endif
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#if defined(QHWI_PLATFORM_WINDOWS)
            VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#else
            VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
#endif
            VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME,
            VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
            VK_KHR_MULTIVIEW_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ROBUSTNESS_2_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_NV_OPTICAL_FLOW_EXTENSION_NAME,
            VK_NVX_BINARY_IMPORT_EXTENSION_NAME,
            VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME,
            VK_QCOM_IMAGE_PROCESSING_EXTENSION_NAME,
            VK_QCOM_MULTIVIEW_PER_VIEW_RENDER_AREAS_EXTENSION_NAME,
            VK_QCOM_MULTIVIEW_PER_VIEW_VIEWPORTS_EXTENSION_NAME,
            VK_QCOM_TILE_PROPERTIES_EXTENSION_NAME,
            VK_QCOM_TILE_SHADING_EXTENSION_NAME,
            VK_QCOM_TILE_MEMORY_HEAP_EXTENSION_NAME,
            VK_QCOM_RENDER_PASS_SHADER_RESOLVE_EXTENSION_NAME,
            VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
            VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME,
            VK_EXT_CUSTOM_RESOLVE_EXTENSION_NAME
        };
    }

    bool supportsSynchronization2() const { return m_supportsSynchronization2; }

    bool supportsSamplerFilterMinMax() const { return m_supportsSamplerFilterMinMax; }

    bool supportsBarycentric() const { return m_supportsBarycentric; }

    bool supportsExtensions(std::span<const char* const> extensions) const;

    bool supportsCalibratedTimestamps() const { return m_supportsCalibratedTimestamps; }

    const VkFeaturesInfo& getVkFeaturesInfo() const { return m_vkFeaturesInfo; }

    const std::vector<vk::QueueFamilyProperties>& getQueueFamilyProperties() const { return m_queueFamilyProperties; }

    const std::vector<vk::ExtensionProperties>& getSupportedExtensions() const { return m_supportedExtensions; }

private:
    void detectProperties();

    vk::PhysicalDevice m_physicalDevice;
    VkPropertiesInfo m_vkPropertiesInfo;
    VkFeaturesInfo m_vkFeaturesInfo;
    std::vector<vk::ExtensionProperties> m_supportedExtensions;
    std::vector<vk::QueueFamilyProperties> m_queueFamilyProperties;
    vk::PhysicalDeviceMemoryProperties m_memoryProperties;
    bool m_supportsSynchronization2 = false;
    bool m_supportsSubgroupSizeControl = false;
    bool m_supportsSamplerFilterMinMax = false;
    bool m_supportsBarycentric = false;
    bool m_supportsMemoryDecompression = false;
    bool m_supportsCalibratedTimestamps = false;
    bool m_supportsShaderExecutionReorder = false;
};

struct VkFn
{
    PFN_vkCmdBeginPerTileExecutionQCOM cmdBeginPerTileExecutionQCOM = nullptr;
    PFN_vkCmdEndPerTileExecutionQCOM cmdEndPerTileExecutionQCOM = nullptr;
    PFN_vkCmdDispatchTileQCOM cmdDispatchTileQCOM = nullptr;
    PFN_vkGetFramebufferTilePropertiesQCOM getFramebufferTilePropertiesQCOM = nullptr;
    PFN_vkCmdBindTileMemoryQCOM cmdBindTileMemoryQCOM = nullptr;
};

class VkDevice final : public Device
{
public:
    struct Description
    {
        Description(const Device::Description& desc)
            : base(desc)
        {
        }

        Device::Description base;
    };

    VkDevice(Context* context, vk::Instance instance, const VkPhysicalDevice& physicalDevice, const Description& desc);

    VkDevice(Context* context, vk::Instance instance, const vk::PhysicalDevice& physicalDevice, const Description& desc)
        : VkDevice(context, instance, VkPhysicalDevice(physicalDevice), desc)
    {
    }

    VkDevice(Context* context,
             vk::Instance instance,
             vk::PhysicalDevice physicalDevice,
             vk::Device device,
             const Description& desc,
             const char** clientInstanceExtensions,
             size_t clientInstanceExtensionsCount,
             const char** clientDeviceExtensions,
             size_t clientDeviceExtensionsCount);

    ~VkDevice();

    void* getNativeDevice() override { return &m_device.get(); }

    qre::RefCountPtr<PipelineLayout> createPipelineLayout(const PipelineLayout::Description& desc) override;
    qre::RefCountPtr<ComputePipeline> createComputePipeline(const ComputePipeline::Description& desc) override;
    qre::RefCountPtr<GraphicsPipeline> createGraphicsPipeline(const GraphicsPipeline::Description& desc) override;
    qre::RefCountPtr<GraphicsPipeline> createGraphicsPipeline(const VkGraphicsPipeline::Description& desc);
    qre::RefCountPtr<RayTracingPipeline> createRayTracingPipeline(const RayTracingPipeline::Description& desc) override;

    qre::RefCountPtr<Buffer> createBuffer(const Buffer::Description& desc) override;
    qre::RefCountPtr<Texture> createTexture(const Texture::Description& desc) override;

    qre::RefCountPtr<Texture> createTextureFromHandle(const Texture::Description& desc, Texture::Handle handle, Texture::Handle handle2 = nullptr) override
    {
        return createTextureFromVkHandle(desc, (VkImage)handle, (VkImageView)handle2);
    }

    qre::RefCountPtr<Texture> createTextureFromVkHandle(const Texture::Description& desc, VkImage image, VkImageView imageView = VK_NULL_HANDLE);

    qre::RefCountPtr<Texture> createTextureFromVkImage(const Texture::Description& desc, VkImage image) { return createTextureFromVkHandle(desc, image); }

    qre::RefCountPtr<Sampler> createSampler(const Sampler::Description& desc) override;

    qre::RefCountPtr<Shader> createShader(const Shader::Description& desc, const void* byteCode, size_t length) override;

    qre::RefCountPtr<DescriptorSet> createDescriptorSet(const DescriptorSet::Description& desc) override;
    qre::RefCountPtr<DescriptorSetLayout> createDescriptorSetLayout(const DescriptorSetLayout::Description& desc) override;
    void updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSetWrite* writes, uint32_t numWrites = 1) override;

    qre::RefCountPtr<CommandList> createCommandList(CommandQueue* commandQueue, const CommandList::Description& desc) override;

    qre::RefCountPtr<CommandQueue> createCommandQueue(const CommandQueue::Description& desc) override;

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
    qre::RefCountPtr<qhwi::compressedstorage::File> createStorageFile() override;
#endif

    ext::VkSurface* getVkWindowSurface() { return &m_windowSurface; }

    const ext::VkSurface* getVkWindowSurface() const { return &m_windowSurface; }

    void setWindowSurface(const ext::WindowSurface& windowSurface) override;

    void getVkSurfaceFormats(uint32_t* numSurfaceFormatsPtr, vk::SurfaceFormatKHR* surfaceFormats = nullptr) const
    {
        const auto surface = getVkWindowSurface();
        QHWI_ASSERT(surface);
        m_physicalDevice.getVkSurfaceFormats(surface->get(), numSurfaceFormatsPtr, surfaceFormats);
    }

    ext::Swapchain* createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc) override;
    ext::Swapchain* createSwapchain(CommandQueue* commandQueue, const ext::Swapchain::Description& desc, const vk::SwapchainCreateInfoKHR& createInfo);

    qre::RefCountPtr<Fence> createFence(FenceState initialState = FenceState_Unsignaled, const char* name = "") override;
    qre::RefCountPtr<TimerQuery> createTimerQuery() override;

    qre::RefCountPtr<AccelerationStructure> createAccelerationStructure(const AccelerationStructure::Description& desc) override;
    size_t getBottomLevelScratchSize(size_t descriptorCount,
                                     AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                     AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) override;
    AccelerationStructureBuildInfo getAccelerationStructureBuildInfo(const BottomLevelDescriptor* bottomLevelDescriptors,
                                                                     size_t count,
                                                                     void* scratchMemory,
                                                                     AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                                     AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) override;
    qre::RefCountPtr<Buffer> createTopLevelInstanceBuffer(const TopLevelDescriptor* topLevelDescriptors, AccelerationStructure** blasArray, size_t count) override;

    vk::Framebuffer createFramebuffer(const vk::FramebufferCreateInfo& fbCreateInfo) { return m_device->createFramebuffer(fbCreateInfo); }

    vk::UniqueFramebuffer createFramebufferUnique(const vk::FramebufferCreateInfo& fbCreateInfo) { return m_device->createFramebufferUnique(fbCreateInfo); }

    vk::RenderPass createRenderPass(const vk::RenderPassCreateInfo& rpCreateInfo) { return m_device->createRenderPass(rpCreateInfo); }

    vk::UniqueRenderPass createRenderPassUnique(const vk::RenderPassCreateInfo& rpCreateInfo) { return m_device->createRenderPassUnique(rpCreateInfo); }

    AccelerationStructureBuildInfo getAccelerationStructureBuildInfo(size_t instanceCount,
                                                                     AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::eBuild,
                                                                     AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlag_None) override;
    ResultCode getRayTracingShaderGroupHandles(const RayTracingPipeline* pipeline, uint32_t startGroup, uint32_t groupCount, size_t dstSize, void* dst) const override;
    [[deprecated]] bool isTimerQueryResultReady(TimerQuery* timerQuery) override;
    bool getTimerQueryTimeInSec(TimerQuery* timerQuery, CommandQueue* commandQueue, bool blockUntilReady, float& timerInSec) override;

    bool supportsExtension(const char* name) { return m_physicalDevice.supportsExtension(name); }

    bool supportsCalibratedTimestamps() const;

    int32_t findQueueFamilyIndex(vk::QueueFlags queueFlags = (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer),
                                 bool needsPresentSupport = true)
    {
        return m_physicalDevice.findQueueFamilyIndex(m_windowSurface.get(), queueFlags, needsPresentSupport);
    }

    int32_t findOpticalFlowQueueFamilyIndex() { return findQueueFamilyIndex(vk::QueueFlagBits::eOpticalFlowNV, false); }

    int32_t findDataGraphQueueFamilyIndex() { return findQueueFamilyIndex((vk::QueueFlagBits)VK_QUEUE_DATA_GRAPH_BIT_ARM, false); }

    GraphicsBackend getGraphicsBackend() const override;

    Device::Description* desc() override { return &m_desc.base; }

    const Device::Description* desc() const override { return &m_desc.base; }

    vk::Device get() { return m_device.get(); }

    VkContext* getVkContext();

    vk::Instance getInstance() { return m_instance; }

    [[deprecated]] vk::CommandPool getCommandPool() { return getDefaultCommandPool(); }

    vk::CommandPool getDefaultCommandPool() { return m_defaultCommandPool; }

    vk::CommandPool getOpticalFlowCommandPool() { return m_opticalFlowCommandPool; }

    vk::PipelineCache getPipelineCache() { return m_pipelineCache.get(); }

    qre::StaticVector<vk::UniqueDescriptorPool, MaxDescriptorHeaps>& getDescriptorPools() { return m_descriptorPool; }

    vk::DescriptorPool getDescriptorPool(uint32_t index = 0) { return m_descriptorPool[index].get(); }

    vk::DescriptorPool createDescriptorPool();

    vk::QueryPool getTimerQueryPool() { return m_timerQueryHeap.getTimerQueryHeap(); };

    int32_t getMemoryType(uint32_t supportedTypesBitmask, vk::MemoryPropertyFlags memoryPropertyFlags)
    {
        return m_physicalDevice.getMemoryType(supportedTypesBitmask, memoryPropertyFlags);
    }

    constexpr bool isUnifiedMemoryArchitecture() override
    {
        if (m_desc.base.emulateUma)
        {
            return true;
        }
        return m_physicalDevice.isUnifiedMemoryArchitecture();
    }

    const VkFeaturesInfo& getVkFeaturesInfo() const { return m_physicalDevice.getVkFeaturesInfo(); }

    bool supportsDeviceAddress() const { return getVkFeaturesInfo().supportsDeviceAddress(); }

    bool supportsSynchronization2() const { return m_physicalDevice.supportsSynchronization2(); }

    bool supportsSamplerFilterMinMax() const { return m_physicalDevice.supportsSamplerFilterMinMax(); }

    bool supportsDepthResolveMode(ResolveMode mode) const { return m_physicalDevice.supportsDepthResolveMode(mode); }

    bool supportsStencilResolveMode(ResolveMode mode) const { return m_physicalDevice.supportsStencilResolveMode(mode); }

    bool supportsImageProcessingQCOM() const override { return getVkFeaturesInfo().supportsImageProcessingQCOM(); };

    bool supportsMultiView() const override { return getVkFeaturesInfo().supportsMultiView(); }

    bool supportsMultiViewPerViewViewportsQCOM() const override { return getVkFeaturesInfo().supportsMultiViewPerViewViewportsQCOM(); };

    bool supportsMultiViewPerViewRenderAreasQCOM() const override { return getVkFeaturesInfo().supportsMultiViewPerViewRenderAreasQCOM(); };

    bool supportsOpticalFlowNV() const override { return getVkFeaturesInfo().supportsOpticalFlowNV(); }

    bool supportsDynamicRendering() const
    {
        if (!m_desc.base.enableVkDynamicRendering)
        {
            return false;
        }
        return getVkFeaturesInfo().supportsDynamicRendering();
    }

    bool supportsDynamicRenderingLocalRead() const { return getVkFeaturesInfo().supportsDynamicRenderingLocalRead(); }

    bool supportsCustomResolve() const override
    {
        return Device::supportsCustomResolve() && supportsDynamicRendering() && supportsDynamicRenderingLocalRead() && isExtensionEnabled(VK_EXT_CUSTOM_RESOLVE_EXTENSION_NAME);
    }

    bool debugUtilsEnabled() const { return m_debugUtilsInstanceExtensionEnabled; }

    bool isExtensionEnabled(const char* name) const;

    FormatSupportMask queryFormatSupport(Format format) const override;

    size_t getConstantBufferOffsetAlignment() const override;

    size_t getMinStorageBufferOffsetAlignment() const override;

    vk::PhysicalDeviceProperties getPhysicalDeviceProperties() const { return m_physicalDevice.getProperties2().properties; }

    vk::Instance getInstance() const { return m_instance; }

    vk::PhysicalDevice getPhysicalDevice() const { return m_physicalDevice.get(); }

    const VkPhysicalDevice& getPhysicalDevice1() const { return m_physicalDevice; }

    vk::PhysicalDeviceProperties2 getPhysicalDeviceProperties2() const { return m_physicalDevice.getProperties2(); }

    vk::PhysicalDeviceFeatures2 getPhysicalDeviceFeatures2() const { return m_physicalDevice.getFeatures2(); }

    vk::PhysicalDeviceImageProcessingPropertiesQCOM getPhysicalDeviceImageProcessingPropertiesQCOM() const { return m_physicalDevice.getImageProcessingPropertiesQCOM(); };

    vk::PhysicalDeviceConservativeRasterizationPropertiesEXT getPhysicalDeviceConservativeRasterizationPropertiesEXT() const
    {
        return m_physicalDevice.getConservativeRasterizationPropertiesEXT();
    };

    vk::ImageView getImageView(const VkTexture* texture, vk::ImageViewType imageViewType = vk::ImageViewType::e2D, vk::ImageSubresourceRange* subresourceRange = nullptr)
    {
        return m_imageViewCache.get(texture, imageViewType, subresourceRange);
    }

    vk::QueryPool getQueryHeap() { return m_timerQueryHeap.getTimerQueryHeap(); };

    void onDestroyResource(vk::Image image) { m_imageViewCache.onDestroyImage(image); }

    void setFlipViewportY(bool v) { m_flipViewportY = v; }

    bool flipViewportY() const { return m_flipViewportY; }

    const VkPropertiesInfo& getVkPropertiesInfo() const { return m_physicalDevice.getVkPropertiesInfo(); }

    const VkFn& fn() const { return m_fn; }

    void getFramebufferTilePropertiesQCOM(VkFramebuffer framebuffer, uint32_t* pPropertiesCount, VkTilePropertiesQCOM* pProperties);

    VkTilePropertiesQCOM getTilePropertiesQCOM(uint32_t numColorAttachments,
                                               const ColorAttachmentDescription* colorAttachments,
                                               const DepthStencilAttachmentDescription* depthStencilAttachment,
                                               const MultiViewState* multiViewState = nullptr,
                                               const Rect2D* multiViewPerViewRenderAreasQCOM = nullptr,
                                               const uint32_t* colorAttachmentRemapping = nullptr);

    const VmaAllocator& getVmaAllocator() { return m_vmaAllocator; }

    const VmaPool& getVmaSharedAllocationsPool() { return m_sharedAllocationsPool; }

    void logMemoryReport() override;
    void saveMemoryAllocations(const char* fileName = "VmaMemoryReport.json") override;

    vk::DeviceMemory tileMem() const { return m_tileMem.get(); }

    vk::DeviceSize allocateTileMemory(vk::DeviceSize size, vk::DeviceSize alignment);
    void freeTileMemory(vk::DeviceSize offset);

    int32_t defaultQueueFamilyIndex() const { return m_defaultQueueFamilyIndex; }

    int32_t opticalFlowQueueFamilyIndex() const { return m_opticalFlowQueueFamilyIndex; }

    int32_t dataGraphQueueFamilyIndex() const { return m_dataGraphQueueFamilyIndex; }

private:
    struct InitFromHandleParams
    {
        vk::Device device = VK_NULL_HANDLE;
        std::span<const char*> instanceExtensions;
        std::span<const char*> deviceExtensions;
    };

    void init(Context* context, vk::Instance instance, const VkPhysicalDevice& physicalDevice, const Description& desc, const InitFromHandleParams* initFromHandleParams = nullptr);
    vk::Instance m_instance;
    VkPhysicalDevice m_physicalDevice;

    Description m_desc;
    vk::UniqueDevice m_device;
    bool m_ownsDeviceObject;
    static constexpr uint32_t MaxCommandPools = 2;
    vk::UniqueCommandPool m_commandPool[MaxCommandPools];
    vk::CommandPool m_defaultCommandPool, m_opticalFlowCommandPool;
    vk::UniquePipelineCache m_pipelineCache;
    qre::StaticVector<vk::UniqueDescriptorPool, MaxDescriptorHeaps> m_descriptorPool;
    ext::VkSurface m_windowSurface;
    VkImageViewCache m_imageViewCache;
    VkTimerQueryHeap m_timerQueryHeap;
    VkFeaturesInfo m_vkVkFeaturesInfo;

    bool m_debugUtilsInstanceExtensionEnabled = false;

    std::vector<vk::OpticalFlowImageFormatPropertiesNV> m_opticalFlowInputFormats, m_opticalFlowOutputFormats;

    void createInternalResources();

    int32_t m_defaultQueueFamilyIndex = -1;
    int32_t m_opticalFlowQueueFamilyIndex = -1;
    int32_t m_dataGraphQueueFamilyIndex = -1;

    bool m_flipViewportY = true;

    qre::StaticVector<const char*, MaxVkDeviceExtensions> m_enabledDeviceExtensions;
    VkFn m_fn;
    VmaAllocator m_vmaAllocator{};
    VkExportMemoryAllocateInfoKHR m_vmaMemoryAllocateInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR, nullptr, 0 };
    VmaPool m_sharedAllocationsPool;

    VkDeviceSize m_tileMemSize = 0;
    uint32_t m_tileMemHeapIndex = (uint32_t)-1;
    uint32_t m_tileMemMemoryTypeIndex = (uint32_t)-1;
    vk::MemoryType m_tileMemMemoryType{};
    vk::UniqueDeviceMemory m_tileMem;
    vk::DeviceSize m_tileMemOffset = 0;
};

inline qhwi::VkDevice* vk(qhwi::Device* p)
{
    return static_cast<qhwi::VkDevice*>(p);
}

class VkDebugUtilsMessenger final
{
public:
    void init(vk::Instance vkInstance, const vk::DebugUtilsMessengerCreateInfoEXT& createInfo);

private:
    vk::UniqueDebugUtilsMessengerEXT m_vkDebugUtilsMessenger;
};

class VkContext final : public Context
{
public:
    struct Description
    {
        Description(const Context::Description& desc);

        Description& setDebugUtilsMessengerCreateInfo(const vk::DebugUtilsMessengerCreateInfoEXT& info)
        {
            vk.debugUtilsMessengerCreateInfo = info;
            return *this;
        }

        Context::Description base;

        struct
        {
            vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
        } vk;
    };

    VkContext(const Description& desc);

    ResultCode init() override { return ResultCode::eSuccess; };

    qre::RefCountPtr<Device> createDevice(const Device::Description& desc) override;
    qre::RefCountPtr<qhwi::Device> createDevice(VkPhysicalDevice physicalDevice, const Device::Description& desc);

    class EnumeratePhysicalDeviceParams
    {
    public:
        friend class VkContext;

        EnumeratePhysicalDeviceParams();
        typedef bool (*MeetsRequirementsCb)(VkPhysicalDevice&);
        typedef bool (*CompareOp)(const vk::PhysicalDevice&, const vk::PhysicalDevice&);

        EnumeratePhysicalDeviceParams& setMeetsRequirementsCallback(MeetsRequirementsCb cb)
        {
            meetsRequirementsCb = cb;
            return *this;
        }

        EnumeratePhysicalDeviceParams& setCompareOp(CompareOp cb)
        {
            compareOp = cb;
            return *this;
        }

    private:
        static int rankDeviceType(vk::PhysicalDeviceType type);
        MeetsRequirementsCb meetsRequirementsCb = nullptr;
        CompareOp compareOp = nullptr;
    };

    std::vector<qhwi::VkPhysicalDevice> enumeratePhysicalDevices(const EnumeratePhysicalDeviceParams& params = {});
    std::vector<vk::PhysicalDevice> enumeratePhysicalDevices1(const EnumeratePhysicalDeviceParams& params = {});

    vk::Instance instance() const { return m_vkInstance.get(); }

    Context::Description* desc() override { return &m_desc.base; }

    const Context::Description* desc() const override { return &m_desc.base; }

    static bool supportsInstanceExtension(const std::span<const vk::ExtensionProperties>& extensions, const char* name);
    bool supportsInstanceExtension(const char* name) const;
    static bool supportsInstanceExtensions(const std::span<const vk::ExtensionProperties>& extensions, const std::span<const char* const>& names);

    bool supportsInstanceExtensions(const std::span<const char* const>& names) const { return supportsInstanceExtensions(m_supportedInstanceExtensions, names); }

    bool isInstanceExtensionEnabled(const char* name) const;

    static bool supportsInstanceLayer(const std::span<const vk::LayerProperties>& layers, const char* name);
    bool supportsInstanceLayer(const char* name) const;
    static bool supportsInstanceLayers(const std::span<const vk::LayerProperties>& layers, const std::span<const char* const>& names);

    bool supportsInstanceLayers(const std::span<const char* const>& names) const { return supportsInstanceLayers(m_supportedInstanceLayers, names); }

    bool isInstanceLayerEnabled(const char* name) const;

    static qre::StaticVector<const char*, MaxVkInstanceExtensions> getDefaultRequiredInstanceExtensions()
    {
        return { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_SURFACE_EXTENSION_NAMES };
    }

    static qre::StaticVector<const char*, MaxVkInstanceExtensions> getDefaultOptionalInstanceExtensions()
    {
        return { VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                 VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
                 VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
                 VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
                 VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
                 VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };
    }

    bool debugUtilsEnabled() const { return m_debugUtilsEnabled; }

    const VmaVulkanFunctions* vulkanFunctions() { return &m_vulkanFunctions; }

private:
    Description m_desc;
    vk::UniqueInstance m_vkInstance;
    VkDebugUtilsMessenger m_vkDebugUtilsMessenger;
    qre::StaticVector<vk::ExtensionProperties, MaxVkInstanceExtensions> m_supportedInstanceExtensions;
    qre::StaticVector<vk::LayerProperties, MaxVkInstanceLayers> m_supportedInstanceLayers;
    qre::StaticVector<const char*, MaxVkInstanceExtensions> m_enabledInstanceExtensions;
    qre::StaticVector<const char*, MaxVkInstanceLayers> m_enabledInstanceLayers;
    bool m_debugUtilsEnabled = false;
    VmaVulkanFunctions m_vulkanFunctions = {};
};

using VkContextDesc = VkContext::Description;

vk::PipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo(const Shader* shader, ShaderStage shaderStage, const vk::SpecializationInfo* pSpecializationInfo = nullptr);
vk::PipelineColorBlendAttachmentState vk(const ColorAttachmentBlendState& desc);
vk::PipelineDepthStencilStateCreateInfo vk(const DepthStencilState& depthStencilState);
vk::StencilOpState vk(const StencilOpState& stencilOpState, const DepthStencilState& depthStencilState);
vk::PipelineRasterizationStateCreateInfo vk(const RasterizerState& rasterizerState);
vk::PipelineMultisampleStateCreateInfo vk(const MultisampleState& multisampleState);
vk::PushConstantRange vk(const PushConstantRange& pushConstantRange);

void vk(const BlendState& blendState, VkPipelineColorBlendStateCreateInfo& vkBlendState);
void vk(VkGraphicsPipeline::Description* graphicsPipelineDesc, const VertexInputState& vertexInputState, VkPipelineVertexInputStateCreateInfo& vkVertexInputState);

constexpr vk::Viewport vk(const Viewport& v, bool flipViewportY)
{
    return (flipViewportY) ? vk::Viewport(v.x, v.y + v.h, v.w, -v.h, v.zNear, v.zFar) : vk::Viewport(v.x, v.y, v.w, v.h, v.zNear, v.zFar);
}

constexpr vk::Rect2D vk(const Rect2D& r)
{
    return vk::Rect2D(vk::Offset2D(r.x, r.y), vk::Extent2D(r.w, r.h));
}

constexpr vk::ImageSubresource vkImageSubresource(const Subresource* subresource, vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor)
{
    return vk::ImageSubresource(aspectFlags, subresource ? subresource->mipSlice : 0, subresource ? subresource->arraySlice : 0);
}

constexpr vk::ImageSubresourceLayers vkImageSubresourceLayers(const Subresource* subresource, vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor)
{
    return vk::ImageSubresourceLayers(aspectFlags, subresource ? subresource->mipSlice : 0, subresource ? subresource->arraySlice : 0, 1);
}

constexpr vk::ImageSubresourceRange vkImageSubresourceRange(const SubresourceRange& r, vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor)
{
    // planeSlice selects a single aspect plane of a multi-planar or depth/stencil format:
    //   planeSlice 0 = depth (or plane 0), planeSlice 1 = stencil (or plane 1)
    // If the range targets a specific plane, isolate that aspect symmetrically.
    if (r.start.planeSlice == r.end.planeSlice)
    {
        if (r.start.planeSlice == 0 && (aspectFlags & vk::ImageAspectFlagBits::eDepth))
        {
            aspectFlags = vk::ImageAspectFlagBits::eDepth;
        }
        else if (r.start.planeSlice == 1 && (aspectFlags & vk::ImageAspectFlagBits::eStencil))
        {
            aspectFlags = vk::ImageAspectFlagBits::eStencil;
        }
    }
    return vk::ImageSubresourceRange(aspectFlags, r.start.mipSlice, r.end.mipSlice - r.start.mipSlice + 1, r.start.arraySlice, r.end.arraySlice - r.start.arraySlice + 1);
}

vk::AttachmentDescription vk(const VkDevice* device, const ColorAttachmentDescription& desc);
vk::ImageView vkImageView(const TextureAttachmentData& desc);
vk::Extent3D getExtent(const ColorAttachmentDescription& desc);
vk::Extent3D getExtent(const DepthStencilAttachmentDescription& desc);
vk::AttachmentDescription vk(const VkDevice* device, const DepthStencilAttachmentDescription& desc);

void toVkBufferInfos(const DescriptorBufferInfo* bufferInfos, vk::DescriptorBufferInfo* vkBufferInfos, uint32_t count);
void toVkBufferViews(const DescriptorBufferInfo* bufferInfos, vk::BufferView* vkBufferViews, uint32_t count);
void toVkImageInfos(const DescriptorTextureInfo* textureInfos, vk::DescriptorImageInfo* vkImageInfos, vk::ImageView* imageViews, vk::ImageLayout imageLayout, uint32_t count);
void toVkImageInfos(const DescriptorSamplerInfo* samplerInfos, vk::DescriptorImageInfo* vkImageInfos, uint32_t count);
void toVkAccelStructInfos(const DescriptorAccelerationStructureInfo* accelStructInfos, vk::WriteDescriptorSetAccelerationStructureKHR* vkAccelStructInfos, uint32_t count);

constexpr vk::Offset3D vkOffset3D(uvec3 v)
{
    return vk::Offset3D(v.x, v.y, v.z);
}

constexpr vk::Extent3D vkExtent3D(Extent3D s)
{
    return vk::Extent3D(s.w, s.h, s.d);
}

constexpr vk::ImageSubresource calcSubresourceFromIndex(uint32_t index, uint32_t mipLevels)
{
    vk::ImageSubresource s;
    s.mipLevel = index % mipLevels;
    s.arrayLayer = index / mipLevels;
    return s;
}

vk::ImageAspectFlags vkImageAspectFlagsFromFormat(qhwi::Format format);

bool isReadOnlyAccess(vk::AccessFlags2KHR accessFlags);

namespace hash_util
{
uint64_t Fnv1a64Continue(uint64_t& hash, const vk::ImageViewCreateInfo& v);
};

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
namespace compressedstorage
{

class VkFile final : public File
{
public:
    void getFileInformation(FileInfo& fileInfo) const override { fileInfo.sizeInBytes = m_fileMap.Size(); }

    bool map(const char* filePath);

    unsigned char* data() const { return m_fileMap.Data(); }

private:
    qre::UniqueFileMap<unsigned char> m_fileMap;
};

struct VkRequest : Request
{
    explicit VkRequest(const Request& b)
        : Request(b)
        , intermediateSrc(nullptr)
        , intermediateDst(nullptr)
        , parameterBuffer(nullptr)
        , transferBuffer(nullptr)
        , copyToHostMemory(false)
    {
    }

    qre::RefCountPtr<Buffer> intermediateSrc = nullptr;
    qre::RefCountPtr<Buffer> intermediateDst = nullptr;
    qre::RefCountPtr<Buffer> parameterBuffer = nullptr;
    qre::RefCountPtr<Buffer> transferBuffer = nullptr;
    bool copyToHostMemory = false;
};

class VkStorageStatusArray final : public StorageStatusArray
{
public:
    ResultCode getResult(uint32_t index = 0) const override { return ResultCode::eSuccess; }
};

class VkStorageQueue final : public StorageQueue
{
public:
    VkStorageQueue(VkDevice* device)
        : m_device(device)
        , m_statusArray(nullptr)
        , m_commandQueue(nullptr)
        , m_commandList(nullptr)
        , m_fence(nullptr)
        , m_requests()
        , m_isRecording(false)
    {
        init();
    }

    void enqueueRequest(const Request&) override;

    void enqueueStatus(StorageStatusArray* array, uint32_t index = 0) override { m_statusArray = static_cast<VkStorageStatusArray*>(array); }

    void enqueueSignal(Fence* fence, uint64_t value) override { m_fence = fence; }

    void submit() override;

private:
    void init();
    void enqueueMemToMem(std::span<const uint8_t> src, const DestinationMemory& dst, VkRequest& vkRequest);
    void enqueueMemToBuffer(std::span<const uint8_t> src, const DestinationBuffer& dst, VkRequest& vkRequest);
    void enqueueMemToTextureRegion(std::span<const uint8_t> src, const DestinationTextureRegion& dst, VkRequest& vkRequest);
    void enqueueMemToMultipleSubresources(std::span<const uint8_t> src, const DestinationMultipleSubresources& dst, VkRequest& vkRequest);
    void readTileStreamHeader(std::span<const uint8_t> tileStream, uint32_t& numTiles, const uint32_t*& tileOffsetStart, std::span<const uint8_t>& src) const;

    VkDevice* m_device;
    VkStorageStatusArray* m_statusArray;
    qre::RefCountPtr<CommandQueue> m_commandQueue;
    qre::RefCountPtr<CommandList> m_commandList;
    Fence* m_fence;
    qre::StaticVector<VkRequest, 4096> m_requests;
    bool m_isRecording;
};

class VkFactory final : public Factory
{
public:
    VkFactory(VkDevice* v)
        : m_device(v)
    {
    }

    qre::RefCountPtr<StorageQueue> createQueue(const StorageQueueDesc& desc) const override { return qre::MakeRefCountPtr<StorageQueue>(new VkStorageQueue(m_device)); }

    ResultCode openFile(const char* fileToLoad, File* file) const override;
    qre::RefCountPtr<StorageStatusArray> createStatusArray(uint32_t capacity = 1, const char* name = nullptr) const override;

private:
    VkDevice* m_device;
};

} // namespace compressedstorage
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

} // namespace qhwi

#endif
