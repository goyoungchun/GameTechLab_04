// 선형 색 공간의 SceneColor를 sRGB(디스플레이) 공간으로 인코딩한다.
// UI와 화면 오버레이는 sRGB로 만든 색이므로 이 패스 이후에 그린다.

Texture2D<float4> SceneColor : register(t0);

struct FPSInput
{
    float4 Position : SV_POSITION;
};

// 화면 전체를 덮는 삼각형
FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

// pow(1/2.2) 근사가 아닌 정확한 sRGB 곡선. 어두운 구간은 직선이다.
float3 LinearToSRGB(float3 Color)
{
    Color = saturate(Color);
    float3 Low = Color * 12.92f;
    float3 High = 1.055f * pow(Color, 1.0f / 2.4f) - 0.055f;
    return lerp(High, Low, step(Color, 0.0031308f));
}

float4 mainPS(FPSInput Input) : SV_Target
{
    float4 Color = SceneColor.Load(int3(Input.Position.xy, 0));
    return float4(LinearToSRGB(Color.rgb), Color.a);
}
