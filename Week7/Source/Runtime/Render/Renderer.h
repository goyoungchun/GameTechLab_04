#pragma once

#include "RenderPacket.h"
#include "Texture2D.h"
#include "Text/Font.h"

#include "RenderingInfo.h"
#include "Occlusion/GPUOcclusion.h"

constexpr uint32 ObjectSlotBytes = 256;
constexpr uint32 MaxDirectLights = 2;

struct FPerObjectConstants
{
	FMatrix World;
};

struct FSortEntry
{
	uint64 Key;
	uint32 PacketIndex;
};

class UCameraComponent;
class FShaderProgram;
class FComputeShader;

// 뷰 모드에 따라 BindMaterial이 머티리얼 셰이더를 갈아 끼운다.
// 에디터의 EMultipleViewPortsViewMode와 값 순서를 맞춘다.
enum class ERenderViewMode : uint8
{
	Unlit,          
	Lit_Gouraud,  
	Lit_Lambert,   
	Lit_Phong,    
	World_Normal,
	Count
};

//Phong 스페큘러 등 뷰 방향이 필요한 셰이더가 쓴다. 뷰마다 다르다.
struct alignas(16) FCameraConstants
{
	FVector CameraPosition;   // 월드 공간 카메라 위치
	float Padding = 0.0f;
};
static_assert(sizeof(FCameraConstants) == 16);

// 오클루전 컬링의 효과 상한을 재기 위한 측정 결과 (디버그 전용)
struct FOcclusionMeasureResult
{
	bool bValid = false;
	uint32 TotalObjects = 0;      // 불투명 패스에 제출된 물체 수
	uint32 VisibleObjects = 0;    // 최종 깊이 버퍼에 픽셀을 1개 이상 남긴 물체 수
	uint32 TotalDraws = 0;
	uint32 VisibleDraws = 0;
	uint64 TotalTriangles = 0;
	uint64 VisibleTriangles = 0;
	double ElapsedMs = 0.0;       // 측정 자체에 걸린 시간 (GPU 대기 포함)
	// GPU 오클루전 검증. 판정만 하고 거르지 않은(Cull 끔) 상태에서 측정해야 의미가 있다.
	uint32 GPUOccludedDraws = 0;  // GPU가 가렸다고 판정한 드로우 수
	uint32 FalseCulls = 0;        // 그중 실제로는 픽셀이 보인 드로우 수. 0이어야 한다
};

// 아래 라이트 구조체는 UberLit.hlsl의 같은 이름 구조체와 바이트 단위로 같아야 한다.
struct FAmbientLightInfo
{
	FVector4 ColorIntensity; 
};

struct FDirectionalLightInfo
{
	FVector4 ColorIntensity; 
	FVector4 Direction;   
};

// LightBuffer (b4)
struct alignas(16) FLightConstants
{
	FAmbientLightInfo Ambient;
	FDirectionalLightInfo Directional;
	uint32 PointLightCount = 0;
	uint32 SpotLightCount = 0;
	uint32 Padding[2] = {};
};
static_assert(sizeof(FLightConstants) == 64);

//StructuredBuffer(t1)
struct FPointLightInfo
{
	FVector4 ColorIntensity; // rgb: 색상, w: 강도
	FVector Position;
	float AttenuationRadius;
	float Falloff;
	float Padding[3];
};
static_assert(sizeof(FPointLightInfo) == 48);

// StructuredBuffer (t2)
struct FSpotLightInfo
{
	FVector4 ColorIntensity; 
	FVector Position;
	float AttenuationRadius;
	FVector Direction;      
	float Falloff;
	float CosInner;         
	float CosOuter;         
	float Padding[2];
};
static_assert(sizeof(FSpotLightInfo) == 64);

// 라이트 수만큼 커지는 동적 구조화 버퍼와 그 SRV
struct FLightStructuredBuffer
{
	ComPtr<ID3D11Buffer> Buffer;
	ComPtr<ID3D11ShaderResourceView> SRV;
	uint32 Capacity = 0;
};

struct alignas(16) FDirectionalLight
{
	FVector4 Direction;
	FVector4 ColorIntensity;
	FVector4 EXP; 
};

struct alignas(16) FDirectionalLightConstants
{
	FDirectionalLight Light[MaxDirectLights];
	uint32 LightCount;
	uint32 Padding[3] = {};
};
struct alignas(16) FTileCullingConstants
{
	FMatrix View;              // 라이트 중심을 월드 → 뷰 공간으로 옮긴다
	float  ProjXScale;         // Projection.M[1][0]  타일 좌우 면
	float  ProjYScale;         // Projection.M[2][1]  타일 위아래 면
	float  ProjZScale;         // Projection.M[0][2]  깊이 버퍼 값 → 뷰 깊이
	float  ProjZOffset;        // Projection.M[3][2]  〃
	uint32 ScreenWidth;
	uint32 ScreenHeight;
	uint32 TileCountX;
	uint32 TileCountY;
	uint32 PointLightCount;
	uint32 SpotLightCount;
	uint32 Padding[2] = {};
};
class FRenderer
{
public:
	bool Init();

	// 기존 단일 카메라의 ViewProjection으로 렌더 큐 전체를 그린다.
	void RenderAll(FRenderQueue& InQueue, UCameraComponent* CameraComponent);

	// Adapter가 계산한 ViewProjection을 직접 받아 View별 렌더 큐를 그린다.
	void RenderAll(FRenderQueue& InQueue, const FMatrix& ViewProjection);

	// 큐를 정렬해 불투명 패킷만 그린다. 반투명은 RenderTranslucent 호출 전까지 보관한다.
	void RenderOpaque(const FMatrix& ViewProjection);

	// Gather 전에 World가 불러 가려진 물체를 거른다.
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 캐시된 스태틱 메시 경로: World::Gather가 조각별로 채운 묶음을 이번 프레임 불투명 패스에 넘긴다.
	// 묶음 메모리는 World 소유이며 다음 Gather 전까지 유효하다. RenderQueueSorting이 묶음을 정렬하고 RenderOpaque가 그린다.
	void ResetStaticDrawGroups() { StaticGroups.clear(); }
	void AddStaticDrawGroup(const FStaticDrawGroup* Group) { StaticGroups.push_back(Group); }

	// RenderOpaque가 보관한 반투명 패킷을 먼 것부터 그린다.
	void RenderTranslucent(const FMatrix& ViewProjection);

	// 큐를 Material, Opaque, Mesh에 따라 정렬한다
	void RenderQueueSorting(FRenderQueue& InQueue, const FMatrix& ViewProjection);

	// Depth만 그림
	void RenderDepthOnly(const FMatrix& ViewProjection);
	//ComputeShader Culling용
	void LightCulling(const FMatrix& View, const FMatrix& Projection, uint32 Width, uint32 Height, FTexture2D* DepthTexture);
	void DisPatch(uint32 TileCountX, uint32 TileCountY);
	void ResetLightCulling() { bTileCullingValid = false; }
	void RenderCullingView(const FRenderingInfo& RenderingInfo);

	// [측정 전용] RenderOpaque 직후에 호출한다. 불투명 패킷을 깊이 LESS_EQUAL·색 쓰기 없이 다시 그리며
	// 패킷마다 오클루전 쿼리를 걸어, 최종 화면에 실제로 픽셀을 남긴 물체 수를 센다.
	// GPU 결과를 기다리므로 매우 느리다. 버튼 등으로 한 프레임만 실행할 것.
	FOcclusionMeasureResult MeasureOpaqueOcclusion(const FMatrix& ViewProjection);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

	// 이후 그리는 뷰의 셰이딩 모드. 뷰마다 RenderFrame 시작에서 설정한다.
	void SetViewMode(ERenderViewMode InViewMode) { ViewMode = InViewMode; }

	// 이후 그리는 뷰의 카메라 위치를 b5에 올린다. 뷰마다 RenderFrame 시작에서 설정한다.
	void SetCameraPosition(const FVector& InCameraPosition);

	// FScene::UpdateLights가 모은 라이트를 b4 상수버퍼와 t1/t2 구조화 버퍼에 올린다.
	// 버퍼는 뷰끼리 공유하므로, 직전에 올린 것과 같은 출처(씬)·같은 버전이면 업로드를 건너뛴다.
	void SetLights(const void* Source, uint32 Version, const FLightConstants& Constants, const TArray<FPointLightInfo>& PointLights, const TArray<FSpotLightInfo>& SpotLights);
	void SetDirectionalLight(const FDirectionalLightConstants& LightData);

private:
	// FIFO 소비용 배열의 용량만 재사용하며 매 View의 패킷 값은 새로 채운다.
	FRenderQueue RenderPackets;
	// 정렬된 RenderPackets에서 반투명 패킷이 시작되는 위치
	uint32 FirstTranslucentIndex = 0;
	uint32 FirstMaterialIndex = 0;
	TUniquePtr<FConstantBuffer> PerObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;

	// 모든 패킷의 World 행렬을 256바이트 칸에 한 번에 올린 버퍼. D3D11.1 오프셋 바인딩을 못 쓰면 PerObjectCB로 돌아간다.
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	bool bObjectConstantsPrepared = false;
	UMaterial* LastMaterial;
	UStaticMesh* LastMesh;

	// MeasureOpaqueOcclusion 전용. 처음 측정할 때 만들고 이후 재사용한다.
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;

	FGPUOcclusion GPUOcclusion;

	void DrawPackets(uint32 Begin, uint32 End, const FMatrix& ViewProjection,bool bDepthOnly=false);
	void DrawStaticGroups(bool bDepthOnly = false);
	void UpdatePerObjectConstants(const FMatrix& World);

	// 이번 프레임 스태틱 메시 묶음 (정렬 키 순). 비어 있지 않은 것만 담는다.
	std::vector<const FStaticDrawGroup*> StaticGroups;
	void BindMaterial(UMaterial* material, bool bDepthOnly = false);
	void UpdateMaterialParams(const FRenderPacket& RenderPacket);
	void UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection);
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);
	void UploadPerObjectConstants();

	TArray<FSortEntry> SortEntries;

	TUniquePtr<FConstantBuffer> DirectionalLightCB;
	FDirectionalLightConstants DirectionalLightData{};

	TUniquePtr<FConstantBuffer> LightCB;
	// 마지막으로 라이트 버퍼에 올린 데이터의 출처와 버전 (SetLights 업로드 생략 판단용)
	const void* LastLightSource = nullptr;
	uint32 LastLightVersion = 0;
	FLightStructuredBuffer PointLightBuffer;
	FLightStructuredBuffer SpotLightBuffer;

	// 필요한 개수가 용량을 넘을 때만 두 배씩 키워 다시 만든다.
	bool EnsureLightBufferCapacity(FLightStructuredBuffer& Target, uint32 Count, uint32 Stride);
	void UploadLightBuffer(FLightStructuredBuffer& Target, const void* Data, uint32 Count, uint32 Stride);
	// 불투명/반투명 패스 시작 시 VS/PS에 b4(라이트), b5(카메라), t1/t2(Point/Spot)를 묶는다.
	void BindLights(bool bUseTileCulling = false);

	// 라이트 버퍼(SetLights)와 달리 뷰마다 값이 달라 캐시 없이 매번 올린다.
	TUniquePtr<FConstantBuffer> CameraCB;
	TUniquePtr<FConstantBuffer> TileCullingCB;
	TUniquePtr<FConstantBuffer> TileLightInfoCB;
	TUniquePtr<FConstantBuffer> CullingViewCB;
	FShaderProgram* CullingViewShader = nullptr;
	bool bTileCullingValid = false;
	uint32 CurrentTileCountX = 0;

	ERenderViewMode ViewMode = ERenderViewMode::Lit_Gouraud;

	// 뷰 모드별로 갈아 끼울 셰이더. Renderer::Init은 셰이더 로드(FRenderResourceManager::Init)보다
	// 먼저 불리므로 처음 필요할 때 맵에서 찾아(필요하면 변형을 컴파일해) 둔다.
	void ResolveViewModeShaders();
	bool bViewModeShadersResolved = false;
	FShaderProgram* StaticMeshShader = nullptr;
	// 인덱스 = ERenderViewMode. Unlit 칸은 비워 두고 머티리얼 셰이더(StaticMeshShader)를 그대로 쓴다.
	FShaderProgram* ViewModeShaders[static_cast<uint32>(ERenderViewMode::Count)] = {};

	//LightCulling에 필요한 Tile수
	static constexpr uint32 TileSize = 16;
	static constexpr uint32 MaxLightsPerTile = 1024;

	// 셰이더 객체는 FRenderResourceManager가 소유한다.
	FComputeShader* TileCullingShader = nullptr;

	ComPtr<ID3D11Buffer> TileLightBuffer;
	ComPtr<ID3D11UnorderedAccessView> TileLightUAV;
	ComPtr<ID3D11ShaderResourceView> TileLightSRV;
	uint32 UploadedPointLightCount = 0;
	uint32 UploadedSpotLightCount = 0;

	// 타일 수가 아니라, 버퍼에 들어가는 uint 칸의 개수
	uint32 TileLightCapacity = 0;

	bool EnsureTileLightBufferCapacity(uint32 Width, uint32 Height);
};
