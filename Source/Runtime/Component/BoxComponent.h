#pragma once

#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent
{
	DECLARE_CLASS(UBoxComponent, UShapeComponent)
	REFLECT_START(UBoxComponent)
		PROPERTY(BoxExtent)
	REFLECT_END()

public:
	UBoxComponent() = default;
	virtual ~UBoxComponent() override = default;

	const FVector& GetBoxExtent() const { return BoxExtent; }
	void SetBoxExtent(const FVector& InExtent) { BoxExtent = InExtent; OnPropertyChanged("BoxExtent"); }

	void OnPropertyChanged(const FString& PropertyName) override;
	FBox CalcLocalBounds() const override;
	void DrawShape(FLineBatcher& LineBatcher) const override;

private:
	// 로컬 좌표에서 중심으로부터 각 면까지의 거리. 전체 크기는 Extent * 2다.
	FVector BoxExtent{ 0.5f, 0.5f, 0.5f };
};
