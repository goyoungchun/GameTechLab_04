#pragma once
#include "LightSceneProxy.h"
#include "Math/Vector4.h"


class USpotLightComponent;

class FSpotLightSceneProxy : public FLightSceneProxy
{
public:
	FSpotLightSceneProxy(const USpotLightComponent* Component);
	~FSpotLightSceneProxy() = default;

	FSpotLightSceneProxy(const FSpotLightSceneProxy&) = delete;
	FSpotLightSceneProxy& operator=(const FSpotLightSceneProxy&) = delete;

	virtual void UpdateFromComponent(const ULightComponent* InComponent) override;
	virtual ELightType GetLightType() const override { return ELightType::Spot; }

	// 원뿔 중심축 방향 (월드)
	FVector Direction;

	// 원뿔 중심축 기준 반각(도).
	float InnerConeAngle = 15.0f;
	float OuterConeAngle = 44.0f;

	// 빛이 닿는 거리
	float AttenuationRadius = 3.0f;

	// Inner에서 Outer로 가며 밝기가 떨어지는 정도.
	float Falloff = 1.0f;

	//TODO : 상위 Proxy로 빼야함
	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 0.6f, 1.0f);
};
