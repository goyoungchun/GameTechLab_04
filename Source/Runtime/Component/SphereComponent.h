#pragma once

#include "ShapeComponent.h"

#include <algorithm>
#include <cmath>

class USphereComponent : public UShapeComponent
{
	DECLARE_CLASS(USphereComponent, UShapeComponent)
	REFLECT_START(USphereComponent)
		PROPERTY(SphereRadius)
	REFLECT_END()

public:
	USphereComponent() = default;
	virtual ~USphereComponent() override = default;

	float GetSphereRadius() const { return SphereRadius; }
	void SetSphereRadius(float InRadius) { SphereRadius = InRadius; OnPropertyChanged("SphereRadius"); }
	// 비균등 Scale에서도 구형을 유지한다. 예측 Pose의 Scale도 같은 규칙으로 계산한다.
	float GetScaledSphereRadius(const FVector& InScale) const
	{
		return SphereRadius * std::min({ std::abs(InScale.X), std::abs(InScale.Y), std::abs(InScale.Z) });
	}
	float GetCollisionRadius() const override { return GetScaledSphereRadius(GetWorldScale3D()); }

	void OnPropertyChanged(const FString& PropertyName) override;
	FBox CalcLocalBounds() const override;
	void DrawShape(FLineBatcher& LineBatcher) const override;

private:
	float SphereRadius = 0.5f;
};
