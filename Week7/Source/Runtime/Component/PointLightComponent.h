#pragma once

#include "component/LightComponent.h"

class FLineBatcher;
class FLightSceneProxy;

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

	REFLECT_START(ClassName)
		PROPERTY(AttenuationRadius)
		PROPERTY(Falloff)
		PROPERTY(Intensity)
		PROPERTY_TYPE(LightColor, Color)
	REFLECT_END()

public:
	UPointLightComponent() = default;
	virtual ~UPointLightComponent() override = default;

	void DrawDebug(FLineBatcher* LineBatcher) const override;


	float GetFalloff() const { return Falloff; }
	void SetFalloff(float InFalloff) { Falloff = InFalloff; MarkLightDirty(); }

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; MarkLightDirty(); }

	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; MarkLightDirty(); }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; MarkLightDirty(); }


	virtual FLightSceneProxy* CreateSceneProxy() const override;


private:
	// 빛이 닿는 거리
	float AttenuationRadius = 5.0f;

	// Inner에서 Outer로 가며 밝기가 떨어지는 정도.
	float Falloff = 1.0f;

	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 0.6f, 1.0f);
};
