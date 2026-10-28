cbuffer ScreenOverlayConstants : register(b0)
{
    float4 FadeColor;
    float LetterboxRatio;
    float3 Padding;
}

struct FPSInput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    Output.UV = UV;
    return Output;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    if (Input.UV.y < LetterboxRatio || Input.UV.y > 1.0f - LetterboxRatio)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    return FadeColor;
}