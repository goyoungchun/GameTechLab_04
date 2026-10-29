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
	float BoostRechargeTime;
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
	float MaxReverseSpeed = 27.0f;	// 전진 최고속도의 90%
	float BoostMaxSpeed = 45.0f;
	float Acceleration = 22.5f;		// 속력을 높일 때 (u/s²)
	float Deceleration = 30.0f;		// 이전 Lua 프리셋 호환용. 입력을 떼면 관성으로 주행한다.
	float Grip = 10.0f;				// 지면에서 수평 속도를 차량 방향으로 정렬하는 강도 (1/s)
	float TurnRate = 120.0f;		// 기준 조향 속도 (도/초)
	float MaxSteeringAngle = 25.0f; // 앞바퀴 최대 표시 조향각 (도)
	float FullTurnSpeed = 7.5f;		// 이전 Lua 프리셋 호환용
	float SteeringSpeedScale = 100.0f; // 속력에 따른 조향 완화 기준. 0이면 완화하지 않는다.
	float BoostMultiplier = 2.0f;	// 부스트 시 수평 가속도 배율
	float JumpSpeed = 16.0f;			// 점프 순간 위로 더해지는 속도 (u/s)
};

FVehicleTuning MakeVehicleCategoryTuning(EVehicleCategory Category);
