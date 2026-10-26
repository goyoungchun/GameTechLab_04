#pragma once

#include "Component/ActorComponent.h"
#include "VehicleTypes.h"

class UVehicleMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UVehicleMovementComponent, UActorComponent)

public:
	UVehicleMovementComponent();
	virtual ~UVehicleMovementComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	void SetInput(float Throttle, float Steer, bool bBoost);
	void RequestJump();

private:
	FVehicleInput PendingInput;
};