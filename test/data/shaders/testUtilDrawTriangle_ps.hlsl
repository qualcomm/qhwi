struct VSOutput
{
    float4 pos      : SV_POSITION;
    float4 color    : TEXCOORD0;
};

float4 PSMain(VSOutput input) : SV_TARGET
{
    return input.color;
}
