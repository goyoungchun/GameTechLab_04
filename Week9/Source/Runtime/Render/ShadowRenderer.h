#pragma once

#include "Texture/Texture2D.h"
#include "Texture/Texture2DArray.h"
#include "ShadowAtlasAllocator.h"
#include "Component/LightComponent.h"
#include "Engine/LightSceneProxy.h"
#include "Render/RenderPacket.h"

class FRenderer;
class FScene;
class FLightSceneProxy;
struct FVector;
struct FVector4;
struct FSceneView;
struct FViewContext;
struct FShadowView;
struct FDirectionalLightRenderData;

struct FShadowCacheEntry
{
	uint32 Size = 0;
	uint32 ViewCount = 0;
	FShadowAtlasTile Tiles[6];
	FShadowViewCapture Captures[6];
	uint64 LastSeenFrame = 0;
	bool bTilePlacementChanged = false;
};

struct FShadowStats
{
	uint32 DirectionalLights;
	uint32 PointLights;
	uint32 SpotLights;
	uint32 RenderedFaces;
	uint32 Caster;
	uint32 ShadowDraws;
	uint64 UsedAtlasBytes;
	float UsedDirectionalMiBs;

	float DirectionalShadowRenderTime;
	float PointShadowRenderTime;
	float SpotShadowRenderTime;

	uint32 TotalFaces;
	uint32 ReusedFaces;
};

struct FShadowRequest
{
	FScene* Scene = nullptr;
	FLightSceneProxy* Light = nullptr;
	uint32 Size = 0;
	uint32 ViewCount = 0;
	uint32 LightId = 0;
	uint32 CullViewOffset = 0;
	uint32 CullViewCount = 0;
};

class FShadowRenderer
{
public:
	void Initialize();
	void BeginFrame();
	void GatherLocalRequests(FScene& Scene, TArray<FSceneView>& WorldViews);
	void RenderLocalAtlas(FRenderer& Renderer);
	void AllocateDirectionalSlots(FScene& Scene);
	void RenderDirectional(FRenderer& Renderer, FScene& Scene, const FSceneView& View);

	ID3D11ShaderResourceView* CopyShadowSliceForDebug(uint32 SourceSlice, uint32 DestinationSlice);
	ID3D11ShaderResourceView* GetSRV() const;
	uint32 GetDirectionalShadowMapResolution() const { return ShadowMapTexture->GetWidth(); }
	float GetDirectionalShadowMapCapacityMiBs() const
	{
		return static_cast<float>(ShadowMapTexture->GetWidth()) * ShadowMapTexture->GetHeight()
			* ShadowMapTexture->GetArraySize() * 4 / (1024 * 1024);
	}
	ID3D11ShaderResourceView* GetLocalAtlasSRV() const;

	bool GetAtlasTile(uint32 LightId, uint32 Face, FShadowAtlasTile& OutTile) const;
	static FVector4 TileToAtlasRect(const FShadowAtlasTile& Tile);

public:
	static constexpr uint32 LocalAtlasSize = 8192;
	static constexpr uint32 LocalAtlasMinTileSize = 256;

	const FShadowStats& GetShadowStats() { return ShadowStats; }
	void SetShadowEnabled(bool ShadowEnabled) { bShadowEnabled = ShadowEnabled; }
	void SetCSMEnabled(bool CSMEnabled) { bCSMEnabled = CSMEnabled; }
	void SetCacheEnabled(bool CacheEnabled) { bCacheEnabled = CacheEnabled; }

private:
	void ResizeDirectionalShadowMaps(uint32 Resolution);
	void RenderFace(FRenderer& Renderer, FScene& Scene, const FViewContext& ViewContext, uint32 Slot,
		float ShadowBias, float ShadowSlopeBias);
	void DrawShadowView(FRenderer& Renderer, FScene& Scene, const FViewContext& ViewContext,
		float ShadowBias, float ShadowSlopeBias, const FViewContext* CasterCullViews = nullptr, uint32 CasterCullViewCount = 0);
	uint32 AllocateSlots(uint32 Count);
	void FitRequestsToBudget();

	static FViewContext MakeViewContext(const FVector& Location, const FShadowView& ShadowView, uint32 Width, uint32 Height, const FSceneView* View = nullptr);
	void UpdateDirectionalShadowMapFrustum(
		FViewContext& ViewContext,
		const FSceneView& View,
		const FVector& Forward,
		uint32 Index,
		FDirectionalLightRenderData& RenderData
	);
	static float GetCascadeExponentialSum(uint32 Index, uint32 Exponential);
	void UpdateSceneShadowMapFrustum(FViewContext& ViewContext, const FBox& SceneBounds,
		const FVector& Forward, FDirectionalLightRenderData& RenderData);
	void UpdateDirectionalShadowProjection(FViewContext& ViewContext, const FMatrix& LightView,
		const FVector& MinPoint, const FVector& MaxPoint, uint32 Index, FDirectionalLightRenderData& RenderData);
	static uint64 GetRequestArea(const FShadowRequest& Request);

	void BeginShadowMapRenderTimer(ELightType LightType, uint32 LightIndex, uint32 ViewIndex);
	void EndShadowMapRenderTimer();
	void UpdateShadowMapRenderTime();

	void FreeCacheEntryTiles(FShadowCacheEntry& Entry);
	void SweepUnseenCacheEntries();
	void ResetLocalAtlas();
	void RepackLocalAtlas();
	void ClearTile();

	bool ShouldRedrawFace(const FScene& Scene, const FLightSceneProxy& Light, uint32 Face) const;
	void ResetCasterChanges();

private:
	TUniquePtr<FTexture2DArray> ShadowMapTexture;
	TUniquePtr<FTexture2DArray> ShadowDebugTexture;
	uint32 NextSlot = 0;

	FShadowStats ShadowStats{};

	// Shadow Atlas
	TArray<FShadowRequest> LocalRequests;
	TArray<FViewContext> LocalCasterCullViews;
	TUniquePtr<FTexture2D> LocalAtlas;
	FShadowAtlasAllocator LocalAtlasAllocator;

	struct FShadowMapTimerQuery
	{
		ComPtr<ID3D11Query> Disjoint;
		ComPtr<ID3D11Query> Begin;
		ComPtr<ID3D11Query> End;

		ELightType LightType;
		uint32 LightIndex = 0;
		uint32 ViewIndex = 0;
		bool bIssued = false;
	};

	TArray<FShadowMapTimerQuery> ShadowMapQueries;
	uint32 ShadowMapQueryIndex = 0;
	bool bShadowMapTimerActive = false;

	bool bShadowEnabled = true;
	bool bCSMEnabled = true;
	bool bCacheEnabled = false;

	TMap<uint32, FShadowCacheEntry> ShadowCaches;
	uint64 FrameNumber = 0;

	bool bShadowEnabledLastFrame = true;
	TArray<FScene*> Scenes;
};
