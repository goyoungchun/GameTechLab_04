#pragma once

#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
	DECLARE_CLASS(USphereComponent, UShapeComponent)

public:
	USphereComponent() = default;
	virtual ~USphereComponent() override = default;
};
