#pragma once
#include "LightSceneProxy.h"
#include "Math/Vector4.h"


class UAmbientLightComponent;

class FAmbientLightSceneProxy : public FLightSceneProxy
{
public:
	FAmbientLightSceneProxy(const UAmbientLightComponent* Component);
	~FAmbientLightSceneProxy() = default;

	FAmbientLightSceneProxy(const FAmbientLightSceneProxy&) = delete;
	FAmbientLightSceneProxy& operator=(const FAmbientLightSceneProxy&) = delete;

	virtual void UpdateFromComponent(const ULightComponent* InComponent) override;
	virtual ELightType GetLightType() const override { return ELightType::Ambient; }

	// 위치/방향 없이 전역으로 더해지는 빛

	//TODO : 상위 Proxy로 빼야함
	float Intensity = 0.2f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};
