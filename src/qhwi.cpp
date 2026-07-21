//
//
//  qhwi.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi.h>
#include <qhwi/qhwi_util.h>
#include <qhwi/qhwi_profiling.h>
#ifdef QHWI_BACKEND_D3D12
#    include <qhwi/qhwi_d3d12.h>
#endif
#ifdef QHWI_BACKEND_VULKAN
#    include <qhwi/qhwi_vk.h>
#endif
#ifdef QHWI_WINDOW_BACKEND_GLFW
#    include <qhwi/qhwi_glfw.h>
#endif
#ifdef QHWI_WINDOW_BACKEND_ANDROID
#    include <qhwi/qhwi_android_window.h>
#endif
#ifdef QHWI_GPU_CAPTURE_BACKEND_PIX
#    include <qhwi/qhwi_pix.h>
#endif
#include <qhwi/qhwi_util.h>
#include <qre/image.h>
#include <qre/memory/align.h>
#include <qre/text/textutil.h>
#include <algorithm>

static constexpr void setBit(uint32_t& mask, uint32_t bit, bool on)
{
    if (on)
    {
        mask |= bit;
    }
    else
    {
        mask &= ~bit;
    }
}

static qhwi::GraphicsBackend getDefaultGraphicsBackend()
{
    qhwi::GraphicsBackend graphicsBackend;
#if defined(QHWI_PLATFORM_WINDOWS)
#    ifdef QHWI_BACKEND_D3D12
    graphicsBackend = qhwi::GraphicsBackend::eDirectX12;
#    else
    graphicsBackend = qhwi::GraphicsBackend::eVulkan;
#    endif
#elif defined(QHWI_PLATFORM_LINUX)
    graphicsBackend = qhwi::GraphicsBackend::eVulkan;
#elif defined(QHWI_PLATFORM_ANDROID)
    graphicsBackend = qhwi::GraphicsBackend::eVulkan;
#endif
    return graphicsBackend;
}

static qhwi::WindowBackend getDefaultWindowBackend()
{
    qhwi::WindowBackend windowBackend = qhwi::WindowBackend::eAuto;
#if defined(QHWI_PLATFORM_WINDOWS) || defined(QHWI_PLATFORM_LINUX)
#    if defined(QHWI_WINDOW_BACKEND_GLFW)
    windowBackend = qhwi::WindowBackend::eGlfw;
#    endif
#elif defined(QHWI_PLATFORM_ANDROID)
#    if defined(QHWI_WINDOW_BACKEND_ANDROID)
    windowBackend = qhwi::WindowBackend::eAndroid;
#    endif
#endif
    return windowBackend;
}

qhwi::ext::Window* qhwi::Context::createWindow(const ext::Window::Description& desc)
{
    WindowBackend windowBackend = desc.windowBackend;
    if (windowBackend == WindowBackend::eAuto)
    {
        windowBackend = getDefaultWindowBackend();
    }
#ifdef QHWI_WINDOW_BACKEND_GLFW
    if (windowBackend == WindowBackend::eGlfw)
    {
        qhwi::ext::GlfwWindow::Description glfwWindowDesc(desc);
        return new qhwi::ext::GlfwWindow(glfwWindowDesc);
    }
#endif
#ifdef QHWI_WINDOW_BACKEND_ANDROID
    if (windowBackend == WindowBackend::eAndroid)
    {
        qhwi::ext::AndroidWindow::Description androidWindowDesc(desc);
        return new qhwi::ext::AndroidWindow(androidWindowDesc);
    }
#endif
    return nullptr;
}

qhwi::ext::Window* qhwi::Context::createWindowFromHandle(const ext::Window::Description& desc, const ext::WindowSurface& surface)
{
    return nullptr;
}

bool qhwi::Device::supportsDepthStencilFormat(Format format) const
{
    auto mask = queryFormatSupport(format);
    return (mask & FormatSupport_DepthStencilAttachment) ? true : false;
}

qre::RefCountPtr<qhwi::Context> qhwi::createContext(const Context::Description& desc)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("qhwi::createContext", QHWI_COLOR_CONTEXT);

    GraphicsBackend graphicsBackend = desc.graphicsBackend;
    ResultCode resultCode = ResultCode::eErrorOther;
    qre::RefCountPtr<Context> context;
    if (graphicsBackend == GraphicsBackend::eAuto)
    {
        graphicsBackend = getDefaultGraphicsBackend();
    }
#ifdef QHWI_BACKEND_D3D12
    if (graphicsBackend == qhwi::GraphicsBackend::eDirectX12)
    {
        qhwi::D3dContext::Description d3dContextDesc(desc);
        context = qre::MakeRefCountPtr<qhwi::Context>(new qhwi::D3dContext(d3dContextDesc));
    }
#endif
#ifdef QHWI_BACKEND_VULKAN
    else if (graphicsBackend == qhwi::GraphicsBackend::eVulkan)
    {
        qhwi::VkContext::Description vkContextDesc(desc);
        context = qre::MakeRefCountPtr<qhwi::Context>(new qhwi::VkContext(vkContextDesc));
    }
#endif

    if (context)
    {
        resultCode = context->init();

        if (resultCode == ResultCode::eSuccess)
        {
            return context;
        }
        else
        {
            return nullptr;
        }
    }

    QHWI_FATAL("create context failed");
}

static bool getDefaultGpuBackend(qhwi::ext::GpuCaptureBackend& b)
{
#if defined(QHWI_GPU_CAPTURE_BACKEND_PIX)
    b = qhwi::ext::GpuCaptureBackend::ePix;
    return true;
#else
    return false;
#endif
}

qhwi::ext::GpuCapture* qhwi::ext::createGpuCapture(const char* path, const GpuCaptureDesc& desc)
{
    GpuCaptureBackend backend = desc.backend;
    if (backend == GpuCaptureBackend::eAuto)
    {
        if (!getDefaultGpuBackend(backend))
        {
            return nullptr;
        }
    }
#ifdef QHWI_GPU_CAPTURE_BACKEND_PIX
    if (backend == GpuCaptureBackend::ePix)
    {
        return new PixGpuCapture(path);
    }
#endif
    return nullptr;
}

uint32_t qhwi::Texture::calculateMipLevels(uint32_t w, uint32_t h, uint32_t d)
{
    return qre::ImageLevelCount(w, h, d);
}

int qhwi::PipelineLayout::Description::find(const DescriptorSetLayout* layout) const
{
    for (uint32_t j = 0; j < descriptorSetLayouts.size(); j++)
    {
        if (descriptorSetLayouts[j] == layout)
        {
            return j;
        }
    }
    QHWI_ERR("descriptor set layout not found");
}

void qhwi::UploadPool::compact(size_t threshold)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("UploadPool::compact", QHWI_COLOR_TRANSFER);

    if (m_size < threshold)
    {
        return;
    }
    for (auto& [_, entries] : m_entries)
    {
        for (size_t entryIndex = 0; entryIndex < entries.size();)
        {
            const Entry& entry = entries[entryIndex];
            if (entry.end == 0)
            {
                m_size -= entry.size;

                entries[entryIndex].buffer.Get()->unmap();

                // Swap and pop
                std::swap(entries[entryIndex], entries[entries.size() - 1]);
                entries.pop_back();
            }
            else
            {
                entryIndex++;
            }
        }
    }
}

void qhwi::UploadPool::reset(CommandList* commandList)
{
    std::vector<Entry>& entries = m_entries[commandList];
    for (Entry& entry : entries)
    {
        entry.end = 0;
    }
}

bool qhwi::UploadPool::get(Device* device, CommandList* commandList, size_t size, Buffer*& buffer, size_t& offset, size_t alignment)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("UploadPool::get", QHWI_COLOR_TRANSFER);

    std::vector<Entry>& entries = m_entries[commandList];

    if (entries.empty())
    {
        entries.reserve(InitialNumEntries);
    }

    for (Entry& entry : entries)
    {
        if ((qre::AlignUp(entry.end, alignment) + size) <= entry.size)
        {
            buffer = entry.buffer.Get();
            offset = qre::AlignUp(entry.end, alignment);
            entry.end = offset + size;
            return true;
        }
    }
    Entry entry;
    char name[MaxStringSize];
    qre::PrintfSafe(name, "UploadManager_StagingBuffer[%zd]", m_entries.size());
    entry.size = std::max(MinBufferSize, size);

    entry.buffer = device->createBuffer({ .heapType = HeapType::eUpload, .size = entry.size, .name = name });
    buffer = entry.buffer.Get();
    m_size += buffer->size();
    offset = qre::AlignUp(entry.end, alignment);
    entry.end = offset + size;
    entries.push_back(entry);

    return true;
}

bool qhwi::operator==(const qhwi::CommandList::GraphicsResourceState& lhs, const qhwi::CommandList::GraphicsResourceState& rhs)
{
    for (uint32_t j = 0; j < lhs.MaxVertexBuffers; j++)
    {
        if (lhs.vertexBuffers[j] != rhs.vertexBuffers[j])
        {
            return false;
        }
    }
    if (lhs.indexBuffer != rhs.indexBuffer)
    {
        return false;
    }
    if (lhs.indirectBuffer != rhs.indirectBuffer)
    {
        return false;
    }
    for (uint32_t j = 0; j < MaxDescriptorSetsPerPipeline; j++)
    {
        if (lhs.descSets[j] != rhs.descSets[j])
        {
            return false;
        }
    }
    return true;
}

bool qhwi::operator==(const qhwi::CommandList::ComputeResourceState& lhs, const qhwi::CommandList::ComputeResourceState& rhs)
{
    for (uint32_t j = 0; j < MaxDescriptorSetsPerPipeline; j++)
    {
        if (lhs.descSets[j] != rhs.descSets[j])
        {
            return false;
        }
    }
    return true;
}

void qhwi::CommandList::GraphicsResourceState::setVertexBuffer(uint32_t slot, Buffer* b)
{
    vertexBuffers[slot] = b;
}

void qhwi::CommandList::GraphicsResourceState::setIndexBuffer(Buffer* b)
{
    indexBuffer = b;
}

void qhwi::CommandList::GraphicsResourceState::setIndirectBuffer(Buffer* b)
{
    indirectBuffer = b;
}

void qhwi::CommandList::GraphicsResourceState::setDescSet(uint32_t slot, DescriptorSet* ds)
{
    QHWI_ASSERT(slot < MaxDescriptorSetsPerPipeline && ds != nullptr);
    descSets[slot] = ds;
}

void qhwi::CommandList::ComputeResourceState::setIndirectBuffer(Buffer* b)
{
    indirectBuffer = b;
}

void qhwi::CommandList::ComputeResourceState::setDescSet(uint32_t slot, DescriptorSet* ds)
{
    QHWI_ASSERT(slot < MaxDescriptorSetsPerPipeline && ds != nullptr);
    descSets[slot] = ds;
}

void qhwi::CommandList::RayTracingResourceState::setDescSet(uint32_t slot, DescriptorSet* ds)
{
    QHWI_ASSERT(slot < MaxDescriptorSetsPerPipeline && ds != nullptr);
    descSets[slot] = ds;
}

qhwi::CommandList::CommandList()
    : RefCounterDelete()
{
    m_resourcesUsedByHw.reserve(ResourcesUsedByHw_InitialSize);
    memset(m_referencedTimerQueries, 0, sizeof(m_referencedTimerQueries));
}

qhwi::CommandList::~CommandList()
{
    while (!m_resourcesUsedByHw.empty())
    {
        auto e = m_resourcesUsedByHw.extract(m_resourcesUsedByHw.begin());
        e.value()->Release();
    }
}

void qhwi::CommandList::onBegin()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::onBegin", QHWI_COLOR_COMMAND_LIST);

    device()->uploadPool().reset(this);

    while (!m_resourcesUsedByHw.empty())
    {
        auto e = m_resourcesUsedByHw.extract(m_resourcesUsedByHw.begin());
        e.value()->Release();
    }

    m_usedResourceCount = 0;
}

uint32_t qhwi::DescriptorSetLayout::Description::getBindingSetOffset(DescriptorType type) const
{
    switch (type)
    {
    case DescriptorType::eConstantBuffer:
    case DescriptorType::eDynamicConstantBuffer:
        return bindingSetOffsets[uint8_t(BindingOffset::eBShift)];

    case DescriptorType::eByteAddressBuffer:
    case DescriptorType::eStructuredBuffer:
    case DescriptorType::eTypedBuffer:
    case DescriptorType::eTexture:
    case DescriptorType::eInputAttachment:
    case DescriptorType::eAccelerationStructure:
    case DescriptorType::eBlockMatchImageQCOM:
    case DescriptorType::eSampleWeightImageQCOM:
        return bindingSetOffsets[uint8_t(BindingOffset::eTShift)];

    case DescriptorType::eRWByteAddressBuffer:
    case DescriptorType::eRWStructuredBuffer:
    case DescriptorType::eRWTypedBuffer:
    case DescriptorType::eRWTexture:
        return bindingSetOffsets[uint8_t(BindingOffset::eUShift)];

    case DescriptorType::eSampler:
        return bindingSetOffsets[uint8_t(BindingOffset::eSShift)];

    default:
        QHWI_FATAL("Invalid descriptor type");
    }
}

uint32_t qhwi::DescriptorSetLayout::Description::getBindingLayoutIndex(DescriptorType type, uint32_t shaderRegister) const
{
    DescriptorSetLayoutBinding bindingSearchKey{
        .shaderRegister = shaderRegister,
        .type = type,
    };

    auto iter = std::lower_bound(bindings.begin(), bindings.end(), bindingSearchKey);
    uint32_t index = uint32_t(iter - bindings.begin());

    QHWI_ASSERT_MSG((iter != bindings.end() && (iter->type == bindingSearchKey.type && iter->shaderRegister == bindingSearchKey.shaderRegister)),
                    "binding not found, type: %d register: %d",
                    type,
                    shaderRegister);

    return index;
}

void qhwi::DescriptorSetDesc::reorderBindings()
{
    auto prevBindings = bindings;
    bindings.clear();
    bindings.resize(layoutDesc()->bindings.size());
    for (uint32_t j = 0; j < prevBindings.size(); j++)
    {
        const auto& b = prevBindings[j];
        uint32_t newIndex = layoutDesc()->getBindingLayoutIndex(b->type(), b->shaderRegister());
        bindings[newIndex] = b;
    }
}

void qhwi::Device::createDescriptorSetAndLayout(const qhwi::DescriptorSet::Description& dsDesc,
                                                ShaderStageMask shaderStages,
                                                uint32_t registerSpace,
                                                qre::RefCountPtr<DescriptorSetLayout>& descriptorSetLayout,
                                                qre::RefCountPtr<DescriptorSet>& descriptorSet,
                                                const BindingSetOffsets& bindingSetOffsets)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("Device::createDescriptorSetAndLayout", QHWI_COLOR_RESOURCE_CREATE);

    qhwi::DescriptorSetDesc descriptorSetDesc = dsDesc;
    qhwi::DescriptorSetLayoutDesc descriptorSetLayoutDesc;

    char name[MaxStringSize];
    qre::PrintfSafe(name, "%s_layout", (const char*)descriptorSetDesc.name);

    descriptorSetLayoutDesc.setName(name).setRegisterSpace(registerSpace).setBindingSetOffsets(bindingSetOffsets);

    auto& dsBindings = descriptorSetDesc.bindings;
    size_t numBindings = descriptorSetDesc.numBindings();
    for (size_t i = 0; i < numBindings; ++i)
    {
        auto& dsBinding = dsBindings[i];
        descriptorSetLayoutDesc.addBinding({ .shaderRegister = dsBinding->shaderRegister(), .type = dsBinding->type(), .shaderStages = shaderStages });
    }

    descriptorSetLayout = createDescriptorSetLayout(descriptorSetLayoutDesc);

    descriptorSetDesc.layout = descriptorSetLayout;
    descriptorSet = createDescriptorSet(descriptorSetDesc);
}

void qhwi::Device::updateDescriptorSet(DescriptorSet* descriptorSet, DescriptorSetWrite* writes, uint32_t numWrites)
{
    auto dsDesc = descriptorSet->desc();
    auto& dsBindings = dsDesc->bindings;

    for (uint32_t j = 0; j < numWrites; ++j)
    {
        const DescriptorSetWrite& dsWrite = writes[j];

        if (dsWrite.bufferInfos)
        {
            qhwi::DescriptorSetBinding* pBinding = dsDesc->findBinding(dsWrite.type, dsWrite.shaderRegister);

            if (!pBinding)
            {
                auto binding = qre::MakeRefCountPtr(new DescriptorSetBinding);
                pBinding = binding;
                binding->initAsBufferInfos(dsWrite.arrayIndex + dsWrite.count, dsWrite.type, dsWrite.shaderRegister);
                auto bindingLayoutIndex = descriptorSet->layoutDesc()->getBindingLayoutIndex(dsWrite.type, dsWrite.shaderRegister);
                dsBindings[bindingLayoutIndex] = binding;
            }

            for (uint32_t k = dsWrite.arrayIndex; k < dsWrite.arrayIndex + dsWrite.count; k++)
            {
                pBinding->bufferInfo(k) = dsWrite.bufferInfos[k];
            }
        }

        if (dsWrite.textureInfos)
        {
            qhwi::DescriptorSetBinding* pBinding = dsDesc->findBinding(dsWrite.type, dsWrite.shaderRegister);

            if (!pBinding)
            {
                auto binding = qre::MakeRefCountPtr(new DescriptorSetBinding);
                pBinding = binding;
                binding->initAsTextureInfos(dsWrite.arrayIndex + dsWrite.count, dsWrite.type, dsWrite.shaderRegister);
                auto bindingLayoutIndex = descriptorSet->layoutDesc()->getBindingLayoutIndex(dsWrite.type, dsWrite.shaderRegister);
                dsBindings[bindingLayoutIndex] = binding;
            }

            for (uint32_t k = dsWrite.arrayIndex; k < dsWrite.arrayIndex + dsWrite.count; k++)
            {
                pBinding->textureInfo(k) = dsWrite.textureInfos[k];
            }
        }

        if (dsWrite.samplerInfos)
        {
            qhwi::DescriptorSetBinding* pBinding = dsDesc->findBinding(dsWrite.type, dsWrite.shaderRegister);

            if (!pBinding)
            {
                auto binding = qre::MakeRefCountPtr(new DescriptorSetBinding);
                pBinding = binding;
                binding->initAsSamplerInfos(dsWrite.arrayIndex + dsWrite.count, dsWrite.shaderRegister);
                auto bindingLayoutIndex = descriptorSet->layoutDesc()->getBindingLayoutIndex(dsWrite.type, dsWrite.shaderRegister);
                dsBindings[bindingLayoutIndex] = binding;
            }

            for (uint32_t k = dsWrite.arrayIndex; k < dsWrite.arrayIndex + dsWrite.count; k++)
            {
                pBinding->samplerInfo(k) = dsWrite.samplerInfos[k];
            }
        }

        if (dsWrite.accelStructInfos)
        {
            qhwi::DescriptorSetBinding* pBinding = dsDesc->findBinding(dsWrite.type, dsWrite.shaderRegister);

            if (!pBinding)
            {
                auto binding = qre::MakeRefCountPtr(new DescriptorSetBinding);
                pBinding = binding;
                binding->initAsAccelStructInfos(dsWrite.arrayIndex + dsWrite.count, dsWrite.shaderRegister);
                auto bindingLayoutIndex = descriptorSet->layoutDesc()->getBindingLayoutIndex(dsWrite.type, dsWrite.shaderRegister);
                dsBindings[bindingLayoutIndex] = binding;
            }

            for (uint32_t k = dsWrite.arrayIndex; k < dsWrite.arrayIndex + dsWrite.count; k++)
            {
                pBinding->accelStructInfo(k) = dsWrite.accelStructInfos[k];
            }
        }
    }
}

void qhwi::CommandList::updateBuffer(Buffer* dst, std::span<const UpdateBufferRegion> regions)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandList::updateBuffer", QHWI_COLOR_TRANSFER);

    QHWI_ASSERT(m_inTransferPass);

    if (dst->heapType() == HeapType::eUpload)
    {
        QHWI_FATAL("heap type: upload unsupported");
    }

    size_t totalSize = 0;
    for (const auto& r : regions)
    {
        QHWI_ASSERT(r.srcData && r.srcSize);
        totalSize += r.srcSize;
    }

    Buffer* src = nullptr;
    size_t srcOffset = 0;
    if (!device()->uploadPool().get(device(), this, totalSize, src, srcOffset))
    {
        QHWI_FATAL("no staging region found");
    }
    uint8_t* srcAddr = (uint8_t*)src->mapAddr();
    bool firstMapCall = (srcAddr == nullptr);
    if (firstMapCall)
    {
        srcAddr = (uint8_t*)src->map();
    }
    std::vector<CopyBufferRegion> copyBufferRegions(regions.size());
    auto copyBufferRegionsIt = copyBufferRegions.begin();
    for (const auto& r : regions)
    {
        uint8_t* p0 = srcAddr + srcOffset;
        memcpy(p0, r.srcData, r.srcSize);
        *copyBufferRegionsIt++ = { r.dstOffset, srcOffset, r.srcSize };
        srcOffset += r.srcSize;
    }
    copyBuffer(dst, src, copyBufferRegions);

    markResourceAsUsed(dst);
}

void qhwi::CommandList::markResourceAsUsed(qre::RefCounterDelete* object)
{
    QHWI_ASSERT_MSG(object, "Cannot track a null resource.");

    auto [_, inserted] = m_resourcesUsedByHw.insert(object);
    if (inserted)
    {
        object->AddRef();
    }
}

struct MaskIterator
{
    MaskIterator(uint32_t mask)
        : mask(mask)
    {
    }

    bool hasNext() { return (mask != 0); }

    int next()
    {
        int index = qre::math::ffs(mask) - 1;
        mask &= (mask - 1);
        return index;
    }

    uint32_t mask;
};

void qhwi::CommandList::setGraphicsState(bool enableIndexBuffer, bool enableIndirectBuffer)
{
    if (m_nextGraphicsState == m_currentGraphicsState && !m_stateChanged)
    {
        return;
    }
    GraphicsResourceState* rs = &m_nextGraphicsState;
    // clear next state
    setNextState(rs, true, enableIndexBuffer, enableIndirectBuffer);
    // calculate next state
    setNextState(rs, false, enableIndexBuffer, enableIndirectBuffer);
    // transition state, add barriers
    for (Buffer* vb : rs->vertexBuffers)
    {
        if (vb)
        {
            const auto& s = bufferState(vb);
            setState(vb, s.nextState);
        }
    }
    if (enableIndexBuffer && rs->indexBuffer)
    {
        const auto& s = bufferState(rs->indexBuffer);
        setState(rs->indexBuffer, s.nextState);
    }
    MaskIterator descSetIter(m_currentPipelineLayout->descSetMask());
    while (descSetIter.hasNext())
    {
        int slot = descSetIter.next();
        DescriptorSet* ds = rs->descSets[slot];
        if (ds)
        {
            setState(ds);
        }
    }
    if (enableIndirectBuffer && rs->indirectBuffer)
    {
        const auto& s = bufferState(rs->indirectBuffer);
        setState(rs->indirectBuffer, s.nextState);
    }
    m_currentGraphicsState = m_nextGraphicsState;
    m_stateChanged = false;
}

void qhwi::CommandList::setComputeState()
{
    ComputeResourceState* rs = &m_nextComputeState;
    // clear next state
    setNextState(rs, true);
    // calculate next state
    setNextState(rs);
    MaskIterator descSetIter(m_currentPipelineLayout->descSetMask());
    while (descSetIter.hasNext())
    {
        int slot = descSetIter.next();
        DescriptorSet* ds = rs->descSets[slot];
        if (ds)
        {
            setState(ds);
        }
    }
    if (rs->indirectBuffer)
    {
        const auto& s = bufferState(rs->indirectBuffer);
        setState(rs->indirectBuffer, s.nextState);
    }
    m_currentComputeState = m_nextComputeState;
    m_stateChanged = false;
}

void qhwi::CommandList::setRayTracingState()
{
    RayTracingResourceState* rs = &m_nextRayTracingState;
    // clear next state
    setNextState(rs, true);
    // calculate next state
    setNextState(rs);
    MaskIterator descSetIter(m_currentPipelineLayout->descSetMask());
    while (descSetIter.hasNext())
    {
        int slot = descSetIter.next();
        DescriptorSet* ds = rs->descSets[slot];
        if (ds)
        {
            setState(ds);
        }
    }
    m_currentRayTracingState = m_nextRayTracingState;
    m_stateChanged = false;
}

void qhwi::CommandList::setNextState(const GraphicsResourceState* rs, bool clear, bool enableIndexBuffer, bool enableIndirectBuffer)
{
    for (Buffer* vb : rs->vertexBuffers)
    {
        if (vb)
        {
            auto& s = bufferState(vb);
            s.nextState = (clear ? BufferState_NoAccess : BufferState_VertexInput);
        }
    }
    if (enableIndexBuffer && rs->indexBuffer)
    {
        auto& s = bufferState(rs->indexBuffer);
        s.nextState = (clear ? BufferState_NoAccess : (s.nextState | BufferState_IndexInput));
    }
    MaskIterator descSetIter(m_currentPipelineLayout->descSetMask());
    while (descSetIter.hasNext())
    {
        int slot = descSetIter.next();
        DescriptorSet* ds = rs->descSets[slot];
        if (ds)
        {
            setNextState(ds, clear);
        }
    }
    if (enableIndirectBuffer && rs->indirectBuffer)
    {
        auto& s = bufferState(rs->indirectBuffer);
        s.nextState = (clear ? BufferState_NoAccess : (s.nextState | BufferState_DrawIndirect_CommandBufferRead));
    }
}

void qhwi::CommandList::setNextState(const ComputeResourceState* rs, bool clear)
{
    MaskIterator descSetIter(m_currentPipelineLayout->descSetMask());
    while (descSetIter.hasNext())
    {
        int slot = descSetIter.next();
        DescriptorSet* ds = rs->descSets[slot];
        if (ds)
        {
            setNextState(ds, clear);
        }
    }
    if (rs->indirectBuffer)
    {
        auto& s = bufferState(rs->indirectBuffer);
        s.nextState = (clear ? BufferState_NoAccess : (s.nextState | BufferState_DispatchIndirect_CommandBufferRead));
    }
}

void qhwi::CommandList::setNextState(const RayTracingResourceState* rs, bool clear)
{
    MaskIterator descSetIter(m_currentPipelineLayout->descSetMask());
    while (descSetIter.hasNext())
    {
        int slot = descSetIter.next();
        DescriptorSet* ds = rs->descSets[slot];
        if (ds)
        {
            setNextState(ds, clear);
        }
    }
}

qhwi::PipelineStageMask qhwi::CommandList::toPipelineStageMask(qhwi::ShaderStageMask m0)
{
    PipelineStageMask m1 = 0;
    if (m0 & ShaderStage_Vertex)
    {
        m1 |= PipelineStage_VertexShader;
    }
    if (m0 & ShaderStage_Pixel)
    {
        m1 |= PipelineStage_PixelShader;
    }
    if (m0 & ShaderStage_Compute)
    {
        m1 |= PipelineStage_ComputeShader;
    }
    if (m0 & ShaderStage_Domain)
    {
        m1 |= PipelineStage_DomainShader;
    }
    if (m0 & ShaderStage_Hull)
    {
        m1 |= PipelineStage_HullShader;
    }
    if (m0 & ShaderStage_Geometry)
    {
        m1 |= PipelineStage_GeometryShader;
    }
    if (m0 & ShaderStage_Amplification)
    {
        m1 |= PipelineStage_AmplificationShader;
    }
    if (m0 & ShaderStage_Mesh)
    {
        m1 |= PipelineStage_MeshShader;
    }
    if (m0 & ShaderStage_AnyHit)
    {
        m1 |= PipelineStage_RayTracingShader;
    }
    if (m0 & ShaderStage_ClosestHit)
    {
        m1 |= PipelineStage_RayTracingShader;
    }
    if (m0 & ShaderStage_Miss)
    {
        m1 |= PipelineStage_RayTracingShader;
    }
    if (m0 & ShaderStage_RayGeneration)
    {
        m1 |= PipelineStage_RayTracingShader;
    }
    if (m0 & ShaderStage_Intersection)
    {
        m1 |= PipelineStage_RayTracingShader;
    }
    if (m0 & ShaderStage_Callable)
    {
        m1 |= PipelineStage_RayTracingShader;
    }
    return m1;
}

qhwi::AccessModeMask qhwi::CommandList::toAccessModeMask(qhwi::DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::eConstantBuffer:
    case DescriptorType::eDynamicConstantBuffer:
        return AccessMode_ConstantBufferRead;
    case DescriptorType::eByteAddressBuffer:
    case DescriptorType::eStructuredBuffer:
        return AccessMode_StorageRead;
    case DescriptorType::eRWByteAddressBuffer:
    case DescriptorType::eRWStructuredBuffer:
    case DescriptorType::eRWTexture:
        return AccessMode_StorageReadWrite;
    case DescriptorType::eTexture:
        return AccessMode_Sample;
    case DescriptorType::eTypedBuffer:
        return AccessMode_StorageRead;
    case DescriptorType::eRWTypedBuffer:
        return AccessMode_StorageReadWrite;
    case DescriptorType::eInputAttachment:
        return AccessMode_InputAttachmentRead;
    default:
        QHWI_FATAL("unhandled descriptor type: %d", type);
    }
}

qhwi::ImageLayout qhwi::CommandList::toImageLayout(qhwi::DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::eRWTexture:
        return ImageLayout::eUnorderedAccess;
    case DescriptorType::eTexture:
        return ImageLayout::eShaderReadOnlyOptimal;
    case DescriptorType::eInputAttachment:
        return ImageLayout::eLocalRead;
    default:
        return ImageLayout::eUndefined;
    }
}

void qhwi::CommandList::setNextState(const DescriptorSet* ds, bool clear)
{
    size_t numBindings = ds->desc()->numBindings();
    for (size_t i = 0; i < numBindings; i++)
    {
        const auto& binding = ds->desc()->bindings[i];
        if (!binding)
        {
            continue;
        }
        const auto& descSetLayoutBinding = ds->layoutDesc()->bindings[i];
        auto bindingType = ds->desc()->bindingType(uint32_t(i));
        ShaderStageMask shaderStages = descSetLayoutBinding.shaderStages;
        if (isBufferDescriptorType(bindingType))
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& buffer = binding->bufferInfo(j).buffer;
                if (buffer && buffer->automaticBarriersEnabled())
                {
                    BufferState nextState = { toPipelineStageMask(shaderStages), toAccessModeMask(bindingType) };
                    auto& s = bufferState(buffer);
                    s.nextState = (clear ? BufferState_NoAccess : (s.nextState | nextState));
                }
            }
        }
        else if (isTextureDescriptorType(bindingType) || bindingType == DescriptorType::eInputAttachment)
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& textureBindingInfo = binding->textureInfo(j);
                const auto& texture = binding->textureInfo(j).texture;
                const auto& textureDesc = texture->desc();
                if (texture && texture->automaticBarriersEnabled())
                {
                    const TextureState nextState = { toPipelineStageMask(shaderStages), toAccessModeMask(bindingType), toImageLayout(bindingType) };
                    auto& ts1 = textureSubresourceStateTrackers(texture);

                    if (ts1.isUnifiedState())
                    {
                        const auto& s = ts1.getState();
                        auto updatedNextState = clear ? TextureState_NoAccess : s.nextState | nextState;
                        ts1.setNextState(updatedNextState, textureBindingInfo.subresourceRange);
                    }
                    else
                    {
                        const Subresource& start = textureBindingInfo.subresourceRange.start;
                        const Subresource end = { .mipSlice = std::min(textureBindingInfo.subresourceRange.end.mipSlice, uint16_t(textureDesc->mipLevels - 1)),
                                                  .arraySlice = std::min(textureBindingInfo.subresourceRange.end.arraySlice, uint16_t(textureDesc->arrayLayers - 1)),
                                                  .planeSlice = std::min(textureBindingInfo.subresourceRange.end.planeSlice, uint16_t(0)) };

                        for (uint16_t plane = start.planeSlice; plane <= end.planeSlice; ++plane)
                        {
                            for (uint16_t arraySlice = start.arraySlice; arraySlice <= end.arraySlice; ++arraySlice)
                            {
                                for (uint16_t mipSlice = start.mipSlice; mipSlice <= end.mipSlice; ++mipSlice)
                                {
                                    Subresource subresource = { .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = plane };
                                    const auto& s = ts1.getState(subresource);
                                    auto updatedNextState = clear ? TextureState_NoAccess : s.nextState | nextState;
                                    ts1.setNextState(updatedNextState, subresource);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void qhwi::CommandList::setState(const DescriptorSet* ds)
{
    size_t numBindings = ds->desc()->numBindings();
    for (size_t i = 0; i < numBindings; i++)
    {
        const auto& binding = ds->desc()->bindings[i];
        if (!binding)
        {
            continue;
        }
        auto bindingType = ds->desc()->bindingType(uint32_t(i));
        if (isBufferDescriptorType(bindingType))
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& buffer = binding->bufferInfo(j).buffer;
                if (buffer && buffer->automaticBarriersEnabled())
                {
                    const auto& s = bufferState(buffer);
                    setState(buffer, s.nextState);
                }
            }
        }
        else if (isTextureDescriptorType(bindingType) || bindingType == DescriptorType::eInputAttachment)
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& textureInfo = binding->textureInfo(j);
                const auto& texture = textureInfo.texture;
                const auto& textureDesc = texture->desc();
                if (texture && texture->automaticBarriersEnabled())
                {
                    auto& ts1 = textureSubresourceStateTrackers(texture);

                    if (ts1.isUnifiedState())
                    {
                        const auto& s = ts1.getState();
                        setState(texture, s.nextState, textureInfo.subresourceRange);
                    }
                    else
                    {
                        const Subresource& start = textureInfo.subresourceRange.start;
                        const Subresource end = { .mipSlice = std::min(textureInfo.subresourceRange.end.mipSlice, uint16_t(textureDesc->mipLevels - 1)),
                                                  .arraySlice = std::min(textureInfo.subresourceRange.end.arraySlice, uint16_t(textureDesc->arrayLayers - 1)),
                                                  .planeSlice = std::min(textureInfo.subresourceRange.end.planeSlice, uint16_t(0)) };

                        for (uint16_t plane = start.planeSlice; plane <= end.planeSlice; ++plane)
                        {
                            for (uint16_t arraySlice = start.arraySlice; arraySlice <= end.arraySlice; ++arraySlice)
                            {
                                for (uint16_t mipSlice = start.mipSlice; mipSlice <= end.mipSlice; ++mipSlice)
                                {
                                    Subresource subresource = { .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = plane };
                                    const auto& s = ts1.getState(subresource);
                                    setState(texture, s.nextState, subresource);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void qhwi::CommandList::setState(Buffer* buffer, BufferState nextState)
{
    auto& s = bufferState(buffer);
    if (s.currentState == nextState)
    {
        return;
    }
    BufferBarrier b = { buffer, s.currentState, nextState };
    addBufferBarrier(b);
    m_stateChanged = true;
}

void qhwi::CommandList::setState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange)
{
    auto& ts1 = textureSubresourceStateTrackers(texture);

    if (ts1.isUnifiedState())
    {
        const auto& s = ts1.getState();
        if (s.currentState == nextState)
        {
            return;
        }
        TextureBarrier b = { texture, s.currentState, nextState, subresourceRange };
        addTextureBarrier(b);
    }
    else
    {
        const auto& desc = texture->desc();
        const Subresource& start = subresourceRange.start;
        const Subresource end = { .mipSlice = std::min(subresourceRange.end.mipSlice, uint16_t(desc->mipLevels - 1)),
                                  .arraySlice = std::min(subresourceRange.end.arraySlice, uint16_t(desc->arrayLayers - 1)),
                                  .planeSlice = std::min(subresourceRange.end.planeSlice, uint16_t(0)) };

        for (uint16_t plane = start.planeSlice; plane <= end.planeSlice; ++plane)
        {
            for (uint16_t arraySlice = start.arraySlice; arraySlice <= end.arraySlice; ++arraySlice)
            {
                for (uint16_t mipSlice = start.mipSlice; mipSlice <= end.mipSlice; ++mipSlice)
                {
                    Subresource subresource = { .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = plane };
                    const auto& s = ts1.getState(subresource);

                    bool isUavBarrierRequired = (s.currentState.access & AccessMode::AccessMode_ShaderStorageRead && nextState.access & AccessMode_ShaderStorageWrite) ||
                                                (s.currentState.access & AccessMode::AccessMode_ShaderStorageWrite && nextState.access & AccessMode_ShaderStorageReadWrite);

                    if (s.currentState == nextState && !isUavBarrierRequired)
                    {
                        continue;
                    }

                    TextureBarrier b = { texture, s.currentState, nextState, subresource };
                    addTextureBarrier(b);
                }
            }
        }
    }

    m_stateChanged = true;
}

void qhwi::CommandList::notifyState(Buffer* buffer, BufferState nextState)
{
    auto& s = bufferState(buffer);
    if (s.currentState == nextState)
    {
        return;
    }
    s.currentState = nextState;
    m_stateChanged = true;
}

void qhwi::CommandList::notifyState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange)
{
    auto& ts1 = textureSubresourceStateTrackers(texture);
    const auto& s = ts1.getState();
    if (s.currentState == nextState)
    {
        return;
    }
    ts1.setCurrentState(nextState, subresourceRange);
    m_stateChanged = true;
}

qhwi::CommandList::BufferState1& qhwi::CommandList::bufferState(Buffer* buffer)
{
    auto it = m_bufferState.find(buffer);
    if (it == m_bufferState.end())
    {
        it = m_bufferState.insert({ buffer, BufferState1() }).first;
    }
    return it->second;
}

qhwi::CommandList::TextureSubresourceStateTrackers& qhwi::CommandList::textureSubresourceStateTrackers(Texture* texture)
{
    auto it = m_textureState.find(texture);
    if (it == m_textureState.end())
    {
        it = m_textureState.insert({ texture, TextureSubresourceStateTrackers(texture) }).first;
    }

    return it->second;
}

void qhwi::CommandList::setNextState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange)
{
    textureSubresourceStateTrackers(texture).setNextState(nextState, subresourceRange);
}

void qhwi::CommandList::setCurrentState(Texture* texture, TextureState currentState, SubresourceRange subresourceRange)
{
    textureSubresourceStateTrackers(texture).setCurrentState(currentState, subresourceRange);
}

qhwi::CommandQueue::CommandQueue()
    : RefCounterDelete()
{
}

qhwi::CommandQueue::~CommandQueue()
{
    //
    // Release references to all command lists in flight.
    // This relies on the command lists being already completed (not used by the GPU), which
    // is accomplished by calling waitForIdle() in destructors of all CommandQueue sub-classes.
    //
    while (!m_commandListsInFlight.empty())
    {
        CommandListTracker& trackedCommandList(m_commandListsInFlight.front());

        trackedCommandList.commandList->Release();
        trackedCommandList.commandList = nullptr;

        m_commandListsInFlight.pop();
    }
}

void qhwi::CommandQueue::trackCommandLists(uint32_t numCommandLists, CommandList** commandLists, uint64_t submissionID)
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandQueue::trackCommandLists", QHWI_COLOR_QUEUE_SUBMIT);

    QHWI_ASSERT_MSG(numCommandLists, "Cannot track 0 command lists.");
    QHWI_ASSERT_MSG(commandLists, "Invalid command list array.");

    for (uint32_t i = 0; i < numCommandLists; i++)
    {
        commandLists[i]->AddRef();

        m_commandListsInFlight.push({ .commandList = commandLists[i], .submissionId = submissionID });
    }
}

void qhwi::CommandQueue::reclaimResources()
{
    QHWI_PROFILE_SCOPE_NAMED_COLOR("CommandQueue::reclaimResources", QHWI_COLOR_QUEUE_SUBMIT);

    uint64_t lastCompletedID = getLastCompletedID();

    // Iterate starting from the oldest tracked command list
    while (!m_commandListsInFlight.empty())
    {
        QHWI_PROFILE_SCOPE_NAMED_COLOR("releaseCommandList", QHWI_COLOR_QUEUE_SUBMIT);
        CommandListTracker& trackedCommandList(m_commandListsInFlight.front());

        // If completed, release the reference
        if (trackedCommandList.submissionId <= lastCompletedID)
        {
            trackedCommandList.commandList->Release();
            trackedCommandList.commandList = nullptr;

            m_commandListsInFlight.pop();
        }
        else
        {
            // Everything else is only newer, so couldn't have possibly completed yet.
            break;
        }
    }
}

void qhwi::CommandList::markDescSetResourcesAsUsed(qhwi::DescriptorSet* descSet)
{
    size_t numBindings = descSet->desc()->numBindings();
    for (size_t i = 0; i < numBindings; i++)
    {
        DescriptorSetBinding* binding = descSet->desc()->bindings[i];
        if (!binding)
        {
            continue;
        }
        auto bindingType = descSet->desc()->bindingType(uint32_t(i));
        if (isBufferDescriptorType(bindingType))
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& buffer = binding->bufferInfo(j).buffer;
                if (buffer)
                {
                    markResourceAsUsed(binding->bufferInfo(j).buffer);
                }
            }
        }
        else if (isTextureDescriptorType(bindingType) || bindingType == DescriptorType::eInputAttachment)
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& texture = binding->textureInfo(j).texture;
                if (texture)
                {
                    markResourceAsUsed(texture);
                }
            }
        }
        else if (bindingType == DescriptorType::eAccelerationStructure)
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& accelStruct = binding->accelStructInfo(j).accelerationStructure;
                if (accelStruct)
                {
                    markResourceAsUsed(accelStruct);
                }
            }
        }
        else if (bindingType == DescriptorType::eSampler)
        {
            for (uint32_t j = 0; j < binding->count(); j++)
            {
                const auto& sampler = binding->samplerInfo(j).sampler;
                if (sampler)
                {
                    markResourceAsUsed(sampler);
                }
            }
        }
    }
}

void qhwi::TextureSubresourceStates::init(const qhwi::Texture* texture, qhwi::TextureState initialState)
{
    QHWI_ASSERT(texture != nullptr);
    m_texture = texture;
    const auto textureDesc = texture->desc();
    QHWI_ASSERT(textureDesc->arrayLayers >= 1 && textureDesc->mipLevels >= 1);
    m_subresourceStates.resize(textureDesc->arrayLayers * textureDesc->mipLevels, initialState);
    m_isUnifiedState = true;
}

void qhwi::TextureSubresourceStates::setState(qhwi::TextureState state, qhwi::SubresourceRange subresourceRange)
{
    if (subresourceRange.isEntireResource(m_texture))
    {
        // All subresources have the same state.
        // Setting currentState of all subresources
        for (auto& subresourceState : m_subresourceStates)
        {
            subresourceState = state;
        }

        m_isUnifiedState = true;
    }
    else
    {
        // All subresources do not have the same state.
        m_isUnifiedState = false;

        // Check if the subresources in the subresource range have the same state
        uint32_t firstSubresourceIndex = m_texture->calcSubresourceIndex(subresourceRange.start);
        auto& subresourceState = m_subresourceStates[firstSubresourceIndex];
        const auto& desc = m_texture->desc();
        const Subresource& start = subresourceRange.start;
        const Subresource end = {
            .mipSlice = std::min(subresourceRange.end.mipSlice, uint16_t(desc->mipLevels - 1)),
            .arraySlice = std::min(subresourceRange.end.arraySlice, uint16_t(desc->arrayLayers - 1)),
            .planeSlice = std::min(subresourceRange.end.planeSlice, uint16_t(0)),
        };

        for (uint16_t planeSlice = start.planeSlice; planeSlice <= end.planeSlice; ++planeSlice)
        {
            for (uint16_t arraySlice = start.arraySlice; arraySlice <= end.arraySlice; ++arraySlice)
            {
                for (uint16_t mipSlice = start.mipSlice; mipSlice <= end.mipSlice; ++mipSlice)
                {
                    auto arrayIndex = m_texture->calcSubresourceIndex({ .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = planeSlice });
                    QHWI_ASSERT_MSG(m_subresourceStates[arrayIndex] == subresourceState, "Subresources in the range have different states");

                    m_subresourceStates[arrayIndex] = state;
                }
            }
        }
    }
}

const qhwi::TextureState& qhwi::TextureSubresourceStates::getState(qhwi::Subresource subresource) const
{
    if (m_isUnifiedState)
    {
        // All subresources share the same state
        return m_subresourceStates[0];
    }
    else
    {
        // All subresources may not have the same state.
        // Check if the subresources in the subresource range have the same state
        uint32_t subresourceIndex = m_texture->calcSubresourceIndex(subresource);
        const auto& subresourceState = m_subresourceStates[subresourceIndex];

        return subresourceState;
    }
}

void qhwi::CommandList::TextureSubresourceStateTrackers::setCurrentState(qhwi::TextureState state, qhwi::SubresourceRange subresourceRange)
{
    if (subresourceRange.isEntireResource(m_texture))
    {
        // All subresources have the same state.
        // Setting currentState of all subresources
        for (auto& subresourceState : m_subresourceStates)
        {
            subresourceState.currentState = state;
        }

        m_isUnifiedState = true;
    }
    else
    {
        // All subresources do not have the same state.
        m_isUnifiedState = false;

        // Check if the subresources in the subresource range have the same state
        uint32_t firstSubresourceIndex = m_texture->calcSubresourceIndex(subresourceRange.start);
        auto& subresourceState = m_subresourceStates[firstSubresourceIndex];
        const auto& desc = m_texture->desc();
        const Subresource& start = subresourceRange.start;
        const Subresource end = {
            .mipSlice = std::min(subresourceRange.end.mipSlice, uint16_t(desc->mipLevels - 1)),
            .arraySlice = std::min(subresourceRange.end.arraySlice, uint16_t(desc->arrayLayers - 1)),
            .planeSlice = std::min(subresourceRange.end.planeSlice, uint16_t(0)),
        };

        for (uint16_t planeSlice = start.planeSlice; planeSlice <= end.planeSlice; ++planeSlice)
        {
            for (uint16_t arraySlice = start.arraySlice; arraySlice <= end.arraySlice; ++arraySlice)
            {
                for (uint16_t mipSlice = start.mipSlice; mipSlice <= end.mipSlice; ++mipSlice)
                {
                    auto arrayIndex = m_texture->calcSubresourceIndex({ .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = planeSlice });
                    QHWI_ASSERT_MSG(m_subresourceStates[arrayIndex] == subresourceState, "Subresources in the range have different states");

                    m_subresourceStates[arrayIndex].currentState = state;
                }
            }
        }
    }
}

void qhwi::CommandList::TextureSubresourceStateTrackers::setNextState(qhwi::TextureState state, qhwi::SubresourceRange subresourceRange)
{
    if (subresourceRange.isEntireResource(m_texture))
    {
        // All subresources have the same state.
        // Setting currentState of all subresources
        if (m_isUnifiedState)
        {
            for (auto& subresourceState : m_subresourceStates)
            {
                subresourceState.nextState = state;
            }
        }
        else
        {
            QHWI_ASSERT(false);
        }
    }
    else
    {
        // All subresources do not have the same state.
        m_isUnifiedState = false;

        // Check if the subresources in the subresource range have the same state
        uint32_t firstSubresourceIndex = m_texture->calcSubresourceIndex(subresourceRange.start);
        auto& subresourceState = m_subresourceStates[firstSubresourceIndex];

        for (uint16_t planeSlice = subresourceRange.start.planeSlice; planeSlice < std::min(subresourceRange.end.planeSlice + 1, 1); ++planeSlice)
        {
            for (uint16_t arraySlice = subresourceRange.start.arraySlice; arraySlice < std::min(uint32_t(subresourceRange.end.arraySlice + 1), m_texture->desc()->arrayLayers);
                 ++arraySlice)
            {
                for (uint16_t mipSlice = subresourceRange.start.mipSlice; mipSlice < std::min(uint32_t(subresourceRange.end.mipSlice + 1), m_texture->desc()->mipLevels);
                     ++mipSlice)
                {
                    auto arrayIndex = m_texture->calcSubresourceIndex({ .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = planeSlice });
                    QHWI_ASSERT_MSG(m_subresourceStates[arrayIndex] == subresourceState, "Subresources in the range have different states");

                    m_subresourceStates[arrayIndex].nextState = state;
                }
            }
        }
    }
}

qhwi::CommandList::TextureSubresourceStateTrackers::TextureSubresourceStateTrackers(const qhwi::Texture* texture)
{
    QHWI_ASSERT(texture != nullptr);
    m_texture = texture;
    const auto textureDesc = texture->desc();
    QHWI_ASSERT(textureDesc->arrayLayers >= 1 && textureDesc->mipLevels >= 1);
    m_subresourceStates.resize(textureDesc->arrayLayers * textureDesc->mipLevels, TextureStateTracker());
}

const qhwi::CommandList::TextureStateTracker& qhwi::CommandList::TextureSubresourceStateTrackers::getState(qhwi::Subresource subresource) const
{
    if (m_isUnifiedState)
    {
        // All subresources share the same state
        return m_subresourceStates[0];
    }
    else
    {
        // All subresources may not have the same state.
        // Check if the subresources in the subresource range have the same state
        uint32_t subresourceIndex = m_texture->calcSubresourceIndex(subresource);
        auto& subresourceState = m_subresourceStates[subresourceIndex];

        return subresourceState;
    }
}

void qhwi::SubresourceRange::clamp(const qhwi::Texture* texture)
{
    const auto desc = texture->desc();
    end = { std::min(uint16_t(desc->mipLevels - 1), end.mipSlice),
            std::min(uint16_t(desc->arrayLayers - 1), end.arraySlice),
            std::min(uint16_t(desc->numPlanes - 1), end.planeSlice) };
}

bool qhwi::SubresourceRange::isEntireResource(const qhwi::Texture* texture)
{
    if (isEntireResource())
    {
        return true;
    }
    qhwi::SubresourceRange s0 = qhwi::EntireResource;
    s0.clamp(texture);
    return s0 == *this;
}

bool qhwi::CommandList::hasPrevSubpass(std::span<const ColorAttachmentDescription> cbDescs)
{
    for (const auto& cbDesc : cbDescs)
    {
        switch (cbDesc.loadOp)
        {
        case AttachmentLoadOp::ePreserveLocalRender:
        case AttachmentLoadOp::ePreserveLocalSrv:
        case AttachmentLoadOp::ePreserveLocalUav:
            return true;
        default:
            break;
        }
    }
    return false;
}

bool qhwi::CommandList::hasNextSubpass(std::span<const ColorAttachmentDescription> cbDescs)
{
    for (const auto& cbDesc : cbDescs)
    {
        switch (cbDesc.storeOp)
        {
        case AttachmentStoreOp::ePreserveLocalRender:
        case AttachmentStoreOp::ePreserveLocalSrv:
        case AttachmentStoreOp::ePreserveLocalUav:
            return true;
        default:
            break;
        }
    }
    return false;
}

const char* qhwi::getVendorName(uint32_t vendorId)
{
    switch (vendorId)
    {
    case 0x1002:
        return "AMD";
    case 0x1010:
        return "ImgTec";
    case 0x10de:
        return "Nvidia";
    case 0x13b5:
        return "ARM";
    case 0x5143:
        return "Qualcomm";
    case 0x8086:
        return "Intel";
    case 0x106b:
        return "Apple";
    case 0x1414:
        return "Microsoft";
    default:
        return "Unknown";
    }
}

#if defined(QHWI_PLATFORM_WINDOWS)
qhwi::ext::WindowSurface qhwi::ext::toWindowSurface(const HWND hwnd, int width, int height)
{
    QHWI_ASSERT(hwnd);
    if (width == 0 || height == 0)
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    return { .width = width, .height = height, .hwnd = hwnd };
}
#elif defined(QHWI_PLATFORM_ANDROID)
qhwi::ext::WindowSurface qhwi::ext::toWindowSurface(ANativeWindow* window, int width, int height)
{
    QHWI_ASSERT(window);
    return { .width = (width == 0) ? ANativeWindow_getWidth(window) : width, .height = (height == 0) ? ANativeWindow_getHeight(window) : height, .aNativeWindow = window };
}
#elif defined(QHWI_PLATFORM_LINUX)
#    if defined(QHWI_WSI_WAYLAND)
qhwi::ext::WindowSurface qhwi::ext::toWindowSurface(wl_display* wlDisplay, wl_surface* wlSurface, int width, int height)
{
    QHWI_ASSERT(wlDisplay);
    QHWI_ASSERT(wlSurface);
    return { .width = width, .height = height, .wlDisplay = wlDisplay, .wlSurface = wlSurface };
}
#    elif defined(QHWI_WSI_XLIB)
qhwi::ext::WindowSurface qhwi::ext::toWindowSurface(XDisplay* display, XWindow window, int width, int height)
{
    QHWI_ASSERT(display);
    QHWI_ASSERT(window);
    return { .width = width, .height = height, .display = display, .window = window };
}
#    else
qhwi::ext::WindowSurface qhwi::ext::toWindowSurface(xcb_drawable_t drawable, xcb_connection_t* connection, int width, int height)
{
    QHWI_ASSERT(drawable);
    QHWI_ASSERT(connection);
    if (width == 0 || height == 0)
    {
        xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(connection, xcb_get_geometry(connection, drawable), nullptr);
        QHWI_ASSERT(geom != nullptr);

        width = geom->width;
        height = geom->height;
        free(geom);
    }
    return { .width = width, .height = height, .drawable = drawable, .connection = connection };
}
#    endif
#endif

void qhwi::Device::setFeature(Feature feature, bool supported)
{
    const auto desc1 = desc();
    if (feature == Feature_MeshShaders && !desc1->enableMeshShaders)
    {
        supported = false;
    }
    if (feature == Feature_RayQuery && !desc1->enableRayQuery)
    {
        supported = false;
    }
    if (feature == Feature_OpacityMicromaps && !desc1->enableOpacityMicroMaps)
    {
        supported = false;
    }
    if (feature == Feature_CustomResolve && !desc1->enableCustomResolve)
    {
        supported = false;
    }
    setBit(m_featuresInfo.mask, feature, supported);
}
