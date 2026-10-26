#include "EnginePCH.h"
#include "VehicleMovementComponent.h"
#include "Component/BoxComponent.h"
#include "GameFramework/Actor.h"

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

	if (UBoxComponent* Body = GetBody())
	{
		BodyHitHandle = Body->OnComponentHit.AddRaw(this, &UVehicleMovementComponent::HandleBodyHit);
		PreviousLocation = Body->GetWorldLocation();
		bHasPreviousLocation = true;
	}
}

void UVehicleMovementComponent::EndPlay()
{
	if (UBoxComponent* Body = GetBody())
		Body->OnComponentHit.Remove(BodyHitHandle);

	Super::EndPlay();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	const FVehicleInput Input = PendingInput;
	PendingInput = {};

	UpdateVelocityEstimate(DeltaTime);
	UpdateGround(DeltaTime);
}

UBoxComponent* UVehicleMovementComponent::GetBody() const
{
	AActor* Owner = GetOwner();

	if (!Owner)
		return nullptr;

	return Cast<UBoxComponent>(Owner->GetRootComponent());
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

void UVehicleMovementComponent::UpdateVelocityEstimate(float DeltaTime)
{
	const UBoxComponent* Body = GetBody();
	if (!Body)
		return;

	const FVector Location = Body->GetWorldLocation();
	if (bHasPreviousLocation && PreviousDeltaTime > 0.0f)
	{
		const FVector Estimated = (Location - PreviousLocation) / PreviousDeltaTime;
		Velocity = Estimated.Size() <= MaxTeleportSpeed ? Estimated : FVector::ZeroVector;
	}

	PreviousLocation = Location;
	PreviousDeltaTime = DeltaTime;
	bHasPreviousLocation = true;
}

void UVehicleMovementComponent::HandleBodyHit(
	UPrimitiveComponent* Self,
	AActor* OtherActor,
	UPrimitiveComponent* Other,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	// Hit.Normal은 해당 차량이 밀려나는 방향
	// 위쪽이면 무언가가 받치고 있는 상태
	if (Hit.Normal.Z >= std::cos(FMath::DegreesToRadians(MaxGroundSlopeDegrees)))
		bGroundContactThisStep = true;
}

void UVehicleMovementComponent::UpdateGround(float DeltaTime)
{
	bLandedThisFrame = false;

	if (bWaitingFirstStep)
	{
		bWaitingFirstStep = false;
		return;
	}

	const bool bGrounded = bGroundContactThisStep;
	bGroundContactThisStep = false;

	if (bGrounded)
	{
		if (bGroundStateValid && MoveMode == EVehicleMoveMode::Airborne)
		{
			bLandedThisFrame = true;
			LastLandingSpeed = std::max(0.0f, -LastAirborneVerticalSpeed);
			OnLanded.Broadcast(LastLandingSpeed);
		}
		MoveMode = EVehicleMoveMode::Grounded;
		TimeSinceGrounded = 0.0f;
	}
	else
	{
		MoveMode = EVehicleMoveMode::Airborne;
		TimeSinceGrounded += DeltaTime;
		LastAirborneVerticalSpeed = Velocity.Z;
	}

	bGroundStateValid = true;
}

float UVehicleMovementComponent::GetSpeed() const
{
	return FVector(Velocity.X, Velocity.Y, 0.0f).Size();
}

float  UVehicleMovementComponent::GetForwardSpeed() const
{
	const UBoxComponent* Body = GetBody();

	if (!Body)
		return 0.0f;

	return Velocity.Dot(Body->GetWorldRotation().Quaternion().GetForwardVector());
}