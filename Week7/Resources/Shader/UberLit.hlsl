#pragma pack_matrix(row_major)


struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float4 Tangent : TANGENT;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float4 Tangent : TANGENT;
};

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};
cbuffer WorldConstants : register(b2)
{
    matrix World;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float bOpaque;     
    float _materialPad;
};

Texture2D g_txColor : register(t0);
Texture2D g_txNormal : register(t3);

SamplerState g_Sample : register(s0);

float3 GetWorldNormal(float3 Normal, float4 Tangent, float2 UV)
{
    float3 N = normalize(Normal);
    uint Width, Height;
    g_txNormal.GetDimensions(Width, Height);
    if (Width == 0 || Height == 0)
        return N;

    float3 T = Tangent.xyz - N * dot(N, Tangent.xyz);
    float TangentLengthSq = dot(T, T);
    // UV가 없거나 퇴화해 유효한 탄젠트가 없으면 정점 노멀을 유지한다.
    if (!(TangentLengthSq > 0.000001f))
        return N;
    T *= rsqrt(TangentLengthSq);
    float S = Tangent.w < 0.0f ? -1.0f : 1.0f;
    float3 B = cross(N, T) * S;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud는 정점에서 노멀맵과 조명을 계산한다.
    float3 NormalTS = g_txNormal.SampleLevel(g_Sample, UV, 0).rgb * 2.0f - 1.0f;
#else
    float3 NormalTS = g_txNormal.Sample(g_Sample, UV).rgb * 2.0f - 1.0f;
#endif
    return normalize(NormalTS.x * T + NormalTS.y * B + NormalTS.z * N);
}

struct AmbientLightInfo
{
    float4 ColorIntensity;
};
struct DirectionalLightInfo
{
    float4 ColorIntensity;
    float4 Direction;
};

struct PointLightInfo
{
    float4 ColorIntensity;
    float3 Position;
    float AttenuationRadius;
    float Falloff;
    float3 _pad;
};

struct SpotLightInfo
{
    float4 ColorIntensity;
    float3 Position;
    float AttenuationRadius;
    float3 Direction;
    float Falloff;
    float CosInner;
    float CosOuter;
    float2 _pad; 
};

cbuffer LightBuffer : register(b4)
{
    AmbientLightInfo AmbientLight;
    DirectionalLightInfo DirectionalLight;
    uint PointLightCount;
    uint SpotLightCount;
    float2 _lightPad;
};

cbuffer CameraConstants : register(b5)
{
    float3 CameraPosition;
    float _cameraPad;
};

StructuredBuffer<PointLightInfo> g_PointLights : register(t1);
StructuredBuffer<SpotLightInfo> g_SpotLights : register(t2);

// ---- 타일 라이트 컬링 결과 (TileLightCulling.hlsl이 기록한다) ----
// C++ TileSize / MaxLightsPerTile, TileLightCulling.hlsl의 같은 이름 상수와 값이 같아야 한다
#define TILE_SIZE           16
#define MAX_LIGHTS_PER_TILE 1024

cbuffer TileLightInfo : register(b6)
{
    uint TileCountX;        // 타일 번호 = y * TileCountX + x
    uint bUseTileCulling;   // 1: 자기 타일 목록만 순회, 0: 전체 루프
    uint2 _tilePad;
};

// 타일마다 [count][통합 라이트 번호 ...]
// 통합 번호 < PointLightCount 이면 포인트, 아니면 스폿 [번호 - PointLightCount]
// t3는 노멀맵(g_txNormal)이 쓰므로 t4에 둔다. C++ BindLights·LightCulling의 슬롯 번호와 같아야 한다
StructuredBuffer<uint> g_TileLightList : register(t4);

float DistanceAtten(float Dist, float Radius, float Falloff)
{
    return pow(saturate(1.0 - Dist / Radius), Falloff);
}

// 반사 지수. 클수록 하이라이트가 작고 날카롭다. 나중에 머티리얼 파라미터로 뺀다.
static const float Shininess = 64.0f;

float3 CalcSpecular(float3 N, float3 L, float3 V, float3 Radiance)
{
    float3 H = normalize(L + V);
    return Radiance * pow(saturate(dot(N, H)), Shininess);
}

struct FLightResult
{
    float3 Diffuse;
    float3 Specular;
};

FLightResult CalcLightTerm(float3 N, float3 L, float3 V, float3 Radiance)
{
    FLightResult R;
    float NdotL = saturate(dot(N, L));
    R.Diffuse = Radiance * NdotL;
    R.Specular = 0;

#if LIGHTING_MODEL_PHONG
    // 빛이 닿지 않는 뒷면에는 하이라이트를 만들지 않는다
    if (NdotL > 0.0f)
        R.Specular = CalcSpecular(N, L, V, Radiance);
#endif
    return R;
}

FLightResult CalcPointLight(PointLightInfo L, float3 WorldPos, float3 N, float3 V)
{
    float3 ToLight = L.Position - WorldPos;
    float Dist = length(ToLight);
    float3 Dir = ToLight / max(Dist, 0.0001f);

    float Atten = DistanceAtten(Dist, L.AttenuationRadius, L.Falloff);
    return CalcLightTerm(N, Dir, V, L.ColorIntensity.rgb * L.ColorIntensity.w * Atten);
}

FLightResult CalcSpotLight(SpotLightInfo L, float3 WorldPos, float3 N, float3 V)
{
    float3 ToLight = L.Position - WorldPos;
    float Dist = length(ToLight);
    float3 Dir = ToLight / max(Dist, 0.0001f);

    float CosAngle = dot(-Dir, normalize(L.Direction));
    float ConeAtten = smoothstep(L.CosOuter, L.CosInner, CosAngle);
    //ConeAtten = saturate((L.CosOuter - CosAngle) / (L.CosOuter - L.CosInner));;
    float Atten = DistanceAtten(Dist, L.AttenuationRadius, L.Falloff) * ConeAtten;
    return CalcLightTerm(N, Dir, V, L.ColorIntensity.rgb * L.ColorIntensity.w * Atten);
}


// Ambient + Directional: 화면 전체에 닿으므로 타일과 무관하다
FLightResult CalcAmbientAndDirectional(float3 N, float3 V)
{
    //Ambient 값 
    FLightResult Result;
    Result.Diffuse = AmbientLight.ColorIntensity.rgb * AmbientLight.ColorIntensity.w;
    Result.Specular = 0;

    // Direction이 0이면(Directional 라이트 없음) normalize가 NaN이 되어 결과 전체가 깨지므로 길이로 거른다
    float3 DirLightDir = DirectionalLight.Direction.xyz;

    if (dot(DirLightDir, DirLightDir) > 0.0f)
    {
        float3 DirL = -normalize(DirLightDir);
        FLightResult R = CalcLightTerm(N, DirL, V, DirectionalLight.ColorIntensity.rgb * DirectionalLight.ColorIntensity.w);
        Result.Diffuse += R.Diffuse;
        Result.Specular += R.Specular;
    }

    return Result;
}

// 통합 번호 하나를 포인트 또는 스폿으로 풀어 계산한다 (TileLightCulling.hlsl과 같은 규칙)
// 0 ~ PointLightCount-1 → 포인트, 그 뒤 → 스폿 [번호 - PointLightCount]
FLightResult CalcLightByIndex(uint LightIndex, float3 WorldPos, float3 N, float3 V)
{
    if (LightIndex < PointLightCount)
        return CalcPointLight(g_PointLights[LightIndex], WorldPos, N, V);
    return CalcSpotLight(g_SpotLights[LightIndex - PointLightCount], WorldPos, N, V);
}

// 전체 루프: Gouraud VS, 그리고 타일 결과가 없을 때 (와이어프레임·직교 뷰, 반투명)
FLightResult CalcAllLights(float3 WorldPos, float3 N, float3 V)
{
    FLightResult Result = CalcAmbientAndDirectional(N, V);
    uint TotalLightCount = PointLightCount + SpotLightCount;
    for (uint i = 0; i < TotalLightCount; ++i)
    {
        FLightResult R = CalcLightByIndex(i, WorldPos, N, V);
        Result.Diffuse += R.Diffuse;
        Result.Specular += R.Specular;
    }
    return Result;
}

// 타일 경로 (PS 전용): 자기 타일에 닿는 라이트만 순회한다
// PixelPos = SV_POSITION.xy (픽셀 중심 좌표). 뷰포트 시작점이 (0,0)이라는 전제
FLightResult CalcTiledLights(float3 WorldPos, float3 N, float3 V, float2 PixelPos)
{
    FLightResult Result = CalcAmbientAndDirectional(N, V);

    uint2 Tile = uint2(PixelPos) / TILE_SIZE;                                       // 픽셀 → 타일 좌표
    uint TileBase = (Tile.y * TileCountX + Tile.x) * (MAX_LIGHTS_PER_TILE + 1);   // CS와 같은 주소 계산
    uint Count = min(g_TileLightList[TileBase], MAX_LIGHTS_PER_TILE);             // [타일][0] = 개수. 쓰레기 값이어도 루프가 폭주하지 않게 자른다

    // 용량에 도달한 타일은 전체 루프로 돌아가 조명이 누락되지 않게 한다.
    if (Count == MAX_LIGHTS_PER_TILE)
        return CalcAllLights(WorldPos, N, V);

    for (uint k = 0; k < Count; ++k)
    {
        FLightResult R = CalcLightByIndex(g_TileLightList[TileBase + 1 + k], WorldPos, N, V);
        Result.Diffuse += R.Diffuse;
        Result.Specular += R.Specular;
    }
    return Result;
}

PSInput mainVS(VSInput Input)
{
    PSInput Out;
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
    float3 WorldN = normalize(mul(float4(Input.Normal, 0.0f), World).xyz); // 비균등 스케일이면 역전치 행렬 필요

    Out.Position = mul(WorldPos, VP);
    Out.worldPos = WorldPos.xyz;
    Out.Normal = WorldN;
    Out.UV = Input.UV;
    Out.Tangent = float4(mul(float4(Input.Tangent.xyz, 0.0f), World).xyz,Input.Tangent.w);

#if LIGHTING_MODEL_GOURAUD
    // 정점에서는 라이팅만 계산해 넘기고, 텍스처 색은 PS에서 곱한다 (Gouraud는 Specular가 없다)
    float3 V = normalize(CameraPosition - WorldPos.xyz);
    float3 N = GetWorldNormal(WorldN, Out.Tangent, Out.UV + UVOffset);
    FLightResult Light = CalcAllLights(WorldPos.xyz, N, V);
    Out.Color    = float4(Light.Diffuse, 1.0f);
#else
    Out.Color = Input.Color; // Lambert/Phong은 PS에서 계산
#endif
    return Out;
}

float4 mainPS(PSInput Input) : SV_Target
{
    // 표면색 = 텍스처 * BaseColor (StaticMeshShader와 같다)
    float4 Albedo = g_txColor.Sample(g_Sample, Input.UV + UVOffset) * BaseColor;
    float3 FinalRGB = Albedo.rgb;
   
#if LIGHTING_MODEL_GOURAUD
    // VS가 보간해 준 라이팅
    FinalRGB = Albedo.rgb * Input.Color.rgb;
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    float3 N = GetWorldNormal(Input.Normal, Input.Tangent, Input.UV + UVOffset);
    float3 V = normalize(CameraPosition - Input.worldPos);
    FLightResult Light;
    if (bUseTileCulling != 0)
        Light = CalcTiledLights(Input.worldPos, N, V, Input.Position.xy);   // 자기 타일 목록만 순회
    else
        Light = CalcAllLights(Input.worldPos, N, V);   // Lambert면 Specular는 0
    // 스페큘러는 표면색을 곱하지 않는다
    FinalRGB = Albedo.rgb * Light.Diffuse + Light.Specular;
#endif

    // Opaque는 알파를 1로 고정한다. 뷰포트 RT를 ImGui가 알파 블렌딩으로 그리므로 알파가 남으면 비쳐 보인다
    float Alpha = bOpaque > 0.5f ? 1.0f : Albedo.a;
    return float4(FinalRGB, Alpha);

}
