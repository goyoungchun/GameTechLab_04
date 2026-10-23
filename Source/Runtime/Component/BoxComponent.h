#pragma once

#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent
{
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

public:
	UBoxComponent() = default;
	virtual ~UBoxComponent() override = default;
};
