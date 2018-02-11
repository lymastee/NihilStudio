cbuffer ConstantBuffer : register(b0)
{
    matrix mvp;
    float3 diffuseColor;
}

struct VS_Input
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float4 cr : COLOR0;
};

static const float3 lightPos = float3(10.f, 10.f, 10.f);
static const float3 ambientColor = float3(0.1f, 0.f, 0.f);
//static const float3 diffuseColor = float3(0.5f, 0.f, 0.f);
static const float3 specColor = float3(1.f, 1.f, 1.f);
static const float shininess = 22.f;

PS_Input VS(VS_Input input)
{
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(lightPos - input.pos);
    float lambertian = max(dot(lightDir, normal), 0.f);
    float specular = 0.f;
    if(lambertian > 0.f)
    {
        float3 viewDir = normalize(-input.pos);
        float3 halfDir = normalize(lightDir + viewDir);
        float specAngle = max(dot(halfDir, normal), 0.f);
        specular = pow(specAngle, shininess);
    }

    float3 colorLinear = ambientColor + lambertian * diffuseColor + specular * specColor;

    PS_Input output = (PS_Input)0;
    output.pos = mul(float4(input.pos, 1.f), mvp);
    output.cr = float4(colorLinear, 1.f);

    return output;
}

float4 PS(PS_Input input) : SV_TARGET
{
    return input.cr;
}
