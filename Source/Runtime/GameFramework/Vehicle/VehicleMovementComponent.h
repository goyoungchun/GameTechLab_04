#pragma once

#include "Component/ActorComponent.h"
#include "VehicleTypes.h"

class UBoxComponent;
class UPrimitiveComponent;
class AActor;
struct FHitResult;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVehicleLanded, float);

class UVehicleMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UVehicleMovementComponent, UActorComponent)

public:
	UVehicleMovementComponent();
	virtual ~UVehicleMovementComponent() override;

	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	void SetInput(float Throttle, float Steer, bool bBoost);
	void RequestJump();

	const FVector& GetVelocity() const { return Velocity; }
	float GetSpeed() const;	
	float GetForwardSpeed() const;
	float GetTimeSinceGrounded() const { return TimeSinceGrounded; }
	float GetLastLandingSpeed() const { return LastLandingSpeed; }
	bool IsGrounded() const { return MoveMode == EVehicleMoveMode::Grounded; }
	bool HasLandedThisFrame() const { return bLandedThisFrame; }

public:
	FOnVehicleLanded OnLanded;

private:
	UBoxComponent* GetBody() const;
	void UpdateVelocityEstimate(float DeltaTime);
	void UpdateGround(float DeltaTime);
	void HandleBodyHit(
		UPrimitiveComponent* Self,
		AActor* OtherActor,
		UPrimitiveComponent* Other,
		FVector NormalImpulse,
		const FHitResult& Hit
	);
	void UpdateSteering(const FVehicleInput& Input, float DeltaTime);

private:
	FVehicleInput PendingInput;

	FVector Velocity = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	float PreviousDeltaTime = 0.0f;
	bool bHasPreviousLocation = false;
	
	FDelegateHandle BodyHitHandle;
	bool bGroundContactThisStep = false;
	bool bWaitingFirstStep = true;

	EVehicleMoveMode MoveMode = EVehicleMoveMode::Airborne;
	bool bGroundStateValid = false;
	bool bLandedThisFrame = false;
	float TimeSinceGrounded = 0.0f;
	float LastLandingSpeed = 0.0f;
	float LastAirborneVerticalSpeed = 0.0f;

	float MaxGroundSlopeDegrees = 45.0f;
	float MaxTeleportSpeed = 100.0f;

	float TurnRate = 100.0f;
	float FullTurnSpeed = 5.0f;
};