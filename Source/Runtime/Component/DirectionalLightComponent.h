#pragma once

#include "Component/LightComponent.h"

// 모든 위치에서 같은 방향으로 진행하며 거리 감쇠가 없는 조명.
class UDirectionalLightComponent : public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	REFLECT_START(UDirectionalLightComponent)
		PROPERTY_RANGE(CascadeCount, 1, 8)
		PROPERTY_RANGE(CascadeExponential, 1, 8)
		PROPERTY_RANGE(CascadeBlendRange, 0.0f, 100.0f)
		PROPERTY_RANGE(ShadowAngularDiameter, 0.0f, 10.0f)
		PROPERTY(bCascadeDebug)
	REFLECT_END()

public:
	UDirectionalLightComponent();
	void OnPropertyChanged(const FString& PropertyName) override;
	static uint32 GetSharedShadowMapResolution() { return static_cast<uint32>(SharedShadowMapResolution); }

	ELightType GetLightType() const override { return ELightType::Directional; }

	// 로컬 +X를 광선 진행 방향으로 사용한다. 조명 식의 L은 이 방향의 반대다.
	FVector GetLightDirection() const
	{
		FTransform WorldTransform;
		WorldTransform.Rotation = GetWorldRotation();
		return WorldTransform.GetForward();
	}

	////////////////////////////////////////////////////////////
	// Cascade Shadow Map
	////////////////////////////////////////////////////////////

	uint32 GetCascadeCount() const { return CascadeCount; }
	void SetCascadeCount(uint32 Count);

	uint32 GetCascadeExponential() const { return CascadeExponential; }
	void SetCascadeExponential(uint32 Exponential);

	float GetCascadeBlendRange() const { return CascadeBlendRange; }
	void SetCascadeBlendRange(float BlendRange);

	bool GetCascadeDebug() const { return bCascadeDebug; }
	void SetCascadeDebug(bool Debug);

	float GetShadowAngularDiameter() const { return ShadowAngularDiameter; }

private:
	inline static EShadowMapResolution SharedShadowMapResolution = EShadowMapResolution::ShadowMapResolution1024;

	uint32 CascadeCount = 5;
	uint32 CascadeExponential = 3;

	float CascadeBlendRange = 5.0f;
	float ShadowAngularDiameter = 2.0f;

	bool bCascadeDebug = false;
	
};
