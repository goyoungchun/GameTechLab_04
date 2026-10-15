#pragma once

#include "PrimitiveSceneProxy.h"
#include "LightSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Render/FogInfo.h"
#include "Render/Renderer.h"

struct FExponentialHeightFogSceneInfo
{
	uint32 Id;
	FFogInfo FogInfo;
};


class FScene
{
public:
	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);
	// 모든 프록시를 한 번에 지운다. 액터를 통째로 지우기 전(ClearWorld)에 불러야 지워진 컴포넌트를 가리키는 프록시가 남지 않는다.
	void RemoveAllPrimitives();

	void AddFogInfo(uint32 Id, const FFogInfo& FogInfo);
	void RemoveFogInfo(uint32 Id);
	void RemoveAllFogInfo();
	void UpdateFogInfo(uint32 Id, const FFogInfo& FogInfo);
	void UpdateAllTransforms();

	void BuildBVH();

	void MarkDirty(FPrimitiveSceneProxy* Proxy);
	void MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy);

	// Todo : LightComponent로 묶게 되면서 상위 클래스에서 해당 함수를 통일하게 할 것인가?

	// 라이트 프록시를 GPU 형식으로 모아 렌더러에 올린다. 렌더링 직전, 뷰마다 호출한다.
	void UpdateLights(FRenderer* Renderer);
	// 라이트 프록시를 업로드용 Info 캐시로 다시 만든다. bLightInfosDirty일 때만 UpdateLights가 부른다.
	void RebuildLightInfos();

	void UpdateDirectionalLight(FRenderer* Renderer);
	void RegisterDirectLight(UDirectionalLightComponent* DLightComp);
	void UnregisterDirectLight(UDirectionalLightComponent* DLightComp);

	void AddLight(ULightComponent* Component);
	void RemoveLight(ULightComponent* Component);
	// 모든 프록시를 한 번에 지운다. 액터를 통째로 지우기 전(ClearWorld)에 불러야 지워진 컴포넌트를 가리키는 프록시가 남지 않는다.
	void RemoveAllLights();
	void MarkLightDirty(FLightSceneProxy* Proxy);

	TArray<FPrimitiveSceneProxy*> Proxies;
	TArray<FPrimitiveSceneProxy*> DirtyProxies;
	TArray<FPrimitiveSceneProxy*> RenderStateDirtyProxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;
	
	TArray<FExponentialHeightFogSceneInfo> FogInfos;

	// 컬링 결과로 프록시를 바로 내보내 컴포넌트를 역참조하지 않는다. 경계는 Build/Refit 때만 계산한다.
	TBVH<FPrimitiveSceneProxy*> BVH{
		[](const FPrimitiveSceneProxy* Proxy) -> FBox
		{
			const FAABB& B = Proxy->GetBounds();
			return FBox{ B.Center - B.Extent, B.Center + B.Extent };
		}
	};
	bool bElementListChanged = false;
	

	TArray<UDirectionalLightComponent*> DirectionalLightComponents;

	TArray<FLightSceneProxy*> LightProxies;
	//TODO: Dirty를 두는 방식도 Thread Safe하지않음 여긴 게임,렌더 스레드가 둘다 접근할 수 있는 공간 원래는 컴포넌트가 더티를 표시해서 바꿔야함
	//근데 멀티 스레드를 도입할것도 아닌데 왜 이런 고민을...? 그럼 그냥 최적화 용도로 쓰는걸로 
	TArray<FLightSceneProxy*> DirtyLightProxies;

	// 프록시 그대로를 버퍼에 올리기엔 필요없는 정보가많음 ex)컴포넌트 포인트 라던가,씬 포인터 라던가 그래서 버퍼 올리기용 Info를 만듬
	// RebuildLightInfos가 채우는 업로드용 캐시. 라이트가 한 개도 안 바뀐 프레임엔 그대로 다시 쓴다. 한개라도 바뀌었으면 Info를 다시 다 만듬
	//사실상 실패한 설계...
	TArray<FPointLightInfo> PointLightInfos;
	TArray<FSpotLightInfo> SpotLightInfos;
	FLightConstants LightConstants{};

	// 라이트 추가/삭제/갱신이 있었는지. 꺼져 있으면 UpdateLights가 위 캐시를 그대로 다시 올린다.
	bool bLightInfosDirty = true;
	// 캐시를 다시 만들 때마다 모든 씬 공통 카운터로 새 값을 받는다. 렌더러가 같은 씬·같은 버전이면 업로드를 건너뛰는 데 쓴다.
	uint32 LightInfosVersion = 0;

};