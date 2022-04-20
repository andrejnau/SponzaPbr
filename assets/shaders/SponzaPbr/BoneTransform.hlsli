struct BoneInfo
{
    uint bone_id;
    float weight;
};

StructuredBuffer<BoneInfo> bone_info;
StructuredBuffer<float4x4> gBones;

float4x4 GetBoneTransform(uint bones_count, uint bones_offset)
{
    if (bones_count == 0)
    {
        float4x4 transform = {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        };
        return transform;
    }

    float4x4 transform = {
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 }
    };
    for (uint i = 0; i < bones_count; ++i)
    {
        transform += bone_info[i + bones_offset].weight * gBones[bone_info[i + bones_offset].bone_id];
    }
    return transform;
}
