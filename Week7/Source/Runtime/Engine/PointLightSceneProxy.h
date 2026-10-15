#pragma once
#include "LightSceneProxy.h"
#include "Math/Vector4.h"


class UPointLightComponent;

class FPointLightSceneProxy : public FLightSceneProxy
{
public:
	FPointLightSceneProxy(const UPointLightComponent* Component);
	~FPointLightSceneProxy() = default;

	FPointLightSceneProxy(const FPointLightSceneProxy&) = delete;
	FPointLightSceneProxy& operator=(const FPointLightSceneProxy&) = delete;

	virtual void UpdateFromComponent(const ULightComponent* InComponent) override;
	virtual ELightType GetLightType() const override { return ELightType::Point; }

	// 빛이 닿는 거리
	float AttenuationRadius = 5.0f;

	// Inner에서 Outer로 가며 밝기가 떨어지는 정도.
	float Falloff = 1.0f;


	//TODO : 상위 Proxy로 빼야함
	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 0.6f, 1.0f);
	




};