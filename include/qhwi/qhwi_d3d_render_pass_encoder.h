//
//
//  qhwi_d3d_render_pass_encoder.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_D3D_RENDER_PASS_ENCODER_H__
#define __QHWI_D3D_RENDER_PASS_ENCODER_H__
#include <qhwi/qhwi.h>
#include <d3d12.h>
#include <qre/memory/BumpAllocator.h>

namespace qhwi
{
class D3dCommandList;
class D3dBuffer;
class D3dGraphicsPipeline;
class D3dDescriptorSet;
class D3dPipelineLayout;

struct D3dDeferredOp
{
    virtual ~D3dDeferredOp() {}

    virtual void run(D3dCommandList* cmdList) const = 0;
    D3dDeferredOp* pNext = nullptr;
};
struct D3dBeginRenderPassOp;

class D3dRenderPassEncoder
{
public:
    static constexpr size_t MemPoolSize = 4096 * 1024;
    static constexpr size_t MaxBufferBarriers = 1024 * 1024;
    static constexpr size_t MaxTextureBarriers = 1024 * 1024;

    D3dRenderPassEncoder() { m_pool.Init(m_poolData, MemPoolSize); }

    // Event annotation
    void beginMarker(const char* name);
    void endMarker();

    void beginRenderPass(uint32_t numColorAttachments,
                         const ColorAttachmentDescription* colorAttachments,
                         const DepthStencilAttachmentDescription* depthStencilAttachment = nullptr);

    void endRenderPass();

    void draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0);

    void dispatchMesh(int numThreadGroupsX, int numThreadGroupsY, int numThreadGroupsZ);

    void drawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0);

    void drawIndirect(ID3D12CommandSignature* cmdSignature, D3dBuffer* buffer, size_t offset = 0, uint32_t drawCount = 1);

    void drawIndexedIndirect(ID3D12CommandSignature* cmdSignature, D3dBuffer* buffer, size_t offset = 0, uint32_t drawCount = 1);

    void bindVertexBuffer(D3dBuffer* buffer, uint32_t slot, D3D12_VERTEX_BUFFER_VIEW vb);

    void bindIndexBuffer(D3dBuffer* buffer, D3D12_INDEX_BUFFER_VIEW ib);

    void pushConstants(uint32_t rootParamIndex, uint32_t num32BitValues, const void* data, uint32_t offsetIn32BitValues);

    void bindDescriptorSet(D3dDescriptorSet* descriptorSet,
                           PipelineBindPoint pipelineBindPoint,
                           D3dPipelineLayout* pipelineLayout,
                           uint32_t numDynamicOffsets = 0,
                           const uint32_t* dynamicOffsets = nullptr);

    void bindGraphicsPipeline(D3dGraphicsPipeline* pipeline);

    void setViewports(uint32_t numViewports, const D3D12_VIEWPORT* viewports);

    void setScissors(uint32_t numScissors, const D3D12_RECT* scissorRects);

    void setStencilReference(uint32_t reference);

    void beginRecording(D3dCommandList* cmdList) { m_commandList = cmdList; }

    void playback();

    void reset()
    {
        m_pool.Clear();
        m_bufferBarriersPool.clear();
        m_textureBarriersPool.clear();
        m_beginRenderPassOp = nullptr;
        m_currentOp = nullptr;
    }

    void setState(Buffer* buffer, BufferState nextState);
    void setState(Texture* texture, TextureState nextState, SubresourceRange subresourceRange = EntireResource);

protected:
    bool supportsAutomaticResourceBarriers();
    CommandList::GraphicsResourceState& nextGraphicsState();
    PipelineStageMask toPipelineStageMask(ShaderStageMask m0);
    AccessModeMask toAccessModeMask(DescriptorType type);
    ImageLayout toImageLayout(DescriptorType type);

    template <typename T, typename... Args>
    T* add(Args&&... args)
    {
        T* op = m_pool.New<T>(std::forward<Args>(args)...);
        QHWI_ASSERT_MSG(op, "out of memory");
        if (m_currentOp)
        {
            m_currentOp->pNext = op;
        }
        m_currentOp = op;
        return op;
    }

    qre::BumpAllocator m_pool;
    uint8_t m_poolData[MemPoolSize];
    qre::StaticVector<qhwi::BufferBarrier, MaxBufferBarriers> m_bufferBarriersPool;
    qre::StaticVector<qhwi::TextureBarrier, MaxTextureBarriers> m_textureBarriersPool;
    D3dBeginRenderPassOp* m_beginRenderPassOp = nullptr;
    D3dDeferredOp* m_currentOp = nullptr;
    D3dCommandList* m_commandList = nullptr;
};
} // namespace qhwi
#endif
