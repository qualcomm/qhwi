//
//
//  qhwi_vk_render_pass_encoder.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_VK_RENDER_PASS_ENCODER_H__
#define __QHWI_VK_RENDER_PASS_ENCODER_H__
#include <qhwi/qhwi.h>
#include <qhwi/qhwi_vk_config.h>
#include <qre/memory/BumpAllocator.h>
#include <map>

namespace qhwi
{
class VkCommandList;
class VkBuffer;
class VkDescriptorSet;

struct VkDeferredOp
{
    virtual ~VkDeferredOp() {}

    virtual void run(VkCommandList* cmdList) const = 0;
    VkDeferredOp* pNext = nullptr;
};
struct VkBeginRenderPassOp;

class VkRenderPassEncoder
{
public:
    static constexpr size_t MemPoolSize = 4096 * 1024;
    static constexpr size_t MaxBufferBarriers = 1024 * 1024;
    static constexpr size_t MaxTextureBarriers = 1024 * 1024;

    VkRenderPassEncoder() { m_pool.Init(m_poolData, MemPoolSize); }

    // Event annotation
    void beginMarker(const char* name);
    void endMarker();

    void beginRenderPass(uint32_t numColorAttachments,
                         const ColorAttachmentDescription* colorAttachments,
                         const DepthStencilAttachmentDescription* depthStencilAttachment = nullptr,
                         const MultiViewState* multiViewState = nullptr,
                         const Rect2D* multiViewPerViewRenderAreasQCOM = nullptr,
                         const uint32_t* colorAttachmentRemapping = nullptr,
                         bool customResolveParam = false);

    void endRenderPass();

    void beginCustomResolve();

    void draw(int vertexCount, int instanceCount = 1, int firstVertex = 0, int firstInstance = 0);

    void drawMeshTasks(int numThreadGroupsX, int numThreadGroupsY, int numThreadGroupsZ);

    void drawIndexed(int indexCount, int instanceCount = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0);

    void drawIndirect(VkBuffer* buffer, size_t offset = 0, uint32_t drawCount = 1);

    void drawIndexedIndirect(VkBuffer* buffer, size_t offset = 0, uint32_t drawCount = 1);

    void bindVertexBuffer(VkBuffer* buffer, uint32_t slot, size_t offset = 0, size_t stride = 0);

    void bindIndexBuffer(VkBuffer* buffer, vk::IndexType indexType = vk::IndexType::eUint32, size_t offset = 0);

    void pushConstants(vk::PipelineLayout pipelineLayout, vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* data);

    void bindDescriptorSet(vk::PipelineBindPoint pipelineBindPoint,
                           vk::PipelineLayout pipelineLayout,
                           VkDescriptorSet* descriptorSet,
                           uint32_t numDynamicOffsets = 0,
                           const uint32_t* dynamicOffsets = nullptr);

    void bindGraphicsPipeline(vk::Pipeline pipeline);

    void setViewports(uint32_t numViewports, const vk::Viewport* viewports);

    void setScissors(uint32_t numScissors, const vk::Rect2D* scissorRects);

    void setStencilReference(uint32_t reference);

    void beginRecording(VkCommandList* cmdList) { m_commandList = cmdList; }

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

    void setState();
    std::unordered_map<Buffer*, BufferState> m_bufferNextState;
    std::unordered_map<Texture*, TextureSubresourceStates> m_textureNextState;

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
    VkBeginRenderPassOp* m_beginRenderPassOp = nullptr;
    VkDeferredOp* m_currentOp = nullptr;
    VkCommandList* m_commandList = nullptr;
};
} // namespace qhwi
#endif
