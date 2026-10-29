#pragma once

#include "PrimitiveComponent.h"

class FLineBatcher;

class UShapeComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)
	REFLECT_START(UShapeComponent)
		PROPERTY_TYPE(ShapeColor, Color)
		PROPERTY(bDrawShape)
	REFLECT_END()

public:
	UShapeComponent() = default;
	virtual ~UShapeComponent() override = default;

	const FVector4& GetShapeColor() const { return ShapeColor; }
	void SetShapeColor(const FVector4& InColor) { ShapeColor = InColor; OnPropertyChanged("ShapeColor"); }
	bool GetDrawShape() const { return bDrawShape; }
	void SetDrawShape(bool bInDrawShape) { bDrawShape = bInDrawShape; OnPropertyChanged("bDrawShape"); }

	virtual void DrawShape(FLineBatcher& LineBatcher) const = 0;

private:
	FVector4 ShapeColor{ 0.0f, 1.0f, 0.0f, 1.0f };
	bool bDrawShape = true;
};
