#pragma once
#include "Math.h"

class UPrimitive
{
public:
	virtual ~UPrimitive() {}

	virtual void Update() = 0;
	virtual bool CheckCollision(UPrimitive* targetball) = 0;
	virtual void ResolveCollision(UPrimitive* targetball) = 0;

	virtual float GetRadius() = 0;
	virtual float GetMass() = 0;
	virtual FVector GetLocation() = 0;
	virtual FVector GetVelocity() = 0;
	virtual void SetVelocity(FVector vector) = 0;
	virtual void SetLocation(FVector vector) = 0;
	virtual void Gravity(float dt) = 0;
};