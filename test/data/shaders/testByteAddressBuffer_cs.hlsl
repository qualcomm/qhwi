ByteAddressBuffer  inBuffer  : register(t0);
RWStructuredBuffer<uint4> outBuffer : register(u0);

[numThreads(1, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    outBuffer[dispatchThreadID] = inBuffer.Load4(dispatchThreadID * sizeof(uint4));
}
