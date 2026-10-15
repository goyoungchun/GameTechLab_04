#pragma once

#include "SceneComponent.h"
#include "Render/LineBatcher.h"

class FLightSceneProxy;

class ULightComponent : public USceneComponent
{
	friend class FScene;              // 씬만 설정하도록

	DECLARE_CLASS(ULightComponent, USceneComponent)
	REFLECT_START(ULightComponent)
	REFLECT_END()
public:
		virtual void DrawDebug(FLineBatcher* LineBatcher) const = 0;
		virtual FLightSceneProxy* CreateSceneProxy() const = 0;
		FLightSceneProxy* GetSceneProxy() const { return SceneProxy; }

		// 값이 바뀌었음을 씬에 알린다. 다음 UpdateAllTransforms에서 프록시가 다시 복사한다.
		void MarkLightDirty();

		virtual void PostEditChangeProperty(const FProperty& Property) override;
		virtual void OnTransformDirty() override;
private:
	FLightSceneProxy* SceneProxy = nullptr;
};