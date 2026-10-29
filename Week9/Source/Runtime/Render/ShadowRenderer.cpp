#include "EnginePCH.h"
#include "ShadowRenderer.h"
#include "RenderCommand.h"
#include "Engine/Scene.h"
#include "SceneRenderData.h"
#include "SceneView.h"
#include "Renderer.h"
#include "SceneRenderer.h"
#include "Math/Frustum.h"
#include "Core/Stats/LightweightStats.h"
#include "Component/DirectionalLightComponent.h"

DECLARE_CYCLE_STAT("Shadow Map CPU Draw", STAT_ShadowMapBuild);

void FShadowRenderer::ResetLocalAtlas()
{
	LocalAtlasAllocator.Reset(LocalAtlasSize, LocalAtlasMinTileSize);
	ShadowCaches.Reset();
}

void FShadowRenderer::ResizeDirectionalShadowMaps(uint32 Resolution)
{
	constexpr uint32 NumSlices = 16;

	const D3D11_TEXTURE2D_DESC TextureDesc = {
		.Width = Resolution,
		.Height = Resolution,
		.MipLevels = 1,
		.ArraySize = NumSlices,
		.Format = DXGI_FORMAT_R24G8_TYPELESS,
		.SampleDesc = {
			.Count = 1,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
	};

	ShadowMapTexture = RenderCommand::CreateTexture2DArray(TextureDesc);

	// 디버깅용 텍스처 (복사)
	D3D11_TEXTURE2D_DESC DebugDesc = TextureDesc;
	DebugDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	ShadowDebugTexture = RenderCommand::CreateTexture2DArray(DebugDesc);
}

void FShadowRenderer::Initialize()
{
	ResizeDirectionalShadowMaps(UDirectionalLightComponent::GetSharedShadowMapResolution());

	// Shadow Atlas
	D3D11_TEXTURE2D_DESC AtlasDesc{};
	AtlasDesc.ArraySize = 1;
	AtlasDesc.Width = LocalAtlasSize;
	AtlasDesc.Height = LocalAtlasSize;
	AtlasDesc.MipLevels = 1;
	AtlasDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	AtlasDesc.SampleDesc.Count = 1;
	AtlasDesc.Usage = D3D11_USAGE_DEFAULT;
	AtlasDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	LocalAtlas = RenderCommand::CreateTexture2D(AtlasDesc);

	ShadowMapQueries.SetNum(15);

	for (FShadowMapTimerQuery& QuerySet : ShadowMapQueries)
	{
		D3D11_QUERY_DESC QueryDesc{};
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		RenderCommand::GetDevice()->CreateQuery(&QueryDesc, &QuerySet.Disjoint);

		QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
		RenderCommand::GetDevice()->CreateQuery(&QueryDesc, &QuerySet.Begin);
		RenderCommand::GetDevice()->CreateQuery(&QueryDesc, &QuerySet.End);
	}

	ResetLocalAtlas();
}

void FShadowRenderer::BeginFrame()
{
	++FrameNumber;
	ShadowStats = {};
	RenderCommand::UnbindShaderResources(13, 2, EShaderBindFlagBits::Pixel); // t13: Shadow Array, t14: Local Atlas
	NextSlot = 0;
	LocalRequests.Reset();
	UpdateShadowMapRenderTime();
	RenderCommand::UnbindShaderResources(12, 1, EShaderBindFlagBits::Pixel);
	const uint32 Resolution = UDirectionalLightComponent::GetSharedShadowMapResolution();
	if (ShadowMapTexture && ShadowMapTexture->GetWidth() != Resolution)
		ResizeDirectionalShadowMaps(Resolution);
	Scenes.Reset();

	if (!bCacheEnabled)
	{
		RenderCommand::ClearDepthStencil(LocalAtlas.get());
		ResetLocalAtlas();
	}

	if (bShadowEnabled && !bShadowEnabledLastFrame)
		ResetLocalAtlas();

	bShadowEnabledLastFrame = bShadowEnabled;
}

void FShadowRenderer::AllocateDirectionalSlots(FScene& Scene)
{
	if (!ShadowMapTexture)
		return;
	FBox SceneBounds;
	const bool bHasShadowBounds = bCSMEnabled || Scene.BVH.GetRootBounds(SceneBounds);

	for (FLightSceneProxy* Light : Scene.GetLights())
	{
		if (!Light)
			continue;

		// 이전 프레임 슬롯 남지 않도록 초기화
		Light->SetFirstShadowSlot(InvalidShadowSlot);

		// 비활성화 되었으면 넘어감
		if (!Light->IsEnabled() || Light->GetLightType() != ELightType::Directional || !bHasShadowBounds)
			continue;

		const FShadowMapInfo& ShadowMapInfo = Light->GetShadowMapInfo();
		const uint32 FaceCount = bCSMEnabled ? static_cast<uint32>(ShadowMapInfo.ShadowViews.Num())
			: std::min(1u, static_cast<uint32>(ShadowMapInfo.ShadowViews.Num()));

		// 아무 Face가 존재하지 않음
		if (FaceCount == 0)
			continue;

		const uint32 FirstSlot = AllocateSlots(FaceCount);

		// ShadowMapTexture에 남은 공간이 없음
		if (FirstSlot == InvalidShadowSlot)
			continue; // 슬롯 초기화 위해 break하지 않음

		Light->SetFirstShadowSlot(FirstSlot);

		++ShadowStats.DirectionalLights;
	}
}

void FShadowRenderer::RenderDirectional(FRenderer& Renderer, FScene& Scene, const FSceneView& View)
{
	if (!bShadowEnabled || !ShadowMapTexture)
		return;

	const uint32 Width = ShadowMapTexture->GetWidth();
	const uint32 Height = ShadowMapTexture->GetHeight();
	FBox SceneBounds;
	if (!bCSMEnabled && !Scene.BVH.GetRootBounds(SceneBounds))
		return;

	for (FLightSceneProxy* Light : Scene.GetLights())
	{
		if (!Light || Light->GetLightType() != ELightType::Directional || !Light->HasShadowSlot())
			continue;

		const FShadowMapInfo& ShadowMapInfo = Light->GetShadowMapInfo();
		const uint32 FaceCount = bCSMEnabled ? static_cast<uint32>(ShadowMapInfo.ShadowViews.Num())
			: std::min(1u, static_cast<uint32>(ShadowMapInfo.ShadowViews.Num()));
		const uint32 FirstSlot = Light->GetFirstShadowSlot();

		// ViewProjection 행렬 갱신
		auto& Data = Light->GetRenderData();
		if (auto* DirectionalData = std::get_if<FDirectionalLightRenderData>(&Data))
		{
			// 컴포넌트의 CascadeCount는 유지하고 실제 사용하는 개수만 전달한다.
			DirectionalData->CascadeCount = FaceCount;
			BeginShadowMapRenderTimer(ELightType::Directional, ShadowStats.DirectionalLights - 1, View.ViewIndex);

			for (uint32 i = 0; i < FaceCount; ++i)
			{
				const FShadowView& ShadowView = ShadowMapInfo.ShadowViews[i];
				FViewContext ViewContext = MakeViewContext(ShadowMapInfo.Location, ShadowView, Width, Height, &View);
				if (bCSMEnabled)
					UpdateDirectionalShadowMapFrustum(ViewContext, View, ShadowView.Forward, i, *DirectionalData);
				else
					UpdateSceneShadowMapFrustum(ViewContext, SceneBounds, ShadowView.Forward, *DirectionalData);
				RenderFace(Renderer, Scene, ViewContext, FirstSlot + i, ShadowView.ShadowBias, ShadowView.ShadowSlopeBias);

				ShadowStats.UsedDirectionalMiBs += static_cast<float>(Width) * Height * 4 / (1024 * 1024);
			}

			EndShadowMapRenderTimer();
		}
	}
}

void FShadowRenderer::RenderFace(FRenderer& Renderer, FScene& Scene, const FViewContext& ViewContext, uint32 Slot,
	float ShadowBias, float ShadowSlopeBias)
{
	// 렌더링
	RenderCommand::BeginRenderPass(
		ShadowMapTexture->GetSliceDSV(Slot), 
		ShadowMapTexture->GetWidth(),
		ShadowMapTexture->GetHeight()
	);
	DrawShadowView(Renderer, Scene, ViewContext, ShadowBias, ShadowSlopeBias);
	RenderCommand::EndRenderPass();
}

void FShadowRenderer::DrawShadowView(FRenderer& Renderer, FScene& Scene, const FViewContext& ViewContext,
	float ShadowBias, float ShadowSlopeBias, const FViewContext* CasterCullViews, uint32 CasterCullViewCount)
{
	SCOPE_CYCLE_COUNTER(STAT_ShadowMapBuild);

	FSceneRenderData RenderData;
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(ViewContext.ViewProjection);

	// 월드 프리미티브 수집 및 절두체 컬링
	FSceneRenderer::GatherShadowMapRenderPackets(Scene, RenderData, ViewContext, &Frustum, &Renderer, CasterCullViews, CasterCullViewCount);

	// Caster가 없으면 Shadow Pass 생략
	if (RenderData.VisibleProxies.Num() == 0)
	{
		return;
	}

	++ShadowStats.RenderedFaces;
	ShadowStats.Caster += RenderData.VisibleProxies.Num();
	ShadowStats.ShadowDraws += RenderData.RenderStats.DrawCalls;

	// 렌더 큐 자체 정렬
	RenderData.RenderQueue.Sort();

	FSceneRenderer::PrepareDrawData(&Renderer, RenderData);
	Renderer.RenderShadowPass(ViewContext.ViewProjection, RenderData.RenderQueue, ShadowBias, ShadowSlopeBias);
}

FViewContext FShadowRenderer::MakeViewContext(const FVector& Location, const FShadowView& ShadowView, uint32 Width, uint32 Height, const FSceneView* View)
{
	const FMatrix& ViewProjection = ShadowView.ViewProjection;

	//ViewContext 세팅
	FViewContext ViewContext;
	{
		ViewContext.Width = Width;
		ViewContext.Height = Height;
		ViewContext.CameraPosition = Location;
		ViewContext.CameraForward = ShadowView.Forward;
		ViewContext.ViewProjection = ViewProjection;
		if (View)
		{
			ViewContext.NearZ = View->NearClip;
			ViewContext.FarZ = View->FarClip;
		}
		ViewContext.bOrthographic = ShadowView.bIsOrthographic;
		ViewContext.bGameView = false;

		const float ScaleX = ShadowView.Projection.M[1][0];
		const float ScaleY = ShadowView.Projection.M[2][1];
		ViewContext.ProjectionScaleSquared = (ScaleX * ScaleX > ScaleY * ScaleY) ? (ScaleX * ScaleX) : (ScaleY * ScaleY);
		ViewContext.Prepare();
	}

	return ViewContext;
}

uint32 FShadowRenderer::AllocateSlots(uint32 Count)
{
	if (!ShadowMapTexture)
		return InvalidShadowSlot;

	uint32 SliceCount = ShadowMapTexture->GetArraySize();

	if (NextSlot + Count > SliceCount)
		return InvalidShadowSlot;

	const uint32 First = NextSlot;
	NextSlot += Count;

	return First;
}

void FShadowRenderer::GatherLocalRequests(FScene& Scene, TArray<FSceneView>& WorldViews)
{
	Scenes.Add(&Scene);

	const uint32 CullViewOffset = LocalCasterCullViews.Num();
	for (const FSceneView& View : WorldViews)
		LocalCasterCullViews.Add(View.ToViewContext());
	const uint32 CullViewCount = LocalCasterCullViews.Num() - CullViewOffset;

	for (FLightSceneProxy* Light : Scene.GetLights())
	{
		if (!Light)
			continue;

		const ELightType LightType = Light->GetLightType();
		if (!Light->IsEnabled() || (LightType != ELightType::Point && LightType != ELightType::Spot))
			continue;

		const uint32 ShadowViewCount = Light->GetShadowMapInfo().ShadowViews.Num();
		if (ShadowViewCount <= 0)
			continue;

		const uint32 Size = std::clamp(Light->GetShadowMapResolution(), LocalAtlasMinTileSize, LocalAtlasSize);

		bool bIntersectsView = false;

		for (const FSceneView& View : WorldViews)
		{
			if (IsSphereInFrustum(Light->GetCullingSphereCenter(), Light->GetCullingSphereRadius(), View.Frustum))
			{
				if (LightType == ELightType::Spot)
				{
					const auto& RenderData = Light->GetRenderData();
					if (const auto* SpotData = std::get_if<FSpotLightRenderData>(&RenderData))
					{
						const FVector Direction = SpotData->Direction;
						if (!IsConeInFrustum(SpotData->Location, Direction, SpotData->CosOuter, View.Frustum))
						{
							continue;
						}
					}
				}
				bIntersectsView = true;
				break;
			}
		}

		if (!bIntersectsView)
		{
			continue;
		}
		LocalRequests.Add({ &Scene, Light, Size, ShadowViewCount, Light->GetLightId(), CullViewOffset, CullViewCount });
	}
}

void FShadowRenderer::RenderLocalAtlas(FRenderer& Renderer)
{
	if (!bShadowEnabled || !LocalAtlas)
	{
		ResetCasterChanges();
		return;
	}

	FitRequestsToBudget();

	for (const FShadowRequest& Request : LocalRequests)
	{
		ShadowCaches[Request.LightId].LastSeenFrame = FrameNumber;
	}

	SweepUnseenCacheEntries();

	std::stable_sort(LocalRequests.begin(), LocalRequests.end(), [](const FShadowRequest& A, const FShadowRequest& B) { return A.Size > B.Size; });

	for (FShadowRequest& Request : LocalRequests)
	{
		FShadowCacheEntry& CacheEntry = ShadowCaches[Request.LightId];
		CacheEntry.bTilePlacementChanged = false;

		if (CacheEntry.Size != Request.Size || CacheEntry.ViewCount != Request.ViewCount)
			FreeCacheEntryTiles(CacheEntry);
	}

	bool bNeedRepack = false;
	for (FShadowRequest& Request : LocalRequests)
	{
		FShadowCacheEntry& CacheEntry = ShadowCaches[Request.LightId];
		
		if (CacheEntry.ViewCount != 0)
			continue;

		if (!LocalAtlasAllocator.TryAllocate(Request.Size, Request.ViewCount, CacheEntry.Tiles))
		{
			bNeedRepack = true;
			break;
		}
		CacheEntry.Size = Request.Size;
		CacheEntry.ViewCount = Request.ViewCount;
		CacheEntry.bTilePlacementChanged = true;
	}

	if (bNeedRepack)
		RepackLocalAtlas();

	for (FShadowRequest& Request : LocalRequests)
	{
		FLightSceneProxy* Light = Request.Light;
		FShadowCacheEntry& CacheEntry = ShadowCaches[Request.LightId];

		if (CacheEntry.ViewCount == 0)
			continue;

		assert(Request.ViewCount <= 6);
		const FShadowMapInfo& ShadowMapInfo = Light->GetShadowMapInfo();
		const TArray<FShadowView>& ShadowViews = ShadowMapInfo.ShadowViews;

		switch (Light->GetLightType())
		{
		case ELightType::Point:
			++ShadowStats.PointLights;
			BeginShadowMapRenderTimer(ELightType::Point, ShadowStats.PointLights, 1);
			break;
		case ELightType::Spot:
			++ShadowStats.SpotLights;
			BeginShadowMapRenderTimer(ELightType::Spot, ShadowStats.SpotLights, 1);
			break;
		}

		for (uint32 Face = 0; Face < CacheEntry.ViewCount; ++Face)
		{
			const FShadowView& ShadowView = ShadowViews[Face];
			const FShadowAtlasTile& Tile = CacheEntry.Tiles[Face];

			++ShadowStats.TotalFaces;
			ShadowStats.UsedAtlasBytes += static_cast<uint64>(Request.Size) * Request.Size * 4;

			const bool bDraw = !bCacheEnabled || CacheEntry.bTilePlacementChanged || ShouldRedrawFace(*Request.Scene, *Light, Face);
			// reuse cache
			if (!bDraw)
			{
				++ShadowStats.ReusedFaces;
				continue;
			}

			RenderCommand::BindRenderPassNoClear(nullptr, LocalAtlas.get(), LocalAtlasSize, LocalAtlasSize);
			RenderCommand::SetViewport(Tile.X, Tile.Y, Tile.Size, Tile.Size);
			if (bCacheEnabled)
				ClearTile();
			FViewContext ViewContext = MakeViewContext(ShadowMapInfo.Location, ShadowView, Tile.Size, Tile.Size);
			const FViewContext* CasterCullViews = Request.CullViewCount > 0 ? LocalCasterCullViews.GetData() + Request.CullViewOffset : nullptr;
			DrawShadowView(Renderer, *Request.Scene, ViewContext, ShadowView.ShadowBias, ShadowView.ShadowSlopeBias, CasterCullViews, Request.CullViewCount);
			
			CacheEntry.Captures[Face] = {
				ShadowView.ViewProjection,
				ShadowView.ShadowBias,
				ShadowView.ShadowSlopeBias
			};
		}
		EndShadowMapRenderTimer();
	}

	RenderCommand::EndRenderPass();
	ResetCasterChanges();
}

void FShadowRenderer::FitRequestsToBudget()
{
	uint64 AtlasCapacity = (uint64)LocalAtlasSize * (uint64)LocalAtlasSize;
	uint64 Total = 0;
	for (const FShadowRequest& Request : LocalRequests)
		Total += GetRequestArea(Request);

	while (Total > AtlasCapacity)
	{
		uint32 MaxIndex = 0;
		uint32 MaxSize = 0;
		for (uint32 i = 0; i < LocalRequests.Num(); ++i)
		{
			const uint32 Size = LocalRequests[i].Size;
			if (Size >= MaxSize)
			{
				MaxIndex = i;
				MaxSize = Size;
			}
		}

		FShadowRequest& MaxRequest = LocalRequests[MaxIndex];
		if (MaxRequest.Size > LocalAtlasMinTileSize)
		{
			Total -= GetRequestArea(MaxRequest);
			MaxRequest.Size /= 2;
			Total += GetRequestArea(MaxRequest);
		}
		// 만약 더 이상 줄일 수 있는 요청이 없다면
		// 가장 마지막 요청은 드랍한다.
		else
		{
			FShadowRequest& LastRequest = LocalRequests.Last();
			Total -= GetRequestArea(LastRequest);
			LocalRequests.RemoveLast();
		}
	}
}

ID3D11ShaderResourceView* FShadowRenderer::CopyShadowSliceForDebug(uint32 SourceSlice, uint32 DestinationSlice)
{
	if (!ShadowMapTexture || !ShadowDebugTexture)
		return nullptr;
	
	if (SourceSlice >= ShadowMapTexture->GetArraySize())
		return nullptr;
	
	if (DestinationSlice >= ShadowDebugTexture->GetArraySize())
		return nullptr;

	RenderCommand::GetContext()->CopySubresourceRegion(
		ShadowDebugTexture->GetRawPtr(),
		D3D11CalcSubresource(0, DestinationSlice, 1),
		0, 0, 0,
		ShadowMapTexture->GetRawPtr(),
		D3D11CalcSubresource(0, SourceSlice, 1),
		nullptr
	);

	return ShadowDebugTexture->GetSliceSRV(DestinationSlice);
}

ID3D11ShaderResourceView* FShadowRenderer::GetSRV() const
{
	if (!ShadowMapTexture)
		return nullptr;

	return ShadowMapTexture->GetSRV();
}

ID3D11ShaderResourceView* FShadowRenderer::GetLocalAtlasSRV() const
{
	if (!LocalAtlas)
		return nullptr;

	return LocalAtlas->GetSRV();
}

FVector4 FShadowRenderer::TileToAtlasRect(const FShadowAtlasTile& Tile)
{
	if (Tile.Size == 0)
		return {};

	float AtlasSize = (float)LocalAtlasSize;
	return {
		(float)Tile.X / AtlasSize,
		(float)Tile.Y / AtlasSize,
		(float)Tile.Size / AtlasSize,
		(float)Tile.Size / AtlasSize,
	};
}

uint64 FShadowRenderer::GetRequestArea(const FShadowRequest& Request)
{
	return (uint64)Request.Size * (uint64)Request.Size * (uint64)Request.ViewCount;
}

float FShadowRenderer::GetCascadeExponentialSum(uint32 Index, uint32 Exponential)
{
	// 지수가 1일때
	if (Exponential == 1)
	{
		return Index;
	}

	// 등비수열의 합 공식
	int32 Upper = 1 - std::pow(Exponential, Index);
	float Lower = 1 - Exponential;
	return Upper / Lower;
}

// ViewContext의 VP에 주어진 Directional Light로 계산된 VP 행렬을 담는 함수.
// TODO: 리팩토링 할 것
void FShadowRenderer::UpdateDirectionalShadowMapFrustum(
	FViewContext& ViewContext,
	const FSceneView& View,
	const FVector& Forward,
	uint32 Index,
	FDirectionalLightRenderData& RenderData
)
{
	float Near = View.NearClip;
	float Far = View.FarClip;

	uint32 CascadeCount = RenderData.CascadeCount;
	uint32 CascadeExponential = RenderData.CascadeExponential;
	float CascadeBlendRange = RenderData.CascadeBlendRange;

	// 1. Near ~ Far 사이에서 적절한 Depth 범위 계산
	float ExponentialTotalSum = GetCascadeExponentialSum(CascadeCount, CascadeExponential);
	float DepthPoint[] = {
		GetCascadeExponentialSum(Index,		CascadeExponential) / ExponentialTotalSum,
		GetCascadeExponentialSum(Index + 1, CascadeExponential) / ExponentialTotalSum,
	};

	// 2. 2개의 Shadow Map의 보간을 위해 CascadeBlendRange (World 공간 기준 길이) 만큼 Shadow Map의 영역을 연장.
	float EndDepth = DepthPoint[1];
	float BlendRatio = CascadeBlendRange / (Far - Near);
	DepthPoint[1] = std::min(DepthPoint[1] + BlendRatio, 1.0f);

	// 3. Light로의 View 행렬 만들기
	const FVector WorldUpVector = FMatrix::CanMakeLookDirection(Forward, UpVector) ? UpVector : ForwardVector;
	FMatrix LightView = FMatrix::MakeLookDirection(ZeroVector, Forward, WorldUpVector);

	// 4. Frustum의 8개 꼭짓점 구하기
	float HalfTan = std::tanf(FMath::DegreesToRadians(View.FieldOfView) * 0.5f);

	TArray<FVector> Points;
	for (int32 _X = 0; _X < 2; _X += 1)
	{
		float X = Near + (Far - Near) * DepthPoint[_X];
		for (int32 _Y = -1; _Y < 2; _Y += 2)
		{
			float Y = X * HalfTan * View.AspectRatio * _Y;
			for (int32 _Z = -1; _Z < 2; _Z += 2)
			{
				float Z = X * HalfTan * _Z;
				Points.Add({X, Y, Z});
			}
		}
	}

	// 5. Frustum의 중심점 구하기
	FVector Center;
	for (int i = 0; i < 8; ++i)
	{
		Center += FVector(Points[i].X, Points[i].Y, Points[i].Z);
	}

	Center /= 8;

	// 6. Frustum을 외접하는 구 만들기
	double Distance = 0.0f;
	for (int32 i = 0; i < 8; ++i)
	{
		Distance = std::max(Distance, (double)FVector::Distance(Center, Points[i]));
	}

	Distance += 1.0f;

	// 7. Center를 View -> World -> Light 공간으로
	Center = View.ViewLocation + View.ViewForward * Center.X;
	Center = LightView.TransformPosition(Center);

	// 8. 구를 가지고 AABB 만들기
	FVector Extent = OneVector * Distance;
	FVector MinPoint = Center - Extent;
	FVector MaxPoint = Center + Extent;

	// 9. AABB를 앞으로 연장시키기.
	// TODO: 나중에 실제 물체를 가져와서 현재 AABB 영역 상에서
	// 가장 멀리 있는 물체를 찾아서 그거 기준으로 바꿔야 함
	constexpr float PointMultiply = 10.0f;

	if (MinPoint.X < 0)
	{
		MinPoint.X *= PointMultiply;
	}
	else
	{
		MinPoint.X /= PointMultiply;
	}

	// 10. Texel Snapping
	// 직교 투영 행렬을 Texel에 맞게끔 조정
	float Diameter = Distance * 2.0f;
	float TexelY = Diameter / static_cast<float>(ShadowMapTexture->GetWidth());
	float TexelZ = Diameter / static_cast<float>(ShadowMapTexture->GetHeight());

	float SnappedY = std::round(Center.Y / TexelY) * TexelY;
	float SnappedZ = std::round(Center.Z / TexelZ) * TexelZ;

	MinPoint.Y = SnappedY - Distance;
	MaxPoint.Y = SnappedY + Distance;
	MinPoint.Z = SnappedZ - Distance;
	MaxPoint.Z = SnappedZ + Distance;
	
	UpdateDirectionalShadowProjection(ViewContext, LightView, MinPoint, MaxPoint, Index, RenderData);
	RenderData.ShadowSplitDepths[Index] = EndDepth;
}

void FShadowRenderer::UpdateSceneShadowMapFrustum(FViewContext& ViewContext, const FBox& SceneBounds,
	const FVector& Forward, FDirectionalLightRenderData& RenderData)
{
	const FVector WorldUpVector = FMatrix::CanMakeLookDirection(Forward, UpVector) ? UpVector : ForwardVector;
	const FMatrix LightView = FMatrix::MakeLookDirection(ZeroVector, Forward, WorldUpVector);
	// 중심과 절댓값 회전으로 변환하여 Scene AABB 전체를 포함한다.
	const FBox LightBounds = SceneBounds.GetWorldAABB(LightView);
	const FVector Center = (LightBounds.Min + LightBounds.Max) * 0.5f;
	// 기존 필터의 단일 ShadowTexelWorld를 재사용하도록 투영 평면은 정사각형으로 맞춘다.
	const float Extent = std::max(LightBounds.Max.Y - LightBounds.Min.Y,
		LightBounds.Max.Z - LightBounds.Min.Z) * 0.5f + 1.0f;
	const FVector MinPoint(LightBounds.Min.X - 1.0f, Center.Y - Extent, Center.Z - Extent);
	const FVector MaxPoint(LightBounds.Max.X + 1.0f, Center.Y + Extent, Center.Z + Extent);

	UpdateDirectionalShadowProjection(ViewContext, LightView, MinPoint, MaxPoint, 0, RenderData);
	RenderData.ShadowSplitDepths[0] = 1.0f;
}

void FShadowRenderer::UpdateDirectionalShadowProjection(FViewContext& ViewContext, const FMatrix& LightView,
	const FVector& MinPoint, const FVector& MaxPoint, uint32 Index, FDirectionalLightRenderData& RenderData)
{
	// Orthogonal Projection 행렬 만들기
	FMatrix LightProjection = FMatrix::MakeOrthographic(
		MinPoint.Y, MaxPoint.Y,
		MinPoint.Z, MaxPoint.Z,
		MinPoint.X,
		MaxPoint.X
	);


	// 12. 완성된 결과물을 out parameter에 적용
	float Width = MaxPoint.Y - MinPoint.Y;

	ViewContext.Width = MaxPoint.Y - MinPoint.Y;
	ViewContext.Height = MaxPoint.Z - MinPoint.Z;
	ViewContext.ViewProjection = LightView * LightProjection;
	ViewContext.Projection = LightProjection;
	ViewContext.NearZ = MinPoint.X;
	ViewContext.FarZ = MaxPoint.X;
	ViewContext.bOrthographic = true;
	const float ScaleX = LightProjection.M[1][0];
	const float ScaleY = LightProjection.M[2][1];
	ViewContext.ProjectionScaleSquared = (ScaleX * ScaleX > ScaleY * ScaleY) ? (ScaleX * ScaleX) : (ScaleY * ScaleY);
	ViewContext.Prepare();

	RenderData.ViewProjection[Index] = ViewContext.ViewProjection;
	RenderData.ShadowTexelWorld[Index] = Width / static_cast<float>(ShadowMapTexture->GetWidth());
	RenderData.ShadowNear[Index] = ViewContext.NearZ;
	RenderData.ShadowFar[Index] = ViewContext.FarZ;
}

void FShadowRenderer::BeginShadowMapRenderTimer(ELightType LightType, uint32 LightIndex, uint32 ViewIndex)
{
	bShadowMapTimerActive = false;

	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	FShadowMapTimerQuery& QuerySet = ShadowMapQueries[ShadowMapQueryIndex];

	if (QuerySet.bIssued)
	{
		return; // 이전 결과 미회수
	}

	QuerySet.LightType = LightType;
	QuerySet.LightIndex = LightIndex;
	QuerySet.ViewIndex = ViewIndex;

	Context->Begin(QuerySet.Disjoint.Get());
	Context->End(QuerySet.Begin.Get());

	bShadowMapTimerActive = true;
}

void FShadowRenderer::EndShadowMapRenderTimer()
{
	if (!bShadowMapTimerActive)
	{
		return;
	}

	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	FShadowMapTimerQuery& QuerySet = ShadowMapQueries[ShadowMapQueryIndex];

	Context->End(QuerySet.End.Get());
	Context->End(QuerySet.Disjoint.Get());

	QuerySet.bIssued = true;

	ShadowMapQueryIndex = (ShadowMapQueryIndex + 1) % ShadowMapQueries.Num();

	bShadowMapTimerActive = false;
}

void FShadowRenderer::UpdateShadowMapRenderTime()
{
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	constexpr UINT Flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;

	float DirectionalSum = 0.0f;
	float PointSum = 0.0f;
	float SpotSum = 0.0f;
	uint32 DirectionalCount = 0;
	uint32 PointCount = 0;
	uint32 SpotCount = 0;

	for (FShadowMapTimerQuery& QuerySet : ShadowMapQueries)
	{
		if (!QuerySet.bIssued)
		{
			continue;
		}

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Disjoint{};
		UINT64 BeginTimestamp = 0;
		UINT64 EndTimestamp = 0;

		if (Context->GetData(QuerySet.Disjoint.Get(), &Disjoint, sizeof(Disjoint), Flags) != S_OK ||
			Context->GetData(QuerySet.Begin.Get(), &BeginTimestamp, sizeof(BeginTimestamp), Flags) != S_OK ||
			Context->GetData(QuerySet.End.Get(), &EndTimestamp, sizeof(EndTimestamp), Flags) != S_OK)
		{
			continue; // 결과가 준비되면 return
		}

		if (!Disjoint.Disjoint && Disjoint.Frequency != 0 && EndTimestamp >= BeginTimestamp)
		{
			const float Ms =
				static_cast<float>(EndTimestamp - BeginTimestamp) *
				1000.0f / static_cast<float>(Disjoint.Frequency);

			switch (QuerySet.LightType)
			{
			case ELightType::Directional:
				DirectionalSum += Ms;
				++DirectionalCount;
				break;
			case ELightType::Point:
				PointSum += Ms;
				++PointCount;
				break;
			case ELightType::Spot:
				SpotSum += Ms;
				++SpotCount;
				break;
			default:
				break;
			}
		}

		QuerySet.bIssued = false;
	}

	if (DirectionalCount > 0)
	{
		ShadowStats.DirectionalShadowRenderTime = DirectionalSum / DirectionalCount;
	}
	if (PointCount > 0)
	{
		ShadowStats.PointShadowRenderTime = PointSum / PointCount;
	}
	if (SpotCount > 0)
	{
		ShadowStats.SpotShadowRenderTime = SpotSum / SpotCount;
	}
}

void FShadowRenderer::FreeCacheEntryTiles(FShadowCacheEntry& Entry)
{
	for (uint32 i = 0; i < Entry.ViewCount; ++i)
	{
		LocalAtlasAllocator.Free(Entry.Tiles[i]);
		Entry.Tiles[i] = {};
	}
	Entry.Size = 0;
	Entry.ViewCount = 0;
}

void FShadowRenderer::SweepUnseenCacheEntries()
{
	TArray<uint32> Dead;
	for (auto& [Id, CacheEntry] : ShadowCaches)
	{
		if (CacheEntry.LastSeenFrame != FrameNumber)
		{
			FreeCacheEntryTiles(CacheEntry);
			Dead.Add(Id);
		}
	}

	for (uint32 Id : Dead)
		ShadowCaches.Remove(Id);
}

void FShadowRenderer::RepackLocalAtlas()
{
	ResetLocalAtlas();

	for (FShadowRequest& Request : LocalRequests)
	{
		FShadowCacheEntry& CacheEntry = ShadowCaches[Request.LightId];
		CacheEntry.LastSeenFrame = FrameNumber;

		const bool bFits = LocalAtlasAllocator.TryAllocate(Request.Size, Request.ViewCount, CacheEntry.Tiles);
		assert(bFits);
		if (!bFits)
			continue;
		CacheEntry.Size = Request.Size;
		CacheEntry.ViewCount = Request.ViewCount;
		CacheEntry.bTilePlacementChanged = true;
	}
}

void FShadowRenderer::ClearTile()
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::ShadowTileClear);

	if (!PSO || !PSO->Shader)
		return;

	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::Draw(3);
}

bool FShadowRenderer::GetAtlasTile(uint32 LightId, uint32 Face, FShadowAtlasTile& OutTile) const
{
	if (!bShadowEnabled)
		return false;

	const FShadowCacheEntry* CacheEntry = ShadowCaches.Find(LightId);
	if (!CacheEntry || CacheEntry->LastSeenFrame != FrameNumber)
		return false;
	if (Face >= CacheEntry->ViewCount)
		return false;
	OutTile = CacheEntry->Tiles[Face];
	return true;
}

bool FShadowRenderer::ShouldRedrawFace(const FScene& Scene, const FLightSceneProxy& Light, uint32 Face) const
{
	const FShadowCacheEntry& CacheEntry = ShadowCaches[Light.GetLightId()];
	const FShadowViewCapture& Capture = CacheEntry.Captures[Face];
	const TArray<FShadowView>& ShadowViews = Light.GetShadowMapInfo().ShadowViews;
	const FShadowView& ShadowView = ShadowViews[Face];

	if (Capture.ViewProjection != ShadowView.ViewProjection
		|| Capture.ShadowBias != ShadowView.ShadowBias
		|| Capture.ShadowSlopeBias != ShadowView.ShadowSlopeBias)
	{
		return true;
	}

	const TArray<FAABB>& Changes = Scene.ShadowCasterChanges;
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(ShadowView.ViewProjection);
	for (const FAABB& Box : Changes)
	{
		if (IsAABBInFrustum(Box, Frustum))
			return true;
	}

	return false;
}

void FShadowRenderer::ResetCasterChanges()
{
	for (FScene* Scene : Scenes)
	{
		Scene->ShadowCasterChanges.Reset();
	}
}
