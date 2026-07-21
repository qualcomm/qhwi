struct VSInput
{
    float4 pos    : POSITION0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};

struct CBInput
{
    float4x4 modelViewProj[2];
};
[[vk::push_constant]] ConstantBuffer<CBInput> cbIn : register(b0);

VSOutput VSMain(VSInput input, uint ViewId : SV_ViewID)
{
    VSOutput result;
    result.pos   = mul(cbIn.modelViewProj[ViewId], input.pos);
    result.color = input.color;
    return result;
}
