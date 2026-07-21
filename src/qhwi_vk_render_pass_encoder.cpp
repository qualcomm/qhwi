//
//
//  qhwi_vk_render_pass_encoder.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_vk_render_pass_encoder.h>
#include <qhwi/qhwi_vk.h>
#include <qre/text/textutil.h>

namespace qhwi
{
struct VkBeginMarkerOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkBeginMarkerOp(const char* name)
        : name(name)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT(name)); }

    const char* name = nullptr;
};

struct VkEndMarkerOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    void run(VkCommandList* cmdList) const override { cmdList->get().endDebugUtilsLabelEXT(); }
};

struct VkBeginRenderPassOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkBeginRenderPassOp(uint32_t numColorAttachments,
                        const ColorAttachmentDescription* colorAttachments,
                        const DepthStencilAttachmentDescription* depthStencilAttachment,
                        const MultiViewState* multiViewState,
                        const Rect2D* multiViewPerViewRenderAreasQCOM,
                        const uint32_t* colorAttachmentRemapping,
                        bool customResolveParam)
        : numColorAttachments(numColorAttachments)
        , customResolve(customResolveParam)
    {
        for (uint32_t j = 0; j < numColorAttachments; j++)
        {
            this->colorAttachments[j] = colorAttachments[j];
        }
        if (depthStencilAttachment)
        {
            hasDepthStencilAttachment = true;
            this->depthStencilAttachment = *depthStencilAttachment;
        }
        if (multiViewState)
        {
            hasMultiViewState = true;
            this->multiViewState = *multiViewState;
        }
        if (multiViewPerViewRenderAreasQCOM)
        {
            hasMultiViewPerViewRenderAreas = true;
            for (uint32_t j = 0; j < multiViewState->numViews; j++)
            {
                this->multiViewPerViewRenderAreas[j] = multiViewPerViewRenderAreasQCOM[j];
            }
        }
        if (colorAttachmentRemapping)
        {
            hasColorAttachmentRemapping = true;
            for (uint32_t j = 0; j < numColorAttachments; j++)
            {
                this->colorAttachmentRemapping[j] = colorAttachmentRemapping[j];
            }
        }
    }

    void run(VkCommandList* cmdList) const override
    {
        cmdList->addBarriers((uint32_t)numBufferBarriers, bufferBarriers, (uint32_t)numTextureBarriers, textureBarriers);
        cmdList->beginRenderPass1(numColorAttachments,
                                  (numColorAttachments > 0) ? colorAttachments : nullptr,
                                  hasDepthStencilAttachment ? &depthStencilAttachment : nullptr,
                                  hasMultiViewState ? &multiViewState : nullptr,
                                  hasMultiViewPerViewRenderAreas ? multiViewPerViewRenderAreas : nullptr,
                                  hasColorAttachmentRemapping ? colorAttachmentRemapping : nullptr,
                                  nullptr,
                                  customResolve);
    }

    uint32_t numColorAttachments;
    ColorAttachmentDescription colorAttachments[MaxColorAttachments];
    bool hasDepthStencilAttachment = false;
    DepthStencilAttachmentDescription depthStencilAttachment;
    uint32_t numBufferBarriers = 0;
    const BufferBarrier* bufferBarriers = nullptr;
    uint32_t numTextureBarriers = 0;
    const TextureBarrier* textureBarriers = nullptr;
    bool hasMultiViewState = false;
    MultiViewState multiViewState;
    bool hasMultiViewPerViewRenderAreas = false;
    Rect2D multiViewPerViewRenderAreas[MaxViews];
    bool hasColorAttachmentRemapping = false;
    uint32_t colorAttachmentRemapping[MaxColorAttachments] = { 0 };
    bool customResolve = false;
};

struct VkEndRenderPassOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    void run(VkCommandList* cmdList) const override { cmdList->endRenderPass1(); }
};

struct VkBeginCustomResolveOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    void run(VkCommandList* cmdList) const override { cmdList->beginCustomResolve1(); }
};

struct VkDrawOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkDrawOp(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
        : vertexCount(vertexCount)
        , instanceCount(instanceCount)
        , firstVertex(firstVertex)
        , firstInstance(firstInstance)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().draw(vertexCount, instanceCount, firstVertex, firstInstance); }

    int vertexCount, instanceCount, firstVertex, firstInstance;
};

struct VkDrawMeshTasksOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkDrawMeshTasksOp(int numThreadGroupsX, int numThreadGroupsY, int numThreadGroupsZ)
        : numThreadGroupsX(numThreadGroupsX)
        , numThreadGroupsY(numThreadGroupsY)
        , numThreadGroupsZ(numThreadGroupsZ)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().drawMeshTasksEXT(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ); }

    int numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ;
};

struct VkDrawIndexedOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkDrawIndexedOp(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
        : indexCount(indexCount)
        , instanceCount(instanceCount)
        , firstIndex(firstIndex)
        , vertexOffset(vertexOffset)
        , firstInstance(firstInstance)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }

    int indexCount, instanceCount, firstIndex, vertexOffset, firstInstance;
};

struct VkDrawIndirectOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkDrawIndirectOp(vk::Buffer buffer, size_t offset, uint32_t drawCount)
        : buffer(buffer)
        , offset(offset)
        , drawCount(drawCount)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().drawIndirect(buffer, offset, drawCount, sizeof(VkDrawIndirectCommand)); }

    vk::Buffer buffer;
    size_t offset;
    uint32_t drawCount;
};

struct VkDrawIndexedIndirectOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkDrawIndexedIndirectOp(vk::Buffer buffer, size_t offset, uint32_t drawCount)
        : buffer(buffer)
        , offset(offset)
        , drawCount(drawCount)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().drawIndexedIndirect(buffer, offset, drawCount, sizeof(VkDrawIndexedIndirectCommand)); }

    vk::Buffer buffer;
    size_t offset;
    uint32_t drawCount;
};

struct VkBindVertexBufferOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkBindVertexBufferOp(vk::Buffer buffer, uint32_t slot, size_t offset, size_t stride)
        : buffer(buffer)
        , slot(slot)
        , offset(offset)
        , stride(stride)
    {
    }

    void run(VkCommandList* cmdList) const override
    {
        VkDeviceSize offsets[] = { VkDeviceSize(offset) };
        cmdList->get().bindVertexBuffers(slot, 1, &buffer, offsets);
    }

    vk::Buffer buffer;
    uint32_t slot;
    size_t offset, stride;
};

struct VkBindIndexBufferOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkBindIndexBufferOp(vk::Buffer buffer, vk::IndexType indexType, size_t offset)
        : buffer(buffer)
        , indexType(indexType)
        , offset(offset)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().bindIndexBuffer(buffer, offset, indexType); }

    vk::Buffer buffer;
    vk::IndexType indexType;
    size_t offset;
};

struct VkPushConstantsOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkPushConstantsOp(vk::PipelineLayout pipelineLayout, vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* dataParam)
        : pipelineLayout(pipelineLayout)
        , stageFlags(stageFlags)
        , offset(offset)
        , size(size)
    {
        memcpy(data, dataParam, size);
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().pushConstants(pipelineLayout, stageFlags, offset, size, data); }

    vk::PipelineLayout pipelineLayout;
    vk::ShaderStageFlags stageFlags;
    uint32_t offset;
    uint32_t size;
    uint8_t data[MaxPushConstantSize];
};

struct VkBindDescriptorSetOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkBindDescriptorSetOp(vk::PipelineBindPoint pipelineBindPoint,
                          vk::PipelineLayout pipelineLayout,
                          uint32_t set,
                          vk::DescriptorSet descriptorSet,
                          uint32_t numDynamicOffsets = 0,
                          const uint32_t* dynamicOffsetsParam = nullptr)
        : pipelineBindPoint(pipelineBindPoint)
        , pipelineLayout(pipelineLayout)
        , set(set)
        , descriptorSet(descriptorSet)
        , numDynamicOffsets(numDynamicOffsets)
    {
        memcpy(dynamicOffsets, dynamicOffsetsParam, numDynamicOffsets * sizeof(uint32_t));
    }

    void run(VkCommandList* cmdList) const override
    {
        cmdList->get().bindDescriptorSets(pipelineBindPoint, pipelineLayout, set, 1, &descriptorSet, numDynamicOffsets, numDynamicOffsets ? dynamicOffsets : nullptr);
    }

    vk::PipelineBindPoint pipelineBindPoint;
    vk::PipelineLayout pipelineLayout;
    uint32_t set;
    vk::DescriptorSet descriptorSet;
    uint32_t numDynamicOffsets = 0;
    uint32_t dynamicOffsets[MaxDynamicOffsets];
};

struct VkBindGraphicsPipelineOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkBindGraphicsPipelineOp(vk::Pipeline pipeline)
        : pipeline(pipeline)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline); }

    vk::Pipeline pipeline;
};

struct VkSetViewportOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkSetViewportOp(vk::Viewport vp)
        : vp(vp)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().setViewport(0, 1, &vp); }

    vk::Viewport vp;
};

struct VkSetViewportsOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkSetViewportsOp(uint32_t numViewports, const vk::Viewport* viewports)
    {
        vp.resize(numViewports);
        for (uint32_t j = 0; j < numViewports; j++)
        {
            vp[j] = viewports[j];
        }
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().setViewport(0, (uint32_t)vp.size(), vp.data()); }

    qre::StaticVector<vk::Viewport, MaxViewports> vp;
};

struct VkSetScissorOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkSetScissorOp(vk::Rect2D r)
        : r(r)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().setScissor(0, 1, &r); }

    vk::Rect2D r;
};

struct VkSetScissorsOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkSetScissorsOp(uint32_t numScissors, const vk::Rect2D* scissors)
    {
        r.resize(numScissors);
        for (uint32_t j = 0; j < numScissors; j++)
        {
            r[j] = scissors[j];
        }
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().setScissor(0, (uint32_t)r.size(), r.data()); }

    qre::StaticVector<vk::Rect2D, MaxScissors> r;
};

struct VkSetStencilReferenceOp final : public VkDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    VkSetStencilReferenceOp(uint32_t reference)
        : reference(reference)
    {
    }

    void run(VkCommandList* cmdList) const override { cmdList->get().setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, reference); }

    uint32_t reference;
};

void VkRenderPassEncoder::beginMarker(const char* name_)
{
    size_t len = strlen(name_);
    char* name = (char*)m_pool.Alloc(len + 1);
    if (!name)
    {
        QHWI_FATAL("out of memory");
    }
    name[len] = '\0';
    memcpy(name, name_, len);
    add<VkBeginMarkerOp>(name);
}

void VkRenderPassEncoder::endMarker()
{
    add<VkEndMarkerOp>();
}

void VkRenderPassEncoder::beginRenderPass(uint32_t numColorAttachments,
                                          const ColorAttachmentDescription* colorAttachmentDescs,
                                          const DepthStencilAttachmentDescription* depthStencilAttachmentDesc,
                                          const MultiViewState* multiViewState,
                                          const Rect2D* multiViewPerViewRenderAreasQCOM,
                                          const uint32_t* colorAttachmentRemapping,
                                          bool customResolveParam)
{
    m_beginRenderPassOp = add<VkBeginRenderPassOp>(numColorAttachments,
                                                   colorAttachmentDescs,
                                                   depthStencilAttachmentDesc,
                                                   multiViewState,
                                                   multiViewPerViewRenderAreasQCOM,
                                                   colorAttachmentRemapping,
                                                   customResolveParam);
    m_beginRenderPassOp->bufferBarriers = m_bufferBarriersPool.end();
    m_beginRenderPassOp->textureBarriers = m_textureBarriersPool.end();
    m_commandList->m_inRenderPass = true;
}

void VkRenderPassEncoder::endRenderPass()
{
    QHWI_ASSERT(m_beginRenderPassOp);
    if (supportsAutomaticResourceBarriers())
    {
        setState();
    }
    add<VkEndRenderPassOp>();
    m_beginRenderPassOp = nullptr;
}

void VkRenderPassEncoder::beginCustomResolve()
{
    add<VkBeginCustomResolveOp>();
}

void VkRenderPassEncoder::draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
    m_commandList->setGraphicsState(false, false);
    add<VkDrawOp>(vertexCount, instanceCount, firstVertex, firstInstance);
}

void VkRenderPassEncoder::drawMeshTasks(int numThreadGroupsX, int numThreadGroupsY, int numThreadGroupsZ)
{
    m_commandList->setGraphicsState();
    add<VkDrawMeshTasksOp>(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
}

void VkRenderPassEncoder::drawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
    m_commandList->setGraphicsState();
    add<VkDrawIndexedOp>(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VkRenderPassEncoder::drawIndirect(VkBuffer* buffer, size_t offset, uint32_t drawCount)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setIndirectBuffer(buffer);
    }
    m_commandList->setGraphicsState(false);
    add<VkDrawIndirectOp>(buffer->get(), offset, drawCount);
}

void VkRenderPassEncoder::drawIndexedIndirect(VkBuffer* buffer, size_t offset, uint32_t drawCount)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setIndirectBuffer(buffer);
    }
    m_commandList->setGraphicsState();
    add<VkDrawIndexedIndirectOp>(buffer->get(), offset, drawCount);
}

void VkRenderPassEncoder::bindVertexBuffer(VkBuffer* buffer, uint32_t slot, size_t offset, size_t stride)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setVertexBuffer(slot, buffer);
    }
    add<VkBindVertexBufferOp>(buffer->get(), slot, offset, stride);
}

void VkRenderPassEncoder::bindIndexBuffer(VkBuffer* buffer, vk::IndexType indexType, size_t offset)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setIndexBuffer(buffer);
    }
    add<VkBindIndexBufferOp>(buffer->get(), indexType, offset);
}

void VkRenderPassEncoder::pushConstants(vk::PipelineLayout pipelineLayout, vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* data)
{
    add<VkPushConstantsOp>(pipelineLayout, stageFlags, offset, size, data);
}

void VkRenderPassEncoder::bindDescriptorSet(vk::PipelineBindPoint pipelineBindPoint,
                                            vk::PipelineLayout pipelineLayout,
                                            VkDescriptorSet* descriptorSet,
                                            uint32_t numDynamicOffsets,
                                            const uint32_t* dynamicOffsets)
{
    uint32_t set = descriptorSet->layoutDesc()->registerSpace;
    if (supportsAutomaticResourceBarriers())
    {
        nextGraphicsState().setDescSet(set, descriptorSet);
    }
    add<VkBindDescriptorSetOp>(pipelineBindPoint, pipelineLayout, set, descriptorSet->get(), numDynamicOffsets, dynamicOffsets);
}

bool VkRenderPassEncoder::supportsAutomaticResourceBarriers()
{
    return m_commandList->desc()->supportsAutomaticBarriers;
}

CommandList::GraphicsResourceState& VkRenderPassEncoder::nextGraphicsState()
{
    return m_commandList->m_nextGraphicsState;
}

void VkRenderPassEncoder::bindGraphicsPipeline(vk::Pipeline pipeline)
{
    add<VkBindGraphicsPipelineOp>(pipeline);
}

void VkRenderPassEncoder::setViewports(uint32_t numViewports, const vk::Viewport* vp)
{
    if (numViewports == 1)
    {
        add<VkSetViewportOp>(*vp);
    }
    else
    {
        add<VkSetViewportsOp>(numViewports, vp);
    }
}

void VkRenderPassEncoder::setScissors(uint32_t numScissors, const vk::Rect2D* r)
{
    if (numScissors == 1)
    {
        add<VkSetScissorOp>(*r);
    }
    else
    {
        add<VkSetScissorsOp>(numScissors, r);
    }
}

void VkRenderPassEncoder::setStencilReference(uint32_t reference)
{
    add<VkSetStencilReferenceOp>(reference);
}

void VkRenderPassEncoder::playback()
{
    auto op = static_cast<const VkDeferredOp*>(m_pool.Base());
    while (op)
    {
        op->run(m_commandList);
        op = op->pNext;
    }
}

void qhwi::VkRenderPassEncoder::setState(Buffer* buffer, BufferState nextState)
{
    QHWI_ASSERT(m_beginRenderPassOp);
    auto it = m_bufferNextState.find(buffer);
    if (it == m_bufferNextState.end())
    {
        m_bufferNextState[buffer] = nextState;
        return;
    }
    m_bufferNextState[buffer] |= nextState;
}

void qhwi::VkRenderPassEncoder::setState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange)
{
    QHWI_ASSERT(m_beginRenderPassOp);
    auto it = m_textureNextState.find(texture);
    if (it == m_textureNextState.end())
    {
        it = m_textureNextState.insert({ texture, TextureSubresourceStates(texture, nextState) }).first;
        return;
    }

    if (it->second.isUnifiedState())
    {
        auto& state = it->second.getState();
        it->second.setState(state | nextState, subresourceRange);
    }
    else
    {
        const auto& desc = texture->desc();
        const Subresource& start = subresourceRange.start;
        const Subresource end = { .mipSlice = std::min(subresourceRange.end.mipSlice, uint16_t(desc->mipLevels - 1)),
                                  .arraySlice = std::min(subresourceRange.end.arraySlice, uint16_t(desc->arrayLayers - 1)),
                                  .planeSlice = std::min(subresourceRange.end.planeSlice, uint16_t(0)) };

        for (uint16_t planeSlice = start.planeSlice; planeSlice <= end.planeSlice; ++planeSlice)
        {
            for (uint16_t mipSlice = start.mipSlice; mipSlice <= end.mipSlice; ++mipSlice)
            {
                for (uint16_t arraySlice = start.arraySlice; arraySlice <= end.arraySlice; ++arraySlice)
                {
                    Subresource subresouce = { .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = planeSlice };

                    auto& state = it->second.getState();
                    it->second.setState(state | nextState, subresouce);
                }
            }
        }
    }
}

void qhwi::VkRenderPassEncoder::setState()
{
    for (auto it : m_bufferNextState)
    {
        Buffer* buffer = it.first;
        auto& s = m_commandList->bufferState(buffer);
        auto s0 = s.currentState;
        auto s1 = it.second;
        BufferBarrier b = { buffer, s0, s1 };
        m_bufferBarriersPool.push_back(b);
        m_beginRenderPassOp->numBufferBarriers++;
        s.currentState = s1;
    }
    for (auto it : m_textureNextState)
    {
        Texture* texture = it.first;
        auto& ts1 = m_commandList->textureSubresourceStateTrackers(texture);
        const auto& s = ts1.getState();
        auto s0 = s.currentState;
        auto s1 = it.second.getState();
        TextureBarrier b = { texture, s0, s1 };
        m_textureBarriersPool.push_back(b);
        m_beginRenderPassOp->numTextureBarriers++;
        ts1.setCurrentState(s1);
    }
    m_bufferNextState.clear();
    m_textureNextState.clear();
}

PipelineStageMask VkRenderPassEncoder::toPipelineStageMask(ShaderStageMask m0)
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
    return m1;
}

AccessModeMask VkRenderPassEncoder::toAccessModeMask(DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::eConstantBuffer:
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
    default:
        return 0;
    }
}

ImageLayout VkRenderPassEncoder::toImageLayout(DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::eRWTexture:
        return ImageLayout::eUnorderedAccess;
    case DescriptorType::eTexture:
        return ImageLayout::eShaderReadOnlyOptimal;
    default:
        return ImageLayout::eUndefined;
    }
}
} // namespace qhwi
