#pragma once

#include "LightComponent.h"

//언리얼에서 이런 컴포넌트는 존재하지 않음... 그나마 비슷한 거라면 skyLightComponent 인듯
class UAmbientLightComponent final : public ULightComponent
{
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
	REFLECT_START(UAmbientLightComponent)
		PROPERTY(Intensity)
		PROPERTY_TYPE(LightColor, Color)
	REFLECT_END()

public:
	UAmbientLightComponent() = default;
	~UAmbientLightComponent() = default;

	UAmbientLightComponent(const UAmbientLightComponent&) = delete;
	UAmbientLightComponent& operator=(const UAmbientLightComponent& ) = delete;

	// 위치/방향이 없어서 그릴 디버그 형상이 없다
	virtual void DrawDebug(FLineBatcher* LineBatcher) const override {}

	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; MarkLightDirty(); }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; MarkLightDirty(); }

	virtual FLightSceneProxy* CreateSceneProxy() const override;

private:
	//빛의 강도
	float Intensity = 0.2f;
	FVector4 LightColor = FVector4(1, 1, 1, 1);




};