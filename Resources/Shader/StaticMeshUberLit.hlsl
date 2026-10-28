#pragma pack_matrix(row_major)

#define SHADING_MODEL_LAMBERT 0
#define SHADING_MODEL_PHONG 1
#define SHADING_MODEL_BLINN_PHONG 2

#ifndef SHADING_MODEL
#define SHADING_MODEL SHADING_MODEL_BLINN_PHONG
#endif

#ifndef LIGHTING_ENABLED
#define LIGHTING_ENABLED 1
#endif

#ifndef SHADOW_ENABLED
#define SHADOW_ENABLED 1
#endif

#define SHADOW_FILTER_HARD 0
#define SHADOW_FILTER_PCF3x3 1
#define SHADOW_FILTER_PCF5x5 2
#define SHADOW_FILTER_POISSON 3
#define SHADOW_FILTER_PCSS 4

#ifndef SHADOW_FILTER_MODE
#define SHADOW_FILTER_MODE SHADOW_FILTER_PCSS
#endif

#define SPECULAR_NONE 0
#define SPECULAR_PHONG 1
#define SPECULAR_BLINN_PHONG 2

#if SHADING_MODEL == SHADING_MODEL_LAMBERT
#define SPECULAR_MODEL SPECULAR_NONE
#elif SHADING_MODEL == SHADING_MODEL_PHONG
#define SPECULAR_MODEL SPECULAR_PHONG
#elif SHADING_MODEL == SHADING_MODEL_BLINN_PHONG
#define SPECULAR_MODEL SPECULAR_BLINN_PHONG
#else
	#error "Invalid SHADING_MODEL"
#endif

#define MAX_DIRECTIONAL_LIGHT 8
#define INVALID_SHADOW_SLOT 0xFFFFFFFF

struct AmbientLightInfo
{
    float3 Color;
    float Intensity;
};

struct DirectionalLightInfo
{
	float3 Color;
	float Intensity;

	float3 Direction;
    float ShadowAngularRadiusTan;

	float ShadowTexelWorld[8];
	float ShadowNear[8];
	float ShadowFar[8];
	uint ShadowMapIndex[8];
    float ShadowSplitDepths[8];
	matrix ViewProjection[8];

	uint CascadeCount;
	uint CascadeExponential;
    float CascadeBlendRange;
	float bCascadeDebug;
};

struct PointLightInfo
{
	float3 Color;
	float Intensity;
	float3 Location;
	float AttenuationRadiusSquaredInversed;
	matrix ViewProjection[6];
	float ShadowSourceRadius;
	float ShadowNear;
	float ShadowFar;
	float ShadowProjectionScale;
    float4 ShadowAtlasRect[6];
};

struct SpotLightInfo
{
	float3 Color;
	float Intensity;
	float3 Location;
	float AttenuationRadiusSquaredInversed;
	float3 Direction;
	float CosInner;
	float CosOuter;
	matrix ViewProjection;
	float ShadowSourceRadius;
	float ShadowNear;
	float ShadowFar;
	float ShadowProjectionScale;
    float4 ShadowAtlasRect;
};

cbuffer FrameConstants : register(b0)
{
	matrix VP;
	matrix ViewMatrix;
	float3 CameraPosition;
	uint NumDirectionalLights;
	uint NumPointLights;
	uint NumSpotLights;
	uint TileCountX; // 0이면 전체 라이트 순회
	uint MaxLightsPerTile;
	AmbientLightInfo AmbientLight;
	uint TileSize;
	float ShadowFilterSpacing;
	float2 ShadowMapTexelSize;
	float Near;
	float Far;
    float2 LocalAtlasTexelSize;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float3 SpecularColor;
    float Shininess;
    float2 UVOffset;
    uint bUseNormalMap;
    float MaterialParamsPadding;
};

cbuffer WorldConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
	
    uint ReceiveShadow;
    uint3 ObjectPadding;
};

static const float2 PoissonDisk[32] =
{
    float2(0.12500000f, 0.00000000f),
    float2(-0.15964505f, 0.14624794f),
    float2(0.02443623f, -0.27843827f),
    float2(0.20122224f, 0.26245878f),
    float2(-0.36926756f, -0.06531823f),
    float2(0.34980247f, -0.22251570f),
    float2(-0.11700208f, 0.43524190f),
    float2(-0.22313565f, -0.42963412f),
    float2(0.48411512f, 0.17679806f),
    float2(-0.50364111f, 0.20789573f),
    float2(0.24278829f, -0.51882448f),
    float2(0.17941437f, 0.57200130f),
    float2(-0.54075701f, -0.31337974f),
    float2(0.63436952f, -0.13946436f),
    float2(-0.38714585f, 0.55067513f),
    float2(-0.08943965f, -0.69019964f),
    float2(0.54907176f, 0.46275826f),
    float2(-0.73887847f, 0.03055494f),
    float2(0.53895513f, -0.53633233f),
    float2(-0.03605819f, 0.77979152f),
    float2(-0.51281753f, -0.61452679f),
    float2(0.81235959f, 0.10930183f),
    float2(-0.68831064f, 0.47890862f),
    float2(0.18808606f, -0.83606138f),
    float2(0.43503327f, 0.75919105f),
    float2(-0.85044841f, -0.27131624f),
    float2(0.82610240f, -0.38168026f),
    float2(-0.35788820f, 0.85515556f),
    float2(-0.31940733f, -0.88803376f),
    float2(0.84990864f, 0.44668816f),
    float2(-0.94403465f, 0.24884450f),
    float2(0.53659581f, -0.83452977f)
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD1;
    float4 Tangent : TANGENT;
};
Texture2D Texture : register(t0);
Texture2D NormalMap : register(t1);
SamplerState Sampler : register(s0);

#if LIGHTING_ENABLED == 1
Buffer<uint> TileLightCount : register(t8);
Buffer<uint> TileLightIndices : register(t9);
#endif

StructuredBuffer<DirectionalLightInfo> DirectionalLights : register(t10);
StructuredBuffer<PointLightInfo> PointLights : register(t11);
StructuredBuffer<SpotLightInfo> SpotLights : register(t12);
Texture2DArray<float> ShadowMaps : register(t13); // 우선 Directional용으로.
Texture2D<float> LocalShadowAtlas : register(t14);

SamplerComparisonState ShadowSampler : register(s1);
SamplerState ShadowDepthSampler : register(s2);

static const float InvPi = 0.31830988618f;

struct FAtlasTile
{
    float4 Rect;
    float2 ClampMin;
    float2 ClampMax;
};

FAtlasTile MakeAtlasTile(float4 Rect)
{
    FAtlasTile Tile;
    Tile.Rect = Rect;
    Tile.ClampMin = Rect.xy + 0.5 * LocalAtlasTexelSize;
    Tile.ClampMax = Rect.xy + Rect.zw - 0.5 * LocalAtlasTexelSize;
    return Tile;
}

float2 ToAtlasUV(float2 LocalUV, FAtlasTile Tile)
{
    return LocalUV * Tile.Rect.zw + Tile.Rect.xy;
}

float SampleAtlasCmp(float2 AtlasUV, float ReceiverDepth, FAtlasTile Tile)
{
    return LocalShadowAtlas.SampleCmpLevelZero(ShadowSampler, clamp(AtlasUV, Tile.ClampMin, Tile.ClampMax), ReceiverDepth);
}

float SampleAtlasDepth(float2 AtlasUV, FAtlasTile Tile)
{
    return LocalShadowAtlas.SampleLevel(ShadowDepthSampler, clamp(AtlasUV, Tile.ClampMin, Tile.ClampMax), 0.0f);
}

float3 CalculateDiffuse()
{
    return InvPi;
}

float3 SpecularPhong(float3 R, float3 V, float Shininess, float Normalization)
{
    float RdotV = saturate(dot(R, V));
    return pow(RdotV, Shininess) * Normalization;
}

float3 SpecularBlinnPhong(float3 N, float3 H, float Shininess, float Normalization)
{
    float NdotH = saturate(dot(N, H));
    return pow(NdotH, Shininess) * Normalization;
}

float3 CalculateSpecular(float3 N, float3 L, float3 V, float Shininess, float Normalization)
{
#if SPECULAR_MODEL == SPECULAR_PHONG
	return SpecularPhong(reflect(-L, N), V, Shininess / 4.0f, Normalization);
#elif SPECULAR_MODEL == SPECULAR_BLINN_PHONG
	float3 H = normalize(L + V);
	return SpecularBlinnPhong(N, H, Shininess, Normalization);
#endif
    return float3(0.0f, 0.0f, 0.0f);
}

// 컬링 반경에 도달하면 기여도가 0이 된다.
float CalculateDistanceAttenuation(float DistanceSquared, float RadiusSquaredInversed)
{
    float NormalizedDistanceSquared = DistanceSquared * RadiusSquaredInversed;
    float AttenuationFade = saturate(1.0f - NormalizedDistanceSquared * NormalizedDistanceSquared);
    return AttenuationFade * AttenuationFade / (DistanceSquared + 1.0f);
}

float CalculateSpotAngleAttenuation(float CosAngle, float CosInner, float CosOuter)
{
    float AttenuationMask = saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 0.0001f));
	
    return AttenuationMask * AttenuationMask;
}

float ReceiverPlaneOffset(float3 ReceiverBias, float2 OffsetUV)
{
    float PlaneOffset = clamp(dot(ReceiverBias.xy, OffsetUV), -0.003f, 0.0f);
	
    float ConeOffset = ReceiverBias.z * length(OffsetUV);
    return PlaneOffset - ConeOffset;
}

float CalculatePCF(float2 LightUV, float ReceiverDepth, float3 ReceiverBias, uint Slice, int SampleCount, float FilterSpacing = 1.0f)
{
    float Visibility = 0.0f;

    for (int y = -SampleCount; y <= SampleCount; ++y)
    {
        for (int x = -SampleCount; x <= SampleCount; ++x)
        {
            float2 DeltaUV = float2(x, y) * ShadowMapTexelSize * FilterSpacing;
            float2 SampleUV = LightUV + DeltaUV;
            float TapDepth = ReceiverDepth + ReceiverPlaneOffset(ReceiverBias, DeltaUV);
            Visibility += ShadowMaps.SampleCmpLevelZero(ShadowSampler, float3(SampleUV, Slice), TapDepth);
        }
    }

    float Width = SampleCount * 2 + 1;
    return Visibility / (Width * Width);
}

float CalculateAtlasPCF(float2 AtlasUV, float ReceiverDepth, float3 ReceiverBias, FAtlasTile Tile, int SampleCount, float FilterSpacing = 1.0f)
{
    float Visibility = 0.0f;
	
    for (int y = -SampleCount; y <= SampleCount; ++y)
    {
        for (int x = -SampleCount; x <= SampleCount; ++x)
        {
            float2 DeltaUV = float2(x, y) * LocalAtlasTexelSize;
            float2 SampleUV = AtlasUV + DeltaUV * FilterSpacing;
            float TapDepth = ReceiverDepth + ReceiverPlaneOffset(ReceiverBias, DeltaUV * FilterSpacing);
            Visibility += SampleAtlasCmp(SampleUV, TapDepth, Tile);
        }
    }
    float Width = SampleCount * 2 + 1;
    return Visibility / (Width * Width);
}

float2 ComputeRPDBdZduv(float4 LightClip, matrix LightVP, float3 dPdx, float3 dPdy)
{
    float4 dCdx = mul(float4(dPdx, 0.0f), LightVP);
    float4 dCdy = mul(float4(dPdy, 0.0f), LightVP);
	
    float InvW = 1.0f / LightClip.w;
    float3 NDC = LightClip.xyz * InvW;
	
    float3 dNdx = (dCdx.xyz - NDC * dCdx.w) * InvW;
    float3 dNdy = (dCdy.xyz - NDC * dCdy.w) * InvW;
	
    float3 Dx = float3(0.5f * dNdx.x, -0.5f * dNdx.y, dNdx.z); // (u_x, v_x, z_x)
    float3 Dy = float3(0.5f * dNdy.x, -0.5f * dNdy.y, dNdy.z); // (u_y, v_y, z_y)
	
    float Det = Dx.x * Dy.y - Dx.y * Dy.x;
    float Scale = dot(Dx.xy, Dx.xy) * dot(Dy.xy, Dy.xy);
    if (Det * Det < 1e-6f * Scale)
        return 0.0f;

    return float2(Dy.y * Dx.z - Dx.y * Dy.z, Dx.x * Dy.z - Dy.x * Dx.z) / Det;
}

float ComputeTapConeSlope(float4 LightClip, matrix LightVP)
{
    float InvW = 1.0f / LightClip.w;
    float3 dZdP = (float3(LightVP[0][2], LightVP[1][2], LightVP[2][2])
        - LightClip.z * InvW * float3(LightVP[0][3], LightVP[1][3], LightVP[2][3])) * InvW;

    float ScaleX = length(float3(LightVP[0][0], LightVP[1][0], LightVP[2][0]));
    float ScaleY = length(float3(LightVP[0][1], LightVP[1][1], LightVP[2][1]));
    float WorldPerUV = 2.0f * LightClip.w / min(ScaleX, ScaleY);

    return length(dZdP) * WorldPerUV;
}

float TexelWorldSize(matrix VP, float ClipW, float TexelSize)
{
    float ScaleX = length(float3(VP[0][0], VP[1][0], VP[2][0]));
    float ScaleY = length(float3(VP[0][1], VP[1][1], VP[2][1]));
    return 2.0f * TexelSize * ClipW / min(ScaleX, ScaleY);
}

float3 ShadowNormalOffset(float3 Position, float3 Normal, float NdotL, matrix VP, float TexelSize)
{
    float ClipW = mul(float4(Position, 1.0f), VP).w;
	
    float SinTheta = sqrt(saturate(1.0f - NdotL * NdotL));
    return normalize(Normal) * TexelWorldSize(VP, ClipW, TexelSize) * SinTheta * 2.0f;
}

float CalculatePoissonPCF(float2 LightUV, float ReceiverDepth, float3 ReceiverBias, uint Slice, float2 FilterRadiusUV)
{
    float Visibility = 0.0f;
	
    float2 Pixel = floor(LightUV / ShadowMapTexelSize);
    float Hash = frac(sin(dot(Pixel, float2(12.9898f, 78.233f))) * 43758.5453f) * 6.2831853f;
    float Angle = Hash * 3.14159265f;
	
    float S;
    float C;
    sincos(Angle, S, C);

    [loop]
    for (int i = 0; i < 32; ++i)
    {
        float2 Offset = float2(C * PoissonDisk[i].x - S * PoissonDisk[i].y, S * PoissonDisk[i].x + C * PoissonDisk[i].y);
		
        float2 DeltaUV = Offset * FilterRadiusUV;
        float2 SampleUV = LightUV + DeltaUV;
        float TapDepth = ReceiverDepth + ReceiverPlaneOffset(ReceiverBias, DeltaUV);
        Visibility += ShadowMaps.SampleCmpLevelZero(ShadowSampler, float3(SampleUV, Slice), TapDepth);
    }

    return Visibility / 32.0f;
}

float CalculateAtlasPoissonPCF(float2 AtlasUV, float ReceiverDepth, float3 ReceiverBias, FAtlasTile Tile, float2 FilterRadiusUV)
{
    float Visibility = 0.0f;
    float2 Pixel = floor(AtlasUV / LocalAtlasTexelSize);
    float Hash = frac(sin(dot(Pixel, float2(12.9898f, 78.233f))) * 43758.5453f) * 6.2831853f;
    float Angle = Hash * 3.14159265f;
	
    float S;
    float C;
    sincos(Angle, S, C);

    [loop]
    for (int i = 0; i < 32; ++i)
    {
        float2 Offset = float2(C * PoissonDisk[i].x - S * PoissonDisk[i].y, S * PoissonDisk[i].x + C * PoissonDisk[i].y);
        
        float2 DeltaUV = Offset * FilterRadiusUV;
        float2 SampleUV = AtlasUV + DeltaUV;
        float TapDepth = ReceiverDepth + ReceiverPlaneOffset(ReceiverBias, DeltaUV);
        Visibility += SampleAtlasCmp(SampleUV, TapDepth, Tile);
    }

    return Visibility / 32.0f;
}

float LinearPerspectiveDepth(float Depth, float NearPlane, float FarPlane)
{
    return NearPlane * FarPlane / (FarPlane - Depth * (FarPlane - NearPlane));
}

float CalculatePCSS(float2 LightUV, float ReceiverDepth, float3 ReceiverBias, uint Slice, float ShadowNear, 
    float ShadowFar, float ShadowTexelWorld, float LightAngularRadiusTan)
{
    float BlockerDepthSum = 0.0f;
    int BlockerCount = 0;
    
    float ShadowDepthRange = ShadowFar - ShadowNear;
    
    float ReceiverDistanceWorld = ReceiverDepth * ShadowDepthRange;
    float SearchRadiusWorld = ReceiverDistanceWorld * LightAngularRadiusTan;
    float SearchRadiusTexels = min(SearchRadiusWorld / max(ShadowTexelWorld, 0.0001f), 12.0f);
    float SearchRadiusUV =  SearchRadiusTexels * ShadowMapTexelSize.x;
    
    static const int BlockerSampleCount = 32;
	[loop]
    for (int i = 0; i < BlockerSampleCount; ++i)
    {
        float2 SampleUV = LightUV + PoissonDisk[i] * SearchRadiusUV;
        if (any(SampleUV < 0.0f) || any(SampleUV > 1.0f))
        {
            continue;
        }
        float SampleDepth = ShadowMaps.SampleLevel(ShadowDepthSampler, float3(SampleUV, Slice), 0.0f);
        float RefDepth = ReceiverDepth + ReceiverPlaneOffset(ReceiverBias, SampleUV - LightUV);
        if (SampleDepth < RefDepth)
        {
            BlockerDepthSum += SampleDepth;
            BlockerCount++;
        }
    }
	
    if (BlockerCount == 0)
    {
        return ShadowMaps.SampleCmpLevelZero(ShadowSampler, float3(LightUV, Slice), ReceiverDepth);
    }
	
    float AverageBlockerDepth = BlockerDepthSum / BlockerCount;
    float BlockerDistanceWorld = AverageBlockerDepth * ShadowDepthRange;
    float BlockerReceiverDistanceWorld = max(ReceiverDistanceWorld - BlockerDistanceWorld, 0.0f);

    // L / 2 = * dB * tan(theta / 2)
    float LightDiameterWorld = BlockerDistanceWorld * LightAngularRadiusTan;

    // P = (L / 2) * (dR - dB) / dB
    float PenumbraWorld = LightDiameterWorld * BlockerReceiverDistanceWorld / max(BlockerDistanceWorld, 0.0001f);
    float PenumbraRadiusTexels = PenumbraWorld / max(ShadowTexelWorld, 0.0001f);
    float FilterRadiusTexels = clamp(PenumbraRadiusTexels, 2.0f, 6.0f);
	
    return CalculatePoissonPCF(LightUV, ReceiverDepth, ReceiverBias, Slice, FilterRadiusTexels * ShadowMapTexelSize.x);
}

float CalculateAtlasPCSS(float2 AtlasUV, float ReceiverDepth, float3 ReceiverBias, FAtlasTile Tile, float LightRadius,
	float ShadowNear, float ShadowFar, float ShadowProjectionScale)
{
    float BlockerDepthSum = 0.0f;
    int BlockerCount = 0;
    
    float ReceiverDistance = LinearPerspectiveDepth(ReceiverDepth, ShadowNear, ShadowFar);
    
    // Near 투영
    float LightRadiusUV = 0.5f * ShadowProjectionScale * LightRadius / ShadowNear;
    float SearchRadiusUV = LightRadiusUV * max(ReceiverDistance - ShadowNear, 0.0f) / max(ReceiverDistance, 0.0001f);
    float TileTexels = Tile.Rect.z / LocalAtlasTexelSize.x;
    float SearchRadiusTexels = min(SearchRadiusUV * TileTexels, 12.0f);
    SearchRadiusUV = SearchRadiusTexels * LocalAtlasTexelSize.x;
	
    static const int BlockerSampleCount = 32;
	[loop]
    for (int i = 0; i < BlockerSampleCount; ++i)
    {
        float2 SampleUV = AtlasUV + PoissonDisk[i] * SearchRadiusUV;
        if (any(SampleUV < Tile.ClampMin) || any(SampleUV > Tile.ClampMax))
        {
            continue;
        }
        float SampleDepth = SampleAtlasDepth(SampleUV, Tile);
        float RefDepth = ReceiverDepth + ReceiverPlaneOffset(ReceiverBias, SampleUV - AtlasUV);
        if (SampleDepth < RefDepth)
        {
            BlockerDepthSum += LinearPerspectiveDepth(SampleDepth, ShadowNear, ShadowFar);
            BlockerCount++;
        }
    }
	
    if (BlockerCount == 0)
    {
        return SampleAtlasCmp(AtlasUV, ReceiverDepth, Tile);
    }
	
    float AverageBlockerDistance = BlockerDepthSum / BlockerCount;
    float PenumbraWorld = LightRadius * max(ReceiverDistance - AverageBlockerDistance, 0.0f) / max(AverageBlockerDistance, 0.0001f);
    float PenumbraUV = 0.5f * ShadowProjectionScale * PenumbraWorld / max(ReceiverDistance, 0.0001f);

	// UV를 texel 반경으로 변환
    float PenumbraRadiusTexels = PenumbraUV * TileTexels;
    float FilterRadiusTexels = clamp(PenumbraRadiusTexels, 2.0f, 4.0f);
	
    return CalculateAtlasPoissonPCF(AtlasUV, ReceiverDepth, ReceiverBias, Tile, FilterRadiusTexels * LocalAtlasTexelSize.x);
}

float SampleShadow(float3 WorldPosition, float3 dPdx, float3 dPdy, matrix LightVP, uint Slice,
	float ShadowNear, float ShadowFar, float ShadowTexelWorld, float LightAngularRadiusTan, float FilterSpacing)
{
    float4 LightClip = mul(float4(WorldPosition, 1.0f), LightVP);
	
    if (LightClip.w > 0.0f)
    {
        float3 LightNDC = LightClip.xyz / LightClip.w;
        float2 LightUV = float2(LightNDC.x * 0.5f + 0.5f, 0.5f - LightNDC.y * 0.5f);
		
        bool IsInsideShadowMap = all(LightUV >= 0.0f) && all(LightUV <= 1.0f) && LightNDC.z >= 0.0f && LightNDC.z <= 1.0f;
        if (!IsInsideShadowMap)
        {
            return 1.0f;
        }
		
        float3 ReceiverBias = float3(ComputeRPDBdZduv(LightClip, LightVP, dPdx, dPdy), ComputeTapConeSlope(LightClip, LightVP));
		
#if SHADOW_FILTER_MODE == SHADOW_FILTER_HARD
            return ShadowMaps.SampleCmpLevelZero(ShadowSampler, float3(LightUV, Slice), LightNDC.z);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_PCF3x3
            return CalculatePCF(LightUV, LightNDC.z, ReceiverBias, Slice, 1, FilterSpacing);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_PCF5x5
            return CalculatePCF(LightUV, LightNDC.z, ReceiverBias, Slice, 2, FilterSpacing);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_POISSON
            return CalculatePoissonPCF(LightUV, LightNDC.z, ReceiverBias, Slice, ShadowMapTexelSize.x * 2.0f);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_PCSS
        return CalculatePCSS(LightUV, LightNDC.z, ReceiverBias, Slice, ShadowNear, ShadowFar, ShadowTexelWorld, LightAngularRadiusTan);
#endif
    }
    return 1.0f;
}

float SampleLocalShadow(float3 WorldPosition, float3 dPdx, float3 dPdy, matrix LightVP, float4 AtlasRect, float LightRadius,
	float ShadowNear, float ShadowFar, float ShadowProjectionScale, float FilterSpacing = 1.0f)
{
	// 타일 크기가 0
	if (AtlasRect.z == 0)
        return 1.0f;
	
    float4 LightClip = mul(float4(WorldPosition, 1.0f), LightVP);
	
    if (LightClip.w > 0.0f)
    {
        float3 LightNDC = LightClip.xyz / LightClip.w;
        float2 LocalUV = float2(LightNDC.x * 0.5f + 0.5f, 0.5f - LightNDC.y * 0.5f);
		
        bool IsInsideLocalAtlas = all(LocalUV >= 0.0f) && all(LocalUV <= 1.0f) && LightNDC.z >= 0.0f && LightNDC.z <= 1.0f;
        if (!IsInsideLocalAtlas)
        {
            return 1.0f;
        }
        
        FAtlasTile Tile = MakeAtlasTile(AtlasRect);
        float2 AtlasUV = ToAtlasUV(LocalUV, Tile);
        
        float3 ReceiverBias = float3(ComputeRPDBdZduv(LightClip, LightVP, dPdx, dPdy), ComputeTapConeSlope(LightClip, LightVP));
        ReceiverBias.xy /= Tile.Rect.zw;
        ReceiverBias.z /= Tile.Rect.z;
		
#if SHADOW_FILTER_MODE == SHADOW_FILTER_HARD
        return SampleAtlasCmp(AtlasUV, LightNDC.z, Tile);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_PCF3x3
        return CalculateAtlasPCF(AtlasUV, LightNDC.z, ReceiverBias, Tile, 1, FilterSpacing);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_PCF5x5
        return CalculateAtlasPCF(AtlasUV, LightNDC.z, ReceiverBias, Tile, 2, FilterSpacing);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_POISSON
        return CalculateAtlasPoissonPCF(AtlasUV, LightNDC.z, ReceiverBias, Tile, LocalAtlasTexelSize.x * 2.0f);
#elif SHADOW_FILTER_MODE == SHADOW_FILTER_PCSS
        return CalculateAtlasPCSS(AtlasUV, LightNDC.z, ReceiverBias, Tile, LightRadius, ShadowNear, ShadowFar, ShadowProjectionScale);
#endif
    }
    return 1.0f;
}

float GetShadowMask(
	float3 Position, float3 dPdx, float3 dPdy,
	int Face,
	in DirectionalLightInfo Light
)
{
	// 인접 PCF 샘플 사이에 유지할 월드 거리
    float FilterStepWorld = Light.ShadowTexelWorld[0];

	// 현재 캐스케이드의 한 텍셀이 차지하는 월드 거리
    float TexelWorld = Light.ShadowTexelWorld[Face];

	// CalculatePCF의 마지막 인수로 전달할 값
    float FilterSpacing = FilterStepWorld / Light.ShadowTexelWorld[Face];
	
    return SampleShadow(
		Position,
        dPdx,
        dPdy,
		Light.ViewProjection[Face],
		Light.ShadowMapIndex[Face],
		Light.ShadowNear[Face],
		Light.ShadowFar[Face],
        Light.ShadowTexelWorld[Face],
        Light.ShadowAngularRadiusTan,
		FilterSpacing
	);
}

uint GetCSMFace(in DirectionalLightInfo Light, float Value)
{
    uint N = Light.CascadeCount;
    uint E = Light.CascadeExponential;

    int Index;

    if (E == 1)
    {
        Index = (int) floor(Value * N);
    }
    else
    {
        float CascadeScale = pow(E, N) - 1.0f;
        float InvLogCascadeExponent = 1.0f / log2(E);

        float rawIdx = log2(1.0 + Value * CascadeScale) * InvLogCascadeExponent;

        Index = (int) floor(rawIdx);
    }

    return clamp(Index, 0, N - 1);
}

float GetSumOfGeometricSeries(int Index, int Exponential)
{
    if (Exponential == 1)
    {
        return Index;
    }
	
    int Upper = 1 - pow(Exponential, Index);
    float Lower = 1 - Exponential;
	
    return Upper / Lower;
}

float GetDirectionalShadowMask(inout float3 Diffuse, float3 Position, float3 dPdx, float3 dPdy, in DirectionalLightInfo Light, float3 OriginalNormal)
{
    float ShadowMask = 1.0f;
	
    if (ReceiveShadow == 0)
    {
        return 1.0f;
    }
	
    float3 LightDir = -normalize(Light.Direction);
    float OriginalNdotL = dot(OriginalNormal, LightDir);
	
	// Pixel의 좌표에서 View Depth를 추출하여 Near ~ Far 사이의 값을 0 ~ 1 구간으로 만듦
    float4 ViewPosition = mul(float4(Position, 1.0f), ViewMatrix);
    float ViewDepth = ViewPosition.x;
    float ViewDepthNormalized = (ViewDepth - Near) / (Far - Near);
	
	// 어떤 Shadow Map을 사용할지 결정
    uint Face = GetCSMFace(Light, ViewDepthNormalized);
	
    float StartPoint = 0.0f;
    float EndPoint = 0.0f;
	
	if (Face > 0)
    {
        StartPoint = Near + (Far - Near) * Light.ShadowSplitDepths[Face - 1];
        EndPoint = StartPoint + Light.CascadeBlendRange;
    }
	
    float SinTheta = sqrt(saturate(1.0f - OriginalNdotL * OriginalNdotL));
    float3 Offset = OriginalNormal * Light.ShadowTexelWorld[Face] * SinTheta * 0.5f;

    ShadowMask = GetShadowMask(Position + Offset, dPdx, dPdy, Face, Light);
	
    if (Face > 0 && ViewDepth <= EndPoint)
    {
        Offset = OriginalNormal * Light.ShadowTexelWorld[Face - 1] * SinTheta * 0.5f;
        float PreviousShadowMask = GetShadowMask(Position + Offset, dPdx, dPdy, Face - 1, Light);
		
		// 디버그 UI
        Diffuse += float3(0.0f, 0.05f, 0.0f) * Face * Light.bCascadeDebug;
        
		// 두개의 ShadowMask를 t로 보간
        float t = saturate((ViewDepth - StartPoint) / Light.CascadeBlendRange);
        ShadowMask = lerp(PreviousShadowMask, ShadowMask, t);
    }
	else
    {
		// 디버그 UI
        Diffuse += float3(0.05f, 0.0f, 0.05f) * Face * Light.bCascadeDebug;
    }
	
    return ShadowMask;
}

float3 CalculateColor(float3 Albedo, float3 SpecularColor, float3 Normal, float3 Position, 
    float3 View, float2 ScreenPosition, float3 OriginalNormal)
{
#if LIGHTING_ENABLED == 0
	return Albedo;
#else
    float3 Diffuse = float3(0.0f, 0.0f, 0.0f);
    float3 Specular = float3(0.0f, 0.0f, 0.0f);
	
    float3 dPdx = ddx(Position);
    float3 dPdy = ddy(Position);
	
    for (uint i = 0; i < NumDirectionalLights; ++i)
    {
        DirectionalLightInfo Light = DirectionalLights[i];
		
        float3 LightDir = -normalize(Light.Direction);
		
        float OriginalNdotL = dot(OriginalNormal, LightDir);
        float Mask = smoothstep(0.0f, 0.25f, OriginalNdotL);
        if (Mask <= 0.0f)
            continue;
		
        float ShadowMask = 1.0f;
#if SHADOW_ENABLED == 1
        ShadowMask = GetDirectionalShadowMask(Diffuse, Position, dPdx, dPdy, Light, OriginalNormal);
#endif
		
		float NdotL = saturate(dot(Normal, LightDir));
		Diffuse += NdotL * Light.Color * Light.Intensity * CalculateDiffuse() * Mask * ShadowMask;
		Specular += NdotL * Light.Color * Light.Intensity * CalculateSpecular(Normal, LightDir, View, Shininess, 1.0f) * Mask * ShadowMask;
	}
	
    uint LightCount = NumPointLights + NumSpotLights;
    uint ListOffset = 0;
#if LIGHTING_ENABLED == 1
	// CPU가 전달한 화면 타일 크기. SV_POSITION은 뷰포트 내부 픽셀 좌표다.
    if (TileCountX > 0)
    {
        uint2 Tile = uint2(ScreenPosition) / TileSize;
        uint TileIndex = Tile.y * TileCountX + Tile.x;
        LightCount = min(TileLightCount[TileIndex], MaxLightsPerTile);
        ListOffset = TileIndex * MaxLightsPerTile;
    }
#endif
	for (uint j = 0; j < LightCount; ++j)
	{
		uint LightIndex = j;
#if LIGHTING_ENABLED == 1
		if (TileCountX > 0)
			LightIndex = TileLightIndices[ListOffset + j];
#endif
		// 컬링 입력은 Point 배열 다음 Spot 배열 순서다.
        if (LightIndex < NumPointLights)
        {
            PointLightInfo Light = PointLights[LightIndex];
			
            float3 ToLight = Light.Location - Position;
            float DistanceSquared = dot(ToLight, ToLight);
            float Distance = sqrt(DistanceSquared);
            float3 LightDir = ToLight / max(Distance, 0.0001f);
			
            float OriginalNdotL = dot(OriginalNormal, LightDir);
            float Mask = smoothstep(0.0f, 0.25f, OriginalNdotL);
            if (Mask <= 0.0f)
                continue;
			
            float Attenuation = CalculateDistanceAttenuation(DistanceSquared, Light.AttenuationRadiusSquaredInversed);
            if (Attenuation <= 0.0f)
                continue;
			
            float ShadowMask = 1.0f;
			
#if SHADOW_ENABLED == 1
            if (ReceiveShadow != 0)
            {
                float3 ShadowToLight = Light.Location - Position;
                float3 A = abs(ShadowToLight);
                uint Face = (A.x >= A.y && A.x >= A.z) ? (ShadowToLight.x > 0 ? 0 : 1)
					: (A.y >= A.z) ? (ShadowToLight.y > 0 ? 2 : 3)
					: (ShadowToLight.z > 0 ? 4 : 5);
				
                float TexelSize = LocalAtlasTexelSize.x / max(Light.ShadowAtlasRect[Face].z, 0.0001f);
                float3 OffsetPosition = Position + ShadowNormalOffset(Position, OriginalNormal, OriginalNdotL, Light.ViewProjection[Face], TexelSize);

                ShadowToLight = Light.Location - OffsetPosition;
                A = abs(ShadowToLight);
                Face = (A.x >= A.y && A.x >= A.z) ? (ShadowToLight.x > 0 ? 0 : 1)
					: (A.y >= A.z) ? (ShadowToLight.y > 0 ? 2 : 3)
					: (ShadowToLight.z > 0 ? 4 : 5);

                ShadowMask = SampleLocalShadow(OffsetPosition, dPdx, dPdy, Light.ViewProjection[Face], Light.ShadowAtlasRect[Face], Light.ShadowSourceRadius,
					Light.ShadowNear, Light.ShadowFar, Light.ShadowProjectionScale);
            }
#endif
            float NdotL = saturate(dot(Normal, LightDir));
            Diffuse += NdotL * Light.Color * Light.Intensity * Attenuation * CalculateDiffuse() * Mask * ShadowMask;
            Specular += NdotL * Light.Color * Light.Intensity * Attenuation * CalculateSpecular(Normal, LightDir, View, Shininess, 1.0f) * Mask * ShadowMask;
        }
        else
        {
            SpotLightInfo Light = SpotLights[LightIndex - NumPointLights];
			
            float3 ToLight = Light.Location - Position;
            float DistanceSquared = dot(ToLight, ToLight);
            float Distance = sqrt(DistanceSquared);
            float3 LightDir = ToLight / max(Distance, 0.0001f);
			
            float OriginalNdotL = dot(OriginalNormal, LightDir);
            float Mask = smoothstep(0.0f, 0.25f, OriginalNdotL);
            if (Mask <= 0.0f)
                continue;
			
            float DistanceAttenuation = CalculateDistanceAttenuation(DistanceSquared, Light.AttenuationRadiusSquaredInversed);
            float Cos = dot(-LightDir, normalize(Light.Direction));
            float AngleAttenuation = CalculateSpotAngleAttenuation(Cos, Light.CosInner, Light.CosOuter);
            float Attenuation = DistanceAttenuation * AngleAttenuation;
            if (Attenuation <= 0.0f)
                continue;
			
            float ShadowMask = 1.0f;
#if SHADOW_ENABLED == 1
            if (ReceiveShadow != 0)
            {
                float TexelSize = LocalAtlasTexelSize.x / max(Light.ShadowAtlasRect.z, 0.0001f);
                float3 Offset = ShadowNormalOffset(Position, OriginalNormal, OriginalNdotL, Light.ViewProjection, TexelSize);
				
                ShadowMask = SampleLocalShadow(Position + Offset, dPdx, dPdy, Light.ViewProjection, Light.ShadowAtlasRect, Light.ShadowSourceRadius,
					Light.ShadowNear, Light.ShadowFar, Light.ShadowProjectionScale);
            }
#endif
            float NdotL = saturate(dot(Normal, LightDir));
            Diffuse += NdotL * Light.Color * Light.Intensity * DistanceAttenuation * AngleAttenuation * CalculateDiffuse() * Mask * ShadowMask;
            Specular += NdotL * Light.Color * Light.Intensity * DistanceAttenuation * AngleAttenuation * CalculateSpecular(Normal, LightDir, View, Shininess, 1.0f) * Mask * ShadowMask;
        }
    }
	
    float3 FinalColor = Albedo * (AmbientLight.Color * AmbientLight.Intensity + Diffuse) + SpecularColor * Specular;

    return FinalColor;
#endif
}

float3 CalculateNormalMapping(float3 SampledNormal, float3 Normal, float4 Tangent)
{
    float3 DecodedNormal = SampledNormal * 2.0f - 1.0f;
    float3 N = normalize(Normal);
    float3 T = normalize(Tangent.xyz - N * dot(Tangent.xyz, N));
    float3 B = cross(N, T) * Tangent.w;
    float3x3 TBN = float3x3(T, B, N);
	
    return normalize(mul(DecodedNormal, TBN));
}

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
	
    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    Output.Position = mul(float4(Output.WorldPosition, 1.0f), VP);
    Output.Normal = normalize(mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz);
    Output.Color = Input.Color;
    Output.UV = Input.UV;
    Output.Tangent = float4(mul(Input.Tangent.xyz, (float3x3) World), Input.Tangent.w);
	
    return Output;
}

// 색 텍스처는 sRGB로 저장돼 있어서 조명 계산 전에 선형으로 바꾼다. (GammaCorrectionShader의 역함수)
// 노멀맵 같은 데이터 텍스처에는 쓰면 안 된다.
float3 SRGBToLinear(float3 Color)
{
    Color = saturate(Color);
    float3 Low = Color / 12.92f;
    float3 High = pow((Color + 0.055f) / 1.055f, 2.4f);
    return lerp(High, Low, step(Color, 0.04045f));
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 TexColor = Texture.Sample(Sampler, Input.UV + UVOffset);
    TexColor.rgb = SRGBToLinear(TexColor.rgb);
    float4 Albedo = TexColor * BaseColor;
	
    float3 NormalDir = normalize(Input.Normal);
    float3 ViewDir = normalize(CameraPosition - Input.WorldPosition);
	
    float3 SampledNormalMap = NormalMap.Sample(Sampler, Input.UV + UVOffset).rgb;
    if (bUseNormalMap != 0)
    {
		NormalDir = CalculateNormalMapping(SampledNormalMap, Input.Normal, Input.Tangent);
    }
	
	return float4(CalculateColor(Albedo.rgb, SpecularColor, NormalDir, 
		Input.WorldPosition, ViewDir, Input.Position.xy, NormalDir), Albedo.a);
}
