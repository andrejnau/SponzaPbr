Texture2DArray inputTexture;
RWTexture2DArray<float4> outputTexture;

[numthreads(8, 8, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    int4 sampleLocation = int4(2 * ThreadID.x, 2 * ThreadID.y, ThreadID.z, 0);
    float4 gatherValue =
        inputTexture.Load(sampleLocation, int2(0, 0)) +
        inputTexture.Load(sampleLocation, int2(1, 0)) +
        inputTexture.Load(sampleLocation, int2(0, 1)) +
        inputTexture.Load(sampleLocation, int2(1, 1));
    outputTexture[ThreadID] = 0.25 * gatherValue;
}
