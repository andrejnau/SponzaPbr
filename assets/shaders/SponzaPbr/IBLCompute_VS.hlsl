#include "BoneTransform.hlsli"

struct VS_INPUT
{
    float3 pos        : POSITION;
    float3 normal     : NORMAL;
    float2 texCoord   : TEXCOORD;
    float3 tangent    : TANGENT;
    uint bones_offset : BONES_OFFSET;
    uint bones_count  : BONES_COUNT;
};

cbuffer ConstantBuf
{
    float4x4 model;
    float4x4 normalMatrix;
    float4x4 View[6];
    float4x4 Projection;
};

struct VS_OUTPUT
{
    float4 pos       : SV_POSITION;
    float3 fragPos   : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float2 texCoord  : TEXCOORD;
    uint RTIndex     : SV_RenderTargetArrayIndex;
};

VS_OUTPUT main(VS_INPUT vs_in, uint instanceID : SV_InstanceID)
{
// TODO: figure out why this does not work if bone_info and gBones are unbound
#if 0
    float4x4 transform = GetBoneTransform(vs_in.bones_count, vs_in.bones_offset);
#else
   float4x4 transform = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
#endif

    VS_OUTPUT vs_out;
    float4 pos = mul(float4(vs_in.pos, 1.0), transform);
    float4 worldPos = mul(pos, model);
    vs_out.fragPos = worldPos.xyz;
    float4 viewPosition = mul(worldPos, View[instanceID]);
    vs_out.pos = mul(viewPosition, Projection);
    vs_out.texCoord = vs_in.texCoord;
    vs_out.normal = mul(mul(vs_in.normal, transform), (float3x3)normalMatrix);
    vs_out.tangent = mul(mul(vs_in.tangent, transform), (float3x3)normalMatrix);
    vs_out.RTIndex = instanceID;
    return vs_out;
}
