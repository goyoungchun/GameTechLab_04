// 카메라 화면 효과 오버레이: LetterBox, Vignette, Fade를 한 번에 알파 블렌드로 덮는다.
// 감마 보정 이후에 그려지므로 값은 화면에 보이는 밝기 기준이다.
cbuffer ScreenOverlayConstants : register(b0)
{
    float4 FadeColor;          // rgb: 색, a: 페이드 양
    float  LetterboxRatio;     // 위/아래 띠 비율
    float  VignetteIntensity;  // 가장자리 최대 어둡기
    float  VignetteRadius;     // 어두워지기 시작하는 거리
    float  VignetteSoftness;   // 어두워지는 구간 폭
    float  AspectRatio;        // 가로/세로. 비네팅을 원형으로 만들 때 사용
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

// 화면 중심에서 0, 모서리에서 1이 되는 거리 (화면 비율을 반영해서 원형)
float VignetteDistance(float2 UV)
{
    float2 P = UV - 0.5f;
    P.x *= AspectRatio;
    float2 Corner = float2(0.5f * AspectRatio, 0.5f);
    return length(P) / length(Corner);
}

float4 mainPS(FPSInput Input) : SV_Target
{
    // 1) LetterBox: 띠 영역은 완전히 검정
    if (Input.UV.y < LetterboxRatio || Input.UV.y > 1.0f - LetterboxRatio)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // 2) Vignette: 띠 안쪽의 실제 화면 영역 기준으로 계산
    float2 ContentUV = Input.UV;
    ContentUV.y = (Input.UV.y - LetterboxRatio) / max(1.0f - 2.0f * LetterboxRatio, 0.001f);
    float VignetteAlpha = VignetteIntensity *
        smoothstep(VignetteRadius, VignetteRadius + VignetteSoftness, VignetteDistance(ContentUV));

    // 3) Fade를 비네팅 위에 덮는다 (Fade over Vignette 알파 합성, 비네팅 색은 검정)
    float FadeAlpha = FadeColor.a;
    float OutAlpha = FadeAlpha + VignetteAlpha * (1.0f - FadeAlpha);
    float3 OutColor = FadeColor.rgb * FadeAlpha / max(OutAlpha, 0.0001f);

    return float4(OutColor, OutAlpha);
}
