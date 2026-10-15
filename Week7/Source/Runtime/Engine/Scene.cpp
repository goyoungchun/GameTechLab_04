#include "EnginePCH.h"
#include "Scene.h"
#include "AmbientLightSceneProxy.h"
#include "DirectionalLightSceneProxy.h"
#include "PointLightSceneProxy.h"
#include "SpotLightSceneProxy.h"

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = new FPrimitiveSceneProxy(Component);
	Proxy->Scene = this;
	Proxy->PackedIndex = Proxies.Num();
	Component->SceneProxy = Proxy;
	MarkDirty(Proxy);
	MarkRenderStateDirty(Proxy);

	Proxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	Proxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(Proxies.Num()))
		Proxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyProxies.Find(Proxy);   // 프로젝트 TArray의 Find 이름에 맞게
		if (Found != INDEX_NONE) DirtyProxies.RemoveAtSwap(Found);
	}

	if (Proxy->bRenderStateQueued)
	{
		const int32 Found = RenderStateDirtyProxies.Find(Proxy);
		if (Found != INDEX_NONE) RenderStateDirtyProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::RemoveAllPrimitives()
{
	// 하나씩 RemovePrimitive하면 대기열 검색(Find)이 물체 수만큼 반복되므로 통째로 비운다.
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (UPrimitiveComponent* Component = Proxy->GetComponent())
			Component->SceneProxy = nullptr;
		delete Proxy;
	}
	Proxies.Reset();
	PrimitiveBounds.Reset();
	PrimitiveFlags.Reset();
	DirtyProxies.Reset();
	RenderStateDirtyProxies.Reset();
	BVH.Clear();
	bElementListChanged = true;
}

void FScene::AddFogInfo(uint32 Id, const FFogInfo& FogInfo)
{
	FExponentialHeightFogSceneInfo sceneFogInfo;
	sceneFogInfo.Id = Id;
	sceneFogInfo.FogInfo = FogInfo;


	FogInfos.Add(sceneFogInfo);
}

void FScene::RemoveFogInfo(uint32 Id)
{
	// 렌더러는 FogInfos[0]만 쓰므로, 순서를 유지하려고 RemoveAtSwap 대신 RemoveAt을 쓴다.
	for (int32 i = 0; i < FogInfos.Num(); ++i)
	{
		if (FogInfos[i].Id == Id)
		{
			FogInfos.RemoveAt(i);
			return;
		}
	}
}

void FScene::RemoveAllFogInfo()
{
	FogInfos.Reset();

}

void FScene::UpdateFogInfo(uint32 Id, const FFogInfo& FogInfo)
{
	for (FExponentialHeightFogSceneInfo& SceneFogInfo : FogInfos)
	{
		if (SceneFogInfo.Id == Id)
		{
			SceneFogInfo.FogInfo = FogInfo;
			return;
		}


	}
}

//void FScene::RemoveFogInfo(uint32 Id)
//{
//	for (int32 i = 0; i < FogInfos.Num(); ++i)
//	{
//		if (FogInfos[i].Id == Id)
//		{
//			FogInfos.RemoveAtSwap(i);
//			return;
//		}
//	}
//}


void FScene::UpdateAllTransforms()
{
	for (FPrimitiveSceneProxy* Proxy : RenderStateDirtyProxies)
	{
		Proxy->UpdateRenderState();
		Proxy->bRenderStateQueued = false;
	}
	RenderStateDirtyProxies.Reset();

	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		Proxy->UpdateTransform();
		PrimitiveBounds[Proxy->PackedIndex] = Proxy->GetBounds();
		PrimitiveFlags[Proxy->PackedIndex] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
		Proxy->bQueuedForUpdate = false;
	}

	for (FLightSceneProxy* Proxy : DirtyLightProxies)
	{
		Proxy->UpdateFromComponent(Proxy->Component);
		Proxy->bQueuedForUpdate = false;
	}
	if (DirtyLightProxies.Num() > 0)
		bLightInfosDirty = true;
	DirtyLightProxies.Reset();
	
	//const int32 Count = Proxies.Num();
	//for (int32 i = 0; i < Count; ++i)
	//{
	//	FPrimitiveSceneProxy* Proxy = Proxies[i];
	//	Proxy->UpdateTransform();

	//	const FAABB& Bounds = Proxy->GetBounds();
	//	if (PrimitiveBounds[i].Center != Bounds.Center || PrimitiveBounds[i].Extent != Bounds.Extent)
	//	{
	//		bBoundsChanged = true;
	//	}

	//	PrimitiveBounds[i] = Proxy->GetBounds();
	//	PrimitiveFlags[i] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
	//}

	const bool bAnyMoved = DirtyProxies.Num() > 0;
	DirtyProxies.Reset();
	

	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bAnyMoved)
	{
		BVH.Refit();
	}
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<FPrimitiveSceneProxy*> Elements;
	Elements.Reserve(Proxies.Num());
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy && Proxy->GetComponent())
			Elements.Add(Proxy);
	}

	BVH.Build(std::span(Elements.GetData(), Elements.Num()));
}

void FScene::MarkDirty(FPrimitiveSceneProxy* Proxy)
{
	if (Proxy->bQueuedForUpdate) return;            // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyProxies.Add(Proxy);
}

void FScene::MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->bRenderStateQueued) return;
	Proxy->bRenderStateQueued = true;
	RenderStateDirtyProxies.Add(Proxy);
}


// 라이트가 바뀐 프레임에만 프록시를 GPU 형식으로 다시 모으고, 캐시를 렌더러에 넘긴다.
void FScene::UpdateLights(FRenderer* Renderer)
{
	if (!Renderer)
		return;

	if (bLightInfosDirty)
	{
		RebuildLightInfos();
		bLightInfosDirty = false;

		// 씬마다 0부터 세면, 지워진 월드 자리에 새 월드가 같은 주소로 생겼을 때 렌더러가 같은 버전으로 착각한다.
		// 모든 씬이 공유하는 카운터로 버전을 매겨 겹치지 않게 한다.
		static uint32 LightInfosVersionCounter = 0;
		LightInfosVersion = ++LightInfosVersionCounter;
	}

	Renderer->SetLights(this, LightInfosVersion, LightConstants, PointLightInfos, SpotLightInfos);
}

// 라이트 프록시를 종류별로 모아 GPU 형식으로 바꾼다. 컴포넌트는 읽지 않는다.
void FScene::RebuildLightInfos()
{
	// 멤버라 지난 값이 남으므로 반드시 비운다 (지운 Directional이 남지 않게)
	LightConstants = {};
	PointLightInfos.Reset();
	SpotLightInfos.Reset();

	FVector AmbientSum(0.0f, 0.0f, 0.0f);
	bool bHasDirectional = false;

	for (const FLightSceneProxy* Proxy : LightProxies)
	{
		switch (Proxy->GetLightType())
		{
		case ELightType::Ambient:
		{
			// 여러 개면 더한다
			const FAmbientLightSceneProxy* Ambient = static_cast<const FAmbientLightSceneProxy*>(Proxy);
			const float Intensity = std::max(Ambient->Intensity, 0.0f);
			AmbientSum.X += Ambient->LightColor.X * Intensity;
			AmbientSum.Y += Ambient->LightColor.Y * Intensity;
			AmbientSum.Z += Ambient->LightColor.Z * Intensity;
			break;
		}
		case ELightType::Directional:
		{
			// 셰이더는 Directional을 하나만 받으므로 첫 번째만 쓴다
			if (bHasDirectional)
				break;
			bHasDirectional = true;

			const FDirectionalLightSceneProxy* Directional = static_cast<const FDirectionalLightSceneProxy*>(Proxy);
			const FVector4& Color = Directional->LightColor;
			LightConstants.Directional.ColorIntensity = FVector4(Color.X, Color.Y, Color.Z, std::max(Directional->Intensity, 0.0f));
			LightConstants.Directional.Direction = FVector4(Directional->Direction.X, Directional->Direction.Y, Directional->Direction.Z, 0.0f);
			break;
		}
		case ELightType::Point:
		{
			const FPointLightSceneProxy* Point = static_cast<const FPointLightSceneProxy*>(Proxy);
			const FVector4& Color = Point->LightColor;

			FPointLightInfo Info{};
			Info.ColorIntensity = FVector4(Color.X, Color.Y, Color.Z, std::max(Point->Intensity, 0.0f));
			Info.Position = Point->Position;
			// 셰이더가 Dist / Radius와 pow(x, Falloff)를 하므로 0을 막는다
			Info.AttenuationRadius = std::max(Point->AttenuationRadius, 0.001f);
			Info.Falloff = std::max(Point->Falloff, 0.001f);
			PointLightInfos.Add(Info);
			break;
		}
		case ELightType::Spot:
		{
			const FSpotLightSceneProxy* Spot = static_cast<const FSpotLightSceneProxy*>(Proxy);
			const FVector4& Color = Spot->LightColor;

			// 에디터 값은 신뢰할 수 없으므로 DrawDebug와 같은 범위로 자른다
			const float OuterAngle = FMath::Clamp(Spot->OuterConeAngle, 0.0f, 89.0f);
			const float InnerAngle = FMath::Clamp(Spot->InnerConeAngle, 0.0f, OuterAngle);

			FSpotLightInfo Info{};
			Info.ColorIntensity = FVector4(Color.X, Color.Y, Color.Z, std::max(Spot->Intensity, 0.0f));
			Info.Position = Spot->Position;
			Info.AttenuationRadius = std::max(Spot->AttenuationRadius, 0.001f);
			Info.Direction = Spot->Direction;
			Info.Falloff = std::max(Spot->Falloff, 0.001f);
			Info.CosOuter = std::cos(FMath::DegreesToRadians(OuterAngle));
			// smoothstep(CosOuter, CosInner)는 두 값이 같으면 0으로 나누므로 최소 간격을 둔다
			Info.CosInner = std::max(std::cos(FMath::DegreesToRadians(InnerAngle)), Info.CosOuter + 0.0001f);
			SpotLightInfos.Add(Info);
			break;
		}
		}
	}

	LightConstants.Ambient.ColorIntensity = FVector4(AmbientSum.X, AmbientSum.Y, AmbientSum.Z, 1.0f);
	LightConstants.PointLightCount = static_cast<uint32>(PointLightInfos.Num());
	LightConstants.SpotLightCount = static_cast<uint32>(SpotLightInfos.Num());
}

void FScene::UpdateDirectionalLight(FRenderer* Renderer)
{
	if (!Renderer)
		return;

	FDirectionalLight Data{};
	FDirectionalLightConstants Constants{};

	Constants.LightCount = 0;

	Data.Direction = FVector4(0, 0, 0, 0);
	Data.ColorIntensity = FVector4(0, 0, 0, 0);
	Data.EXP = FVector4(1, 0, 0, 0);

	for (UDirectionalLightComponent* DirectLight : DirectionalLightComponents)
	{
		if (Constants.LightCount >= MaxDirectLights)
		{
			break;
		}

		if (!DirectLight)
			continue;

		const FRotator Direct = DirectLight->GetWorldRotation().Quaternion().RotateVector(FVector(1.0f,0.0f,0.0f)).Normalized();
		const FVector4 Color = DirectLight->GetLightColor();

		Data.Direction = FVector4(
			Direct.Pitch,
			Direct.Yaw,
			Direct.Roll,
			1.0f
		);

		Data.ColorIntensity = FVector4(
			Color.X,
			Color.Y,
			Color.Z,
			std::max<float>(DirectLight->GetIntensity(), 0.0f)
		);

		Constants.Light[Constants.LightCount] = Data;
		Constants.LightCount++;
	}

	Renderer->SetDirectionalLight(Constants);
}
void FScene::RegisterDirectLight(UDirectionalLightComponent* DLightComp)
{
	if (!DLightComp)
		return;

	for (UDirectionalLightComponent* Existing : DirectionalLightComponents)
	{
		if (Existing == DLightComp)
			return;
	}
	DirectionalLightComponents.Add(DLightComp);
}
void FScene::UnregisterDirectLight(UDirectionalLightComponent* DLightComp)
{
	for (uint32 i = 0; i < DirectionalLightComponents.Num(); ++i)
	{
		if (DirectionalLightComponents[i] == DLightComp)
		{
			DirectionalLightComponents.RemoveAt(i, 1);
			return;
		}
	}
}

void FScene::AddLight(ULightComponent* Component)
{
	if (!Component || Component->SceneProxy) return;   // 이미 등록됨

	FLightSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy) return;

	Proxy->Scene = this;
	Proxy->Component = Component;
	Proxy->PackedIndex = LightProxies.Num();
	Component->SceneProxy = Proxy;

	LightProxies.Add(Proxy);
	bLightInfosDirty = true;
}

void FScene::RemoveLight(ULightComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FLightSceneProxy* Proxy = Component->SceneProxy;
	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	// 마지막 원소를 빈자리로 옮기고, 옮겨진 프록시의 인덱스를 갱신
	LightProxies.RemoveAtSwap(Index);
	if (Index < static_cast<uint32>(LightProxies.Num()))
		LightProxies[Index]->PackedIndex = static_cast<int32>(Index);

	if (Proxy->bQueuedForUpdate)
	{
		const int32 Found = DirtyLightProxies.Find(Proxy);
		if (Found != INDEX_NONE) DirtyLightProxies.RemoveAtSwap(Found);
	}

	Component->SceneProxy = nullptr;
	delete Proxy;
	bLightInfosDirty = true;
}

void FScene::RemoveAllLights()
{
	for (FLightSceneProxy* Proxy : LightProxies)
	{
		if (Proxy->Component)
			Proxy->Component->SceneProxy = nullptr;
		delete Proxy;
	}
	LightProxies.Reset();
	DirtyLightProxies.Reset();
	bLightInfosDirty = true;
}

void FScene::MarkLightDirty(FLightSceneProxy* Proxy)
{
	if (!Proxy || Proxy->bQueuedForUpdate) return;   // 중복 추가 방지
	Proxy->bQueuedForUpdate = true;
	DirtyLightProxies.Add(Proxy);
}
