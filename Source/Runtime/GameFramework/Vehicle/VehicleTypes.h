#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include <span>

enum class EVehicleCategory : uint8 { Speed, Balanced, Heavy };

struct FVehicleCategoryInfo
{
	const char* Id;
	const char* Label;
	float Mass;
	float SpeedWeight;
};

struct FVehicleModelEntry
{
	const char* Name;
	EVehicleCategory Category;
};

std::span<const FVehicleModelEntry> GetVehicleModelCatalog();
EVehicleCategory GetVehicleCategory(const FString& ModelName);
const FVehicleCategoryInfo& GetVehicleCategoryInfo(EVehicleCategory Category);

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
	float MaxForwardSpeed = 30.0f;	// u/s. 밸런스형의 기준 속도.
	float MaxReverseSpeed = 12.0f;
	float BoostMaxSpeed = 45.0f;
	float Acceleration = 22.5f;		// 속력을 높일 때 (u/s²)
	float Deceleration = 30.0f;		// 속력을 줄일 때: 키를 뗌, 반대 키, 부스트 끝
	float Grip = 10.0f;				// 옆 미끄러짐을 지우는 정도 (1/s)
	float TurnRate = 100.0f;		// 도/초
	float FullTurnSpeed = 7.5f;		// 이 속력부터 TurnRate로 돈다
	float JumpSpeed = 6.0f;			// 점프 순간 위로 더해지는 속도 (u/s)
};

FVehicleTuning MakeVehicleCategoryTuning(EVehicleCategory Category);
