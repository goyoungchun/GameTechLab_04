#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)
	REFLECT_START(UCapsuleComponent)
		PROPERTY(CapsuleRadius)
		PROPERTY(CapsuleHalfHeight)
	REFLECT_END()

public:
	UCapsuleComponent() = default;
	virtual ~UCapsuleComponent() override = default;

	float GetCapsuleRadius() const { return CapsuleRadius; }
	void SetCapsuleRadius(float InRadius) { CapsuleRadius = InRadius; OnPropertyChanged("CapsuleRadius"); }
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	void SetCapsuleHalfHeight(float InHalfHeight) { CapsuleHalfHeight = InHalfHeight; OnPropertyChanged("CapsuleHalfHeight"); }
	void SetCapsuleSize(float InRadius, float InHalfHeight)
	{
		CapsuleRadius = InRadius;
		CapsuleHalfHeight = InHalfHeight;
		OnPropertyChanged("CapsuleSize");
	}

	void OnPropertyChanged(const FString& PropertyName) override;
	FBox CalcLocalBounds() const override;
	void DrawShape(FLineBatcher& LineBatcher) const override;

private:
	float CapsuleRadius = 0.5f;
	// 로컬 Z축 방향이며 반구를 포함한 전체 높이의 절반이다. HalfHeight >= Radius.
	float CapsuleHalfHeight = 1.0f;
};
