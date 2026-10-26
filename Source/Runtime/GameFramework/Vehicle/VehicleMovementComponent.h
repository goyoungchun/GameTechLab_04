#pragma once

#include "Component/ActorComponent.h"

class UVehicleMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UVehicleMovementComponent, UActorComponent)

public:
	UVehicleMovementComponent();
	virtual ~UVehicleMovementComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
};