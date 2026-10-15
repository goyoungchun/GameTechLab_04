#pragma once
#include "LightSceneProxy.h"
#include "Math/Vector4.h"


class UDirectionalLightComponent;

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	FDirectionalLightSceneProxy(const UDirectionalLightComponent* Component);
	~FDirectionalLightSceneProxy() = default;

	FDirectionalLightSceneProxy(const FDirectionalLightSceneProxy&) = delete;
	FDirectionalLightSceneProxy& operator=(const FDirectionalLightSceneProxy&) = delete;

	virtual void UpdateFromComponent(const ULightComponent* InComponent) override;
	virtual ELightType GetLightType() const override { return ELightType::Directional; }

	// 빛이 향하는 방향 (월드). 위치는 의미 없음
	FVector Direction;

	//TODO : 상위 Proxy로 빼야함
	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};
