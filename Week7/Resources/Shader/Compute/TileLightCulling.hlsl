#pragma pack_matrix(row_major)

// Renderer.h의 값과 같아야 한다.
#define TILE_SIZE           16
#define MAX_LIGHTS_PER_TILE  1024

// C++ FTileCullingConstants와 필드 순서 및 크기를 맞춘다.
cbuffer TileCullingConstants : register(b0)
{
    float4x4 View;

    float ProjXScale;
    float ProjYScale;
    float ProjZScale;
    float ProjZOffset;

    uint ScreenWidth;
    uint ScreenHeight;
    uint TileCountX;
    uint TileCountY;

    uint PointLightCount;
    uint SpotLightCount;
    uint2 CullingPadding;
};
/*
    UE공간에서 Projection 
    Xscale  0       0        0
    0       YScale  0        0
    0       0       ZScale   1
                    ZOffset  0
*/
// C++ FPointLightInfo: 48바이트
struct PointLightInfo
{
    float4 ColorIntensity;

    float3 Position;
    float AttenuationRadius;

    float Falloff;
    float3 Padding;
};

// C++ FSpotLightInfo: 64바이트
struct SpotLightInfo
{
    float4 ColorIntensity;

    float3 Position;
    float AttenuationRadius;

    float3 Direction;
    float Falloff;

    float CosInner;
    float CosOuter;
    float2 Padding;
};

// ⑤ 입력
Texture2D<float> DepthTexture : register(t0);
StructuredBuffer<PointLightInfo> PointLights : register(t1);
StructuredBuffer<SpotLightInfo> SpotLights : register(t2);

// ⑥ 출력
// 타일마다 [count][라이트 번호 ...]를 저장할 uint 배열
RWStructuredBuffer<uint> TileLightList : register(u0);


bool IsLightinTile(float4 Planes[6], float3 LightPosition,float Radius)
{
    for (uint PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
    {
        // 라이트 중심과 평면 사이의 부호 있는 거리
        float Distance = dot(Planes[PlaneIndex].xyz, LightPosition) + Planes[PlaneIndex].w;

        // 한 평면이라도 구 전체가 바깥이면 제외한다.
        // 법선이 상자안쪽을 가리키는 방향이므로 Distance에 -를 붙인다...
        if (-Distance > Radius)
            return false;
    }

    return true;
}

// 그룹 하나 = 타일 하나
// 그룹 내부 스레드 하나 = 타일의 픽셀 하나

//SM내부에서 threads들이 공유하는 L1 cache 공유메모리
groupshared uint minDepth;
groupshared uint maxDepth;
groupshared uint TileLightCount;
groupshared uint TileLights[MAX_LIGHTS_PER_TILE];
// numthreads = 하나의 threadGroup에 몇개의 thread가 있냐
[numthreads(TILE_SIZE, TILE_SIZE, 1)]

void mainCS(
    uint3 GroupID : SV_GroupID, // GroupdId.xy = 타일좌표
    uint3 GroupThreadID : SV_GroupThreadID, // GroupdThreadID.xy = 타일안의 픽셀좌표
    uint3 DispatchThreadID : SV_DispatchThreadID, // DispatchThreadID.xy = 화면전체기준 픽셀좌표 = GroupID.xy * 16 + GroupThreadID.xy
    uint GroupIndex : SV_GroupIndex) // 타일내부 스레드의 일련번호
{
    // 이후 구현:
    // 1. 타일의 깊이 범위 계산

    // 1-1. min,max 초기화
    if (GroupIndex == 0)
    {
        minDepth = 0x7F7FFFFF; //uint max
        maxDepth = 0; // uint min
        TileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync(); // thread sync 맞추기. 초기값이 모두에게 보일 때까지 대기
    
    // 1-2. min,max update
    if (DispatchThreadID.x < ScreenWidth && DispatchThreadID.y < ScreenHeight)
    {
    
        float myDepth = DepthTexture.Load(int3(DispatchThreadID.xy, 0));
        if (myDepth < 1.0f)
        {
            uint DepthBits = asuint(myDepth);
        
            InterlockedMin(minDepth, DepthBits);
            InterlockedMax(maxDepth, DepthBits);
        }
    }
        
    GroupMemoryBarrierWithGroupSync(); 
    // 배경만 있는 타일은 앞뒤 면을 만들지 않고 이번 프레임의 빈 목록을 기록한다.
    if (minDepth == 0x7F7FFFFFu)
    {
        if (GroupIndex == 0)
            TileLightList[(GroupID.y * TileCountX + GroupID.x) * (MAX_LIGHTS_PER_TILE + 1)] = 0;
        return;
    }
    float MinDepth = asfloat(minDepth);
    float MaxDepth = asfloat(maxDepth);
    
    // 2. 타일 영역과 라이트의 교차 판정
    // 2-1. 픽셀공간 -> NDC좌표
    float2 TileMinPx = float2(GroupID.xy * TILE_SIZE);
    float2 TileMaxPx = min(TileMinPx + TILE_SIZE, float2(ScreenWidth, ScreenHeight));
    float NdcLeft = TileMinPx.x / ScreenWidth * 2.0f - 1.0f;
    float NdcRight = TileMaxPx.x / ScreenWidth * 2.0f - 1.0f;
    float NdcTop = 1.0f - TileMinPx.y / ScreenHeight * 2.0f;
    float NdcBottom = 1.0f - TileMaxPx.y / ScreenHeight * 2.0f;

    // Planes[i].xyz = 법선, Planes[i].w = 원점으로부터의 거리
    float4 Planes[6];
    Planes[0] = float4(normalize(float3(-NdcLeft / ProjXScale, 1.0f, 0.0f)), 0.0f); // 왼쪽
    Planes[1] = float4(normalize(float3(NdcRight / ProjXScale, -1.0f, 0.0f)), 0.0f); // 오른쪽
    Planes[2] = float4(normalize(float3(-NdcBottom / ProjYScale, 0.0f, 1.0f)), 0.0f); // 아래
    Planes[3] = float4(normalize(float3(NdcTop / ProjYScale, 0.0f, -1.0f)), 0.0f); // 위

    // 깊이 → 앞뒤 면
    float NearX = ProjZOffset / (MinDepth - ProjZScale);
    float FarX = ProjZOffset / (MaxDepth - ProjZScale);
    Planes[4] = float4(1.0f, 0.0f, 0.0f, -NearX); // 앞면
    Planes[5] = float4(-1.0f, 0.0f, 0.0f, FarX); // 뒷면
    
    // 2-2. 빛에 대해서 직육면체와 구가 겹치는지 검사
    uint TotalLightCount = PointLightCount + SpotLightCount;
    uint TileSizeSquare = TILE_SIZE * TILE_SIZE;

    for (uint LightIndex = GroupIndex; LightIndex < TotalLightCount; LightIndex += TileSizeSquare)
    {
        float3 LightWorldPos;
        float Radius;

        if (LightIndex < PointLightCount)
        {
            PointLightInfo L = PointLights[LightIndex];
            LightWorldPos = L.Position;
            Radius = L.AttenuationRadius;
        }
        else
        {
            SpotLightInfo L = SpotLights[LightIndex - PointLightCount];
            LightWorldPos = L.Position;
            Radius = L.AttenuationRadius;
        }

        float3 LightPosition = mul(float4(LightWorldPos, 1.0f), View).xyz;
        if (IsLightinTile(Planes, LightPosition, Radius))
        {
            uint Slot;
            InterlockedAdd(TileLightCount, 1, Slot);
            if (Slot < MAX_LIGHTS_PER_TILE)
                TileLights[Slot] = LightIndex; // 통합 번호를 그대로 기록
        }
    }
    GroupMemoryBarrierWithGroupSync(); // 모든 스레드가 자기 몫의 라이트 검사를 끝낼 때까지 대기

    
    // 3. TileLightList에 개수와 라이트 번호 기록
    uint TileIndex = GroupID.y * TileCountX + GroupID.x; // 이 타일의 번호
    uint TileBase = TileIndex * (MAX_LIGHTS_PER_TILE + 1); // 버퍼에서 이 타일 구역의 시작 칸
    uint Count = min(TileLightCount, MAX_LIGHTS_PER_TILE); // 넘친 경우를 잘라낸다

    if (GroupIndex == 0)
        TileLightList[TileBase] = Count; // 첫 칸: 개수

    for (uint i = GroupIndex; i < Count; i += TILE_SIZE * TILE_SIZE)
        TileLightList[TileBase + 1 + i] = TileLights[i]; // 나머지 칸: 번호 (256개 스레드가 나눠서 복사)
}
