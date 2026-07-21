RWStructuredBuffer<uint> outBuffer : register(u0);

struct cBuffer
{
    uint referenceValue;
};

[[vk::push_constant]] ConstantBuffer<cBuffer> cb : register(b0);

[numThreads(4, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    outBuffer[dispatchThreadID] = cb.referenceValue;
}
