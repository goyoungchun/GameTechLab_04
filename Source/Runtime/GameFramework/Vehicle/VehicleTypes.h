#pragma once

#include "Core/Types.h"

struct FVehicleInput
{
	float Throttle = 0.0f; 
	float Steer = 0.0f; 
	bool bBoost = false; 
	bool bJumpRequested = false;
};

enum class EVehicleMoveMode : uint8
{
	Grounded,
	Airborne
};