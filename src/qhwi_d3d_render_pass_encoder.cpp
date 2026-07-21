//
//
//  qhwi_d3d_render_pass_encoder.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_d3d_render_pass_encoder.h>
#include <qhwi/qhwi_d3d12.h>
#include <qre/text/textutil.h>
#include <pix.h>
using namespace qhwi;

namespace qhwi
{
struct D3dBeginMarkerOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dBeginMarkerOp(const char* name)
        : name(name)
    {
    }

    void run(D3dCommandList* cmdList) const override { PIXBeginEvent(cmdList->get(), 0, name); }

    const char* name = nullptr;
};

struct D3dEndMarkerOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    void run(D3dCommandList* cmdList) const override { PIXEndEvent(); }
};

struct D3dBeginRenderPassOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dBeginRenderPassOp(uint32_t numColorAttachments, const ColorAttachmentDescription* colorAttachments, const DepthStencilAttachmentDescription* depthStencilAttachment)
        : numColorAttachments(numColorAttachments)
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
    }

    void run(D3dCommandList* cmdList) const override
    {
        cmdList->addBarriers((uint32_t)numBufferBarriers, bufferBarriers, (uint32_t)numTextureBarriers, textureBarriers);
        cmdList->beginRenderPass1(numColorAttachments, (numColorAttachments > 0) ? colorAttachments : nullptr, hasDepthStencilAttachment ? &depthStencilAttachment : nullptr);
    }

    uint32_t numColorAttachments;
    ColorAttachmentDescription colorAttachments[MaxColorAttachments];
    bool hasDepthStencilAttachment = false;
    DepthStencilAttachmentDescription depthStencilAttachment;
    uint32_t numBufferBarriers = 0;
    const BufferBarrier* bufferBarriers = nullptr;
    uint32_t numTextureBarriers = 0;
    const TextureBarrier* textureBarriers = nullptr;
};

struct D3dEndRenderPassOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    void run(D3dCommandList* cmdList) const override { cmdList->endRenderPass1(); }
};

struct D3dDrawOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dDrawOp(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
        : vertexCount(vertexCount)
        , instanceCount(instanceCount)
        , firstVertex(firstVertex)
        , firstInstance(firstInstance)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance); }

    int vertexCount, instanceCount, firstVertex, firstInstance;
};

struct D3dDispatchMeshOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dDispatchMeshOp(int numThreadGroupsX, int numThreadGroupsY, int numThreadGroupsZ)
        : numThreadGroupsX(numThreadGroupsX)
        , numThreadGroupsY(numThreadGroupsY)
        , numThreadGroupsZ(numThreadGroupsZ)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->DispatchMesh(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ); }

    int numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ;
};

struct D3dDrawIndexedOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dDrawIndexedOp(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
        : indexCount(indexCount)
        , instanceCount(instanceCount)
        , firstIndex(firstIndex)
        , vertexOffset(vertexOffset)
        , firstInstance(firstInstance)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance); }

    int indexCount, instanceCount, firstIndex, vertexOffset, firstInstance;
};

struct D3dExecuteIndirectOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dExecuteIndirectOp(ID3D12CommandSignature* cmdSignature, ID3D12Resource* buffer, size_t offset, uint32_t drawCount)
        : cmdSignature(cmdSignature)
        , buffer(buffer)
        , offset(offset)
        , drawCount(drawCount)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->ExecuteIndirect(cmdSignature, drawCount, buffer, offset, nullptr, 0); }

    ID3D12CommandSignature* cmdSignature;
    ID3D12Resource* buffer;
    size_t offset;
    uint32_t drawCount;
};

struct D3dBindVertexBufferOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dBindVertexBufferOp(uint32_t slot, D3D12_VERTEX_BUFFER_VIEW vb)
        : slot(slot)
        , vb(vb)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->IASetVertexBuffers(slot, 1, &vb); }

    uint32_t slot;
    D3D12_VERTEX_BUFFER_VIEW vb;
};

struct D3dBindIndexBufferOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dBindIndexBufferOp(D3D12_INDEX_BUFFER_VIEW ib)
        : ib(ib)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->IASetIndexBuffer(&ib); }

    D3D12_INDEX_BUFFER_VIEW ib;
};

struct D3dPushConstantsOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dPushConstantsOp(uint32_t rootParamIndex, uint32_t num32BitValues, const void* dataParam, uint32_t offsetIn32BitValues)
        : rootParamIndex(rootParamIndex)
        , num32BitValues(num32BitValues)
        , offsetIn32BitValues(offsetIn32BitValues)
    {
        memcpy(data, dataParam, num32BitValues * 4);
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->SetGraphicsRoot32BitConstants(rootParamIndex, num32BitValues, data, offsetIn32BitValues); }

    uint32_t rootParamIndex;
    uint32_t num32BitValues;
    uint32_t offsetIn32BitValues;
    uint8_t data[MaxPushConstantSize];
};

struct D3dBindDescriptorSetOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dBindDescriptorSetOp(D3dDescriptorSet* descriptorSet,
                           PipelineBindPoint pipelineBindPoint,
                           D3dPipelineLayout* pipelineLayout,
                           uint32_t numDynamicOffsets = 0,
                           const uint32_t* dynamicOffsetsParam = nullptr)
        : descriptorSet(descriptorSet)
        , pipelineBindPoint(pipelineBindPoint)
        , pipelineLayout(pipelineLayout)
        , numDynamicOffsets(numDynamicOffsets)
    {
        memcpy(dynamicOffsets, dynamicOffsetsParam, numDynamicOffsets * sizeof(uint32_t));
    }

    void run(D3dCommandList* cmdList) const override
    {
        cmdList->bindDescriptorSet1(descriptorSet, pipelineBindPoint, pipelineLayout, numDynamicOffsets, numDynamicOffsets ? dynamicOffsets : nullptr);
    }

    D3dDescriptorSet* descriptorSet;
    PipelineBindPoint pipelineBindPoint;
    D3dPipelineLayout* pipelineLayout;
    uint32_t numDynamicOffsets = 0;
    uint32_t dynamicOffsets[MaxDynamicOffsets];
};

struct D3dBindGraphicsPipelineOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dBindGraphicsPipelineOp(D3dGraphicsPipeline* pipeline)
        : pipeline(pipeline)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->bindGraphicsPipeline1(pipeline); }

    D3dGraphicsPipeline* pipeline;
};

struct D3dSetViewportOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dSetViewportOp(D3D12_VIEWPORT vp)
        : vp(vp)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->RSSetViewports(1, &vp); }

    D3D12_VIEWPORT vp;
};

struct D3dSetViewportsOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dSetViewportsOp(uint32_t numViewports, const D3D12_VIEWPORT* viewports)
    {
        vp.resize(numViewports);
        for (uint32_t j = 0; j < numViewports; j++)
        {
            vp[j] = viewports[j];
        }
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->RSSetViewports(UINT(vp.size()), vp.data()); }

    qre::StaticVector<D3D12_VIEWPORT, MaxViewports> vp;
};

struct D3dSetScissorOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dSetScissorOp(D3D12_RECT r)
        : r(r)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->RSSetScissorRects(1, &r); }

    D3D12_RECT r;
};

struct D3dSetScissorsOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dSetScissorsOp(uint32_t numScissors, const D3D12_RECT* scissors)
    {
        r.resize(numScissors);
        for (uint32_t j = 0; j < numScissors; j++)
        {
            r[j] = scissors[j];
        }
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->RSSetScissorRects(UINT(r.size()), r.data()); }

    qre::StaticVector<D3D12_RECT, MaxScissors> r;
};

struct D3dSetStencilReferenceOp final : public D3dDeferredOp, public qre::BumpAllocator::NoAutoDestruct
{
    D3dSetStencilReferenceOp(uint32_t reference)
        : reference(reference)
    {
    }

    void run(D3dCommandList* cmdList) const override { cmdList->get()->OMSetStencilRef(reference); }

    uint32_t reference;
};
}; // namespace qhwi

void D3dRenderPassEncoder::beginMarker(const char* name_)
{
    size_t len = strlen(name_);
    char* name = (char*)m_pool.Alloc(len + 1);
    if (!name)
    {
        QHWI_FATAL("out of memory");
    }
    name[len] = '\0';
    memcpy(name, name_, len);
    add<D3dBeginMarkerOp>(name);
}

void D3dRenderPassEncoder::endMarker()
{
    add<D3dEndMarkerOp>();
}

void D3dRenderPassEncoder::beginRenderPass(uint32_t numColorAttachments,
                                           const ColorAttachmentDescription* colorAttachmentDescs,
                                           const DepthStencilAttachmentDescription* depthStencilAttachmentDesc)
{
    m_beginRenderPassOp = add<D3dBeginRenderPassOp>(numColorAttachments, colorAttachmentDescs, depthStencilAttachmentDesc);
    m_beginRenderPassOp->bufferBarriers = m_bufferBarriersPool.end();
    m_beginRenderPassOp->textureBarriers = m_textureBarriersPool.end();
    m_commandList->m_inRenderPass = true;
}

void D3dRenderPassEncoder::endRenderPass()
{
    QHWI_ASSERT(m_beginRenderPassOp);
    if (supportsAutomaticResourceBarriers())
    {
        //TODO: setState();
    }
    add<D3dEndRenderPassOp>();
    m_beginRenderPassOp = nullptr;
}

void D3dRenderPassEncoder::draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
    m_commandList->setGraphicsState(false, false);
    add<D3dDrawOp>(vertexCount, instanceCount, firstVertex, firstInstance);
}

void D3dRenderPassEncoder::dispatchMesh(int numThreadGroupsX, int numThreadGroupsY, int numThreadGroupsZ)
{
    m_commandList->setGraphicsState();
    add<D3dDispatchMeshOp>(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
}

void D3dRenderPassEncoder::drawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
    m_commandList->setGraphicsState();
    add<D3dDrawIndexedOp>(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void D3dRenderPassEncoder::drawIndirect(ID3D12CommandSignature* cmdSignature, D3dBuffer* buffer, size_t offset, uint32_t drawCount)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setIndirectBuffer(buffer);
    }
    m_commandList->setGraphicsState(false);
    add<D3dExecuteIndirectOp>(cmdSignature, buffer->get(), offset, drawCount);
}

void D3dRenderPassEncoder::drawIndexedIndirect(ID3D12CommandSignature* cmdSignature, D3dBuffer* buffer, size_t offset, uint32_t drawCount)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setIndirectBuffer(buffer);
    }
    m_commandList->setGraphicsState();
    add<D3dExecuteIndirectOp>(cmdSignature, buffer->get(), offset, drawCount);
}

void D3dRenderPassEncoder::bindVertexBuffer(D3dBuffer* buffer, uint32_t slot, D3D12_VERTEX_BUFFER_VIEW vb)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setVertexBuffer(slot, buffer);
    }
    add<D3dBindVertexBufferOp>(slot, vb);
}

void D3dRenderPassEncoder::bindIndexBuffer(D3dBuffer* buffer, D3D12_INDEX_BUFFER_VIEW ib)
{
    if (buffer->automaticBarriersEnabled())
    {
        nextGraphicsState().setIndexBuffer(buffer);
    }
    add<D3dBindIndexBufferOp>(ib);
}

void D3dRenderPassEncoder::pushConstants(uint32_t rootParamIndex, uint32_t num32BitValues, const void* data, uint32_t offsetIn32BitValues)
{
    add<D3dPushConstantsOp>(rootParamIndex, num32BitValues, data, offsetIn32BitValues);
}

bool D3dRenderPassEncoder::supportsAutomaticResourceBarriers()
{
    return m_commandList->desc()->supportsAutomaticBarriers;
}

CommandList::GraphicsResourceState& D3dRenderPassEncoder::nextGraphicsState()
{
    return m_commandList->m_nextGraphicsState;
}

void D3dRenderPassEncoder::bindDescriptorSet(D3dDescriptorSet* descriptorSet,
                                             PipelineBindPoint pipelineBindPoint,
                                             D3dPipelineLayout* pipelineLayout,
                                             uint32_t numDynamicOffsets,
                                             const uint32_t* dynamicOffsets)
{
    uint32_t slot = descriptorSet->layoutDesc()->registerSpace;
    if (supportsAutomaticResourceBarriers())
    {
        nextGraphicsState().setDescSet(slot, descriptorSet);
    }
    add<D3dBindDescriptorSetOp>(descriptorSet, pipelineBindPoint, pipelineLayout, numDynamicOffsets, dynamicOffsets);
}

void D3dRenderPassEncoder::bindGraphicsPipeline(D3dGraphicsPipeline* pipeline)
{
    add<D3dBindGraphicsPipelineOp>(pipeline);
}

void D3dRenderPassEncoder::setViewports(uint32_t numViewports, const D3D12_VIEWPORT* vp)
{
    if (numViewports == 1)
    {
        add<D3dSetViewportOp>(*vp);
    }
    else
    {
        add<D3dSetViewportsOp>(numViewports, vp);
    }
}

void D3dRenderPassEncoder::setScissors(uint32_t numScissors, const D3D12_RECT* r)
{
    if (numScissors == 1)
    {
        add<D3dSetScissorOp>(*r);
    }
    else
    {
        add<D3dSetScissorsOp>(numScissors, r);
    }
}

void D3dRenderPassEncoder::setStencilReference(uint32_t reference)
{
    add<D3dSetStencilReferenceOp>(reference);
}

void D3dRenderPassEncoder::playback()
{
    auto op = static_cast<const D3dDeferredOp*>(m_pool.Base());
    while (op)
    {
        op->run(m_commandList);
        op = op->pNext;
    }
}

void qhwi::D3dRenderPassEncoder::setState(Buffer* buffer, BufferState nextState)
{
    QHWI_ASSERT(m_beginRenderPassOp);
    auto& s = m_commandList->bufferState(buffer);
    nextState = s.currentState | nextState;
    if (s.currentState == nextState)
    {
        return;
    }
    auto s0 = s.currentState;
    const auto& s1 = nextState;
    BufferBarrier b = { buffer, s0, s1 };
    m_bufferBarriersPool.push_back(b);
    m_beginRenderPassOp->numBufferBarriers++;
    s.currentState = s1;
}

void qhwi::D3dRenderPassEncoder::setState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange)
{
    QHWI_ASSERT(m_beginRenderPassOp);

    auto ts1 = m_commandList->textureSubresourceStateTrackers(texture);

    if (ts1.isUnifiedState())
    {
        const auto& s = ts1.getState();
        auto updatedNextState = s.currentState | nextState;
        if (s.currentState == updatedNextState)
        {
            return;
        }
        auto s0 = s.currentState;
        const auto& s1 = updatedNextState;
        TextureBarrier b = { texture, s0, s1 };
        m_textureBarriersPool.push_back(b);
        m_beginRenderPassOp->numTextureBarriers++;
        m_commandList->setCurrentState(texture, updatedNextState, subresourceRange);
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
            for (uint16_t arraySlice = start.arraySlice; arraySlice < end.arraySlice; ++arraySlice)
            {
                for (uint16_t mipSlice = start.mipSlice; mipSlice < end.mipSlice; ++mipSlice)
                {
                    Subresource subresource = { .mipSlice = mipSlice, .arraySlice = arraySlice, .planeSlice = planeSlice };

                    const auto& s = ts1.getState(subresource);
                    auto updatedNextState = s.currentState | nextState;
                    if (s.currentState == updatedNextState)
                    {
                        continue;
                    }

                    auto s0 = s.currentState;
                    const auto& s1 = updatedNextState;
                    TextureBarrier b = { texture, s0, s1 };
                    m_textureBarriersPool.push_back(b);
                    m_beginRenderPassOp->numTextureBarriers++;
                    m_commandList->setCurrentState(texture, updatedNextState, subresource);
                }
            }
        }
    }
}

PipelineStageMask D3dRenderPassEncoder::toPipelineStageMask(ShaderStageMask m0)
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

AccessModeMask D3dRenderPassEncoder::toAccessModeMask(DescriptorType type)
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

ImageLayout D3dRenderPassEncoder::toImageLayout(DescriptorType type)
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
