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

struct FVehicleTuning
{
	float MaxForwardSpeed = 20.0f;	// u/s
	float MaxReverseSpeed = 8.0f;
	float BoostMaxSpeed = 30.0f;
	float Acceleration = 15.0f;		// 속력을 높일 때 (u/s²)
	float Deceleration = 20.0f;		// 속력을 줄일 때: 키를 뗌, 반대 키, 부스트 끝
	float Grip = 10.0f;				// 옆 미끄러짐을 지우는 정도 (1/s)
	float TurnRate = 100.0f;		// 도/초
	float FullTurnSpeed = 5.0f;		// 이 속력부터 TurnRate로 돈다
	float JumpSpeed = 6.0f;			// 점프 순간 위로 더해지는 속도 (u/s)
};