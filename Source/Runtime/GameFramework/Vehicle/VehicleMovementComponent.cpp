#include "EnginePCH.h"
#include "VehicleMovementComponent.h"

namespace
{
	inline float ClampAxis(float Value)
	{
		return std::isfinite(Value) ? std::clamp(Value, -1.0f, 1.0f) : 0.0f;
	}
}

UVehicleMovementComponent::UVehicleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UVehicleMovementComponent::~UVehicleMovementComponent()
{
}


void UVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	const FVehicleInput Input = PendingInput;
	PendingInput = {};

	// Movement
}

void UVehicleMovementComponent::SetInput(float Throttle, float Steer, bool bBoost)
{
	PendingInput.Throttle = ClampAxis(Throttle);
	PendingInput.Steer = ClampAxis(Steer);
	PendingInput.bBoost = bBoost;
}

void UVehicleMovementComponent::RequestJump()
{
	PendingInput.bJumpRequested = true;
}