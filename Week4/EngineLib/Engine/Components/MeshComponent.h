#pragma once

#include "PrimitiveComponent.h"

// What is the role of UMeshComponent in current faze?
class UMeshComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(UMeshComponent, UPrimitiveComponent)
	DECLARE_SERIALIZATION()
};
