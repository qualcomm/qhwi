RWByteAddressBuffer globalCounter : register(u0);

[numThreads(16, 4, 2)]
void CSMain()
{
    uint oldValue = 0;
    globalCounter.InterlockedAdd(0, 1, oldValue);
}
