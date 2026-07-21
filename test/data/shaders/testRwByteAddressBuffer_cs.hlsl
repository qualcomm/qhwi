StructuredBuffer<uint4>   inBuffer  : register(t0);
RWByteAddressBuffer       outBuffer : register(u0);

[numThreads(1, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    outBuffer.Store4(dispatchThreadID * sizeof(uint4), inBuffer[dispatchThreadID]);
}
