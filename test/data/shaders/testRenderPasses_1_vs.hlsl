struct VSInput
{
    float2 pos : POSITION0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = float4(input.pos, 0.5f, 1.0f);
    return result;
}