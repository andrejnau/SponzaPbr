cbuffer VSParams
{
    float4x4 World;
    float4x4 View[6];
    float4x4 Projection;
};

struct VertexInput
{
    float3 Position   : SV_POSITION;
    float2 texCoord   : TEXCOORD;
};

struct VertexOutput
{
    float4 pos : SV_POSITION;
    float2 texCoord  : TEXCOORD;
    uint RTIndex : SV_RenderTargetArrayIndex;
};

VertexOutput main(VertexInput input, uint instanceID : SV_InstanceID)
{
    VertexOutput output;
    float4 worldPosition = mul(float4(input.Position, 1.0), World);
    float4 viewPosition = mul(worldPosition, View[instanceID]);
    output.pos = mul(viewPosition, Projection);
    output.texCoord = input.texCoord;
    output.RTIndex = instanceID;
    return output;
}
