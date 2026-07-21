//
//
//  test_buffer.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestVertexBuffer : public UnitTest
{
public:
    TestVertexBuffer(GraphicsBackend graphicsBackend, Config config, const char* name = "test_vertex_buffer")
        : UnitTest(graphicsBackend, config, name)
    {
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        const int NumVerts = 64;
        const float4 color = { 0.5f, 0.0f, 0.5f, 1.0f };
        float2 pos[NumVerts];
        float2* posPtr = pos;
        int v0 = (NumVerts / 4);
        for (int j = 0; j < v0; j++)
        {
            float t = 1.0f * float(j) / float(v0 - 1);
            *posPtr++ = { -0.5f + 1.0f * t, -.5f };
        }
        for (int j = 0; j < v0; j++)
        {
            float t = 1.0f * float(j) / float(v0 - 1);
            *posPtr++ = { 0.5f, -0.5f + 1.0f * t };
        }
        for (int j = 0; j < v0; j++)
        {
            float t = 1.0f * float(j) / float(v0 - 1);
            *posPtr++ = { 0.5f - 1.0f * t, 0.5f };
        }
        for (int j = 0; j < v0; j++)
        {
            float t = 1.0f * float(j) / float(v0 - 1);
            *posPtr++ = { -0.5f, 0.5f - 1.0f * t };
        }

        m_drawColorTask.init({ .device = m_device.Get(),
                               .color = color,
                               .posData = pos,
                               .numVerts = NumVerts,
                               .primitiveTopology = PrimitiveTopology::eLineStrip,
                               .colorAttachmentFormat = m_colorAttachmentFormat,
                               .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DrawColorTask m_drawColorTask;
};

class TestIndexBuffer : public UnitTest
{
public:
    TestIndexBuffer(GraphicsBackend graphicsBackend, Config config, const char* name = "test_index_buffer")
        : UnitTest(graphicsBackend, config, name)
    {
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        const int NumTriangles = 63;
        const int NumVerts = NumTriangles + 1;
        const int NumIndices = NumTriangles * 3;
        const float R = 0.5f;
        const float4 color = { 0.5f, 0.0f, 0.5f, 1.0f };

        //initialize positions
        float2 pos[NumVerts];
        float2* posPtr = pos;
        *posPtr++ = { 0.0f, 0.0f };
        for (int j = 1; j < NumVerts; j++)
        {
            float theta = 2.0f * float(M_PI) * (float(j - 1) / float(NumVerts - 1));
            *posPtr++ = { R * cosf(theta), R * sinf(theta) };
        }

        //initialize indices
        uint16_t indices[NumIndices];
        uint16_t* indexPtr = indices;
        for (int j = 0; j < (NumTriangles - 1); j++)
        {
            *indexPtr++ = 0;
            *indexPtr++ = uint16_t(j + 2);
            *indexPtr++ = uint16_t(j + 1);
        }
        *indexPtr++ = 0;
        *indexPtr++ = 1;
        *indexPtr++ = NumVerts - 1;

        m_drawColorTask.init({ .device = m_device.Get(),
                               .color = color,
                               .posData = pos,
                               .numVerts = NumVerts,
                               .indicesData = indices,
                               .numIndices = NumIndices,
                               .primitiveTopology = PrimitiveTopology::eTriangleList,
                               .colorAttachmentFormat = m_colorAttachmentFormat,
                               .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DrawColorTask m_drawColorTask;
};

class TestPrimitiveRestart : public UnitTest
{
public:
    TestPrimitiveRestart(GraphicsBackend graphicsBackend, Config config, const char* name = "test_primitive_restart")
        : UnitTest(graphicsBackend, config, name)
    {
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        const int NumTriangles = 64;
        const int NumVerts = NumTriangles + 1;
        const int NumIndices = NumTriangles * 3;
        const float R = 0.5f;
        const float4 color = { 0.5f, 0.0f, 0.5f, 1.0f };

        //initialize positions
        float2 pos[NumVerts];
        float2* posPtr = pos;
        *posPtr++ = { 0.0f, 0.0f };
        for (int j = 1; j < NumVerts; j++)
        {
            float theta = 2.0f * float(M_PI) * (float(j - 1) / float(NumVerts - 1));
            *posPtr++ = { R * cosf(theta), R * sinf(theta) };
        }

        //initialize indices
        uint16_t indices[NumIndices];
        uint16_t* indexPtr = indices;
        bool draw = true;
        for (int j = 0; j < (NumTriangles - 1); j++)
        {
            if (draw)
            {
                *indexPtr++ = 0;
                *indexPtr++ = uint16_t(j + 2);
                *indexPtr++ = uint16_t(j + 1);
            }
            else
            {
                *indexPtr++ = 0xffff;
            }

            draw = draw ^ 1;
        }

        if (draw)
        {
            *indexPtr++ = 0;
            *indexPtr++ = 1;
            *indexPtr++ = NumVerts - 1;
        }
        else
        {
            *indexPtr++ = 0xffff;
        }

        m_drawColorTask.init({ .device = m_device.Get(),
                               .color = color,
                               .posData = pos,
                               .numVerts = NumVerts,
                               .indicesData = indices,
                               .numIndices = uint32_t(indexPtr - indices),
                               .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                               .colorAttachmentFormat = m_colorAttachmentFormat,
                               .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DrawColorTask m_drawColorTask;
};

static ResultCode compareOutputBuffer(uint32_t compareValue, const Buffer* buffer, uint64_t numElements)
{
    ScopedBufferMap m(const_cast<Buffer*>(buffer));
    uint32_t* data = (uint32_t*)m.map();
    for (uint64_t j = 0; j < numElements; j++)
    {
        if (data[j] != compareValue)
        {
            return ResultCode::eErrorOther;
        }
    }
    return ResultCode::eSuccess;
}

static ResultCode compareOutputBuffer(const void* buf1, const Buffer* buffer, uint64_t bufferOffset, uint64_t size)
{
    ScopedBufferMap m(const_cast<Buffer*>(buffer));
    void* buf2 = m.map(bufferOffset, size);
    return (0 == memcmp(buf1, buf2, size)) ? ResultCode::eSuccess : ResultCode::eErrorOther;
}

static void upload(const Buffer* dstBuffer, uint32_t dstOffset, const void* src, uint64_t size)
{
    void* dst = const_cast<Buffer*>(dstBuffer)->map(dstOffset, size);
    memcpy(dst, src, size);
    const_cast<Buffer*>(dstBuffer)->unmap();
}

class TestClearBuffer : public UnitTest
{
public:
    TestClearBuffer(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_buffer_clear")
    {
        m_numberOfElements = 10;
        m_clearValue = 10;
        m_config.bufferSize = sizeof(uint32_t) * m_numberOfElements;
        m_config.outputType = OutputType::eBuffer;
    }

    ~TestClearBuffer()
    {
        if (m_hostBuffer)
        {
            free(m_hostBuffer);
            m_hostBuffer = nullptr;
        }
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        initBuffers();
        return ResultCode::eSuccess;
    }

    void initBuffers()
    {
        m_hostBuffer = (uint32_t*)calloc(m_config.bufferSize, 1);

        m_inputBuffer.initAsUnorderedAccessBuffer(m_device.Get(), "InputBuffer", nullptr, m_config.bufferSize);

        beginTransfer();
        m_inputBuffer.clear(m_copyCommandList.Get(), m_clearValue);
        endTransfer();
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        beginTransfer();
        m_inputBuffer.downloadToReadbackBuffer(m_copyCommandList.Get(), 0, m_config.bufferSize, 0);
        endTransfer();

        return ::compareOutputBuffer(m_clearValue, m_inputBuffer.readbackBuffer.Get(), m_numberOfElements);
    }

    void recordCommandList(qhwi::CommandList* cmdList) override
    {
        cmdList->begin();
        cmdList->end();
    }

    Buffer1 m_inputBuffer{};
    int m_numberOfElements;
    uint8_t m_clearValue;
    uint32_t* m_hostBuffer = nullptr;
};

class TestUploadSubregionBuffer : public UnitTest
{
public:
    TestUploadSubregionBuffer(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_buffer_upload_subregion")
    {
        m_numberOfElements = 30;
        m_config.bufferSize = sizeof(uint32_t) * m_numberOfElements;
        m_config.outputType = OutputType::eBuffer;
    }

    ~TestUploadSubregionBuffer()
    {
        if (m_hostBuffer)
        {
            free(m_hostBuffer);
            m_hostBuffer = nullptr;
        }
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        initBuffers();
        return ResultCode::eSuccess;
    }

    void initBuffers()
    {
        m_hostBuffer = (uint32_t*)calloc(m_config.bufferSize, 1);
        fillArrayWithRandomNumbers(m_hostBuffer, m_numberOfElements);
        m_inputBuffer.initAsUnorderedAccessBuffer(m_device.Get(), "InputBuffer", nullptr, m_config.bufferSize);

        m_inputBuffer.stagingBuffer = m_device->createBuffer(
            { .heapType = HeapType::eUpload, .heapFlags = {}, .usage = {}, .size = m_config.bufferSize, .format = {}, .name = nullptr, .automaticBarriers = false });
        upload(m_inputBuffer.stagingBuffer.Get(), 0, m_hostBuffer, m_config.bufferSize);
        beginTransfer();
        m_inputBuffer.uploadFromStagingBuffer(m_copyCommandList.Get(), 0, 10 * sizeof(uint32_t), 0);
        m_inputBuffer.uploadFromStagingBuffer(m_copyCommandList.Get(), 10 * sizeof(uint32_t), 10 * sizeof(uint32_t), 10 * sizeof(uint32_t));
        m_inputBuffer.uploadFromStagingBuffer(m_copyCommandList.Get(), 20 * sizeof(int), 10 * sizeof(int), 20 * sizeof(uint32_t));
        endTransfer();
    }

    void fillArrayWithRandomNumbers(uint32_t* pFloatArray, int iSize)
    {
        for (int i = 0; i < iSize; i++)
        {
            pFloatArray[i] = i + 1;
        }
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        beginTransfer();
        m_inputBuffer.downloadToReadbackBuffer(m_copyCommandList.Get(), 0, m_config.bufferSize, 0);
        endTransfer();

        return ::compareOutputBuffer(m_hostBuffer, m_inputBuffer.readbackBuffer.Get(), 0, m_numberOfElements * sizeof(uint32_t));
    }

    void recordCommandList(qhwi::CommandList* cmdList) override
    {
        cmdList->begin();
        cmdList->end();
    }

    Buffer1 m_inputBuffer{};
    int m_numberOfElements;
    uint32_t* m_hostBuffer = nullptr;
};

class TestDownloadSubregionBuffer : public UnitTest
{
public:
    TestDownloadSubregionBuffer(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_buffer_download_subregion")
    {
        m_numberOfElements = 30;
        m_config.bufferSize = sizeof(uint32_t) * m_numberOfElements;
        m_config.outputType = OutputType::eBuffer;
    }

    ~TestDownloadSubregionBuffer()
    {
        if (m_hostBuffer)
        {
            free(m_hostBuffer);
            m_hostBuffer = nullptr;
        }
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        initBuffers();
        return ResultCode::eSuccess;
    }

    void initBuffers()
    {
        m_hostBuffer = (uint32_t*)calloc(m_config.bufferSize, 1);
        fillArrayWithRandomNumbers(m_hostBuffer, m_numberOfElements);
        m_inputBuffer.initAsUnorderedAccessBuffer(m_device.Get(), "InputBuffer", nullptr, m_config.bufferSize);

        m_inputBuffer.stagingBuffer = m_device->createBuffer(
            { .heapType = HeapType::eUpload, .heapFlags = {}, .usage = {}, .size = m_config.bufferSize, .format = {}, .name = nullptr, .automaticBarriers = false });
        upload(m_inputBuffer.stagingBuffer.Get(), 0, m_hostBuffer, m_config.bufferSize);
        beginTransfer();
        m_inputBuffer.uploadFromStagingBuffer(m_copyCommandList.Get(), 0, m_config.bufferSize, 0);
        endTransfer();
    }

    void fillArrayWithRandomNumbers(uint32_t* pFloatArray, int iSize)
    {
        for (int i = 0; i < iSize; i++)
        {
            pFloatArray[i] = i + 1;
        }
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        uint32_t r = 0;
        beginTransfer();
        m_inputBuffer.downloadToReadbackBuffer(m_copyCommandList.Get(), 0, 10 * sizeof(uint32_t));
        endTransfer();
        r |= (uint32_t)::compareOutputBuffer(m_hostBuffer, m_inputBuffer.readbackBuffer.Get(), 0, 10 * sizeof(uint32_t));

        beginTransfer();
        m_inputBuffer.downloadToReadbackBuffer(m_copyCommandList.Get(), 10 * sizeof(uint32_t), 10 * sizeof(uint32_t));
        endTransfer();
        r |= (uint32_t)::compareOutputBuffer(&m_hostBuffer[10], m_inputBuffer.readbackBuffer.Get(), 0, 10 * sizeof(uint32_t));

        beginTransfer();
        m_inputBuffer.downloadToReadbackBuffer(m_copyCommandList.Get(), 20 * sizeof(uint32_t), 10 * sizeof(uint32_t));
        endTransfer();
        r |= (uint32_t)::compareOutputBuffer(&m_hostBuffer[20], m_inputBuffer.readbackBuffer.Get(), 0, 10 * sizeof(uint32_t));

        return (qhwi::ResultCode)r;
    }

    void recordCommandList(qhwi::CommandList* cmdList) override
    {
        cmdList->begin();
        cmdList->end();
    }

    Buffer1 m_inputBuffer{};
    int m_numberOfElements;
    uint32_t* m_hostBuffer = nullptr;
};

UnitTest* create_test_vertex_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestVertexBuffer(backend, config);
}

UnitTest* create_test_index_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestIndexBuffer(backend, config);
}

UnitTest* create_test_primitive_restart(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPrimitiveRestart(backend, config);
}

UnitTest* create_test_buffer_clear(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestClearBuffer(backend, config);
}

UnitTest* create_test_buffer_upload_subregion(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestUploadSubregionBuffer(backend, config);
}

UnitTest* create_test_buffer_download_subregion(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDownloadSubregionBuffer(backend, config);
}
