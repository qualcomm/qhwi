struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 uvw : TEXCOORD0;
};

TextureCube  texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    return texture0.Sample(sampler0, input.uvw);
}
