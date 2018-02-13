cbuffer ConstantBuffer : register(b0)
{
    matrix  mat;
};

struct VS_Input
{
    float3 pos : POSITION;
    float4 cr : COLOR;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float4 cr : COLOR;
};

PS_Input UIVS(VS_Input input)
{
    PS_Input ret = (PS_Input)0;
    ret.pos = mul(float4(input.pos, 1.f), mat);
    ret.cr = input.cr;
    return ret;
}

float4 UIPS(PS_Input input) : SV_TARGET
{
    return input.cr;
}
