#pragma pack_matrix(row_major)
// 빌보드 아이콘: 텍스처 한 장을 그대로 그린다 (b1 머티리얼 파라미터 없음)
cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

Texture2D IconTexture : register(t0);
SamplerState IconSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mul(float4(input.position, 1.0f), World), VP);
    output.uv = input.uv;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return IconTexture.Sample(IconSampler, input.uv);
}
