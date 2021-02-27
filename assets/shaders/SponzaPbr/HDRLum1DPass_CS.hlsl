StructuredBuffer<float> data;
RWStructuredBuffer<float> result;

cbuffer cb
{
    uint bufferSize;
};

#define numthread 128

groupshared float accum[numthread];

[numthreads(numthread, 1, 1)]
void main(uint3 threadId : SV_DispatchthreadId, uint groupId : SV_GroupIndex, uint3 dispatchId : SV_GroupID)
{
    if (threadId.x < bufferSize)
        accum[groupId] = data[threadId.x];
    else
        accum[groupId] = 0;
    GroupMemoryBarrierWithGroupSync();
    [unroll]
    for (uint block_size = numthread / 2; block_size >= 1; block_size >>= 1)
    {
        if (groupId < block_size)
        {
            accum[groupId] += accum[groupId + block_size];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (groupId == 0)
    {
        result[dispatchId.x] = accum[0];
    }
}
