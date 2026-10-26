#include "EnginePCH.h"
#include "VehicleMovementComponent.h"

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
}