#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

public:
	UCapsuleComponent() = default;
	virtual ~UCapsuleComponent() override = default;
};
