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
	bool IsBoosting() const { return bBoosting; }
	float GetSteeringInput() const { return SteeringInput; }
	void EndPlay() override;

	const FVehicleTuning& GetTuning() const { return Tuning; }
	void SetTuning(const FVehicleTuning& InTuning);

	void ResetMotion();

	void SetBoostCapacity(float Seconds);
	void SetBoostRechargeTime(float Seconds);
	void RefillBoost() { BoostRemaining = BoostCapacity; }
	void SetInfiniteBoost(bool bInfinite) { bInfiniteBoost = bInfinite; }
	bool IsInfiniteBoost() const { return bInfiniteBoost; }
	float GetBoostRatio() const;

public:
	FOnVehicleLanded OnLanded;

private:
	UBoxComponent* GetBody() const;
	void UpdateVelocityFromPhysics();
	void UpdateGround(float DeltaTime);
	void HandleBodyHit(
		UPrimitiveComponent* Self,
		AActor* OtherActor,
		UPrimitiveComponent* Other,
		FVector NormalImpulse,
		const FHitResult& Hit
	);
	void UpdateSteering(const FVehicleInput& Input, float DeltaTime);
	void UpdateDrive(const FVehicleInput& Input, float DeltaTime, const FVector& PreviousForward);

private:
	FVehicleInput PendingInput;
	bool bBoosting = false;
	float SteeringInput = 0.0f;

	FVector Velocity = FVector::ZeroVector;
	bool bSkipVelocityRead = false;
	
	FDelegateHandle BodyHitHandle;
	bool bGroundContactThisStep = false;

	EVehicleMoveMode MoveMode = EVehicleMoveMode::Airborne;
	bool bGroundStateValid = false;
	bool bLandedThisFrame = false;
	float TimeSinceGrounded = 0.0f;
	float LastLandingSpeed = 0.0f;
	float LastAirborneVerticalSpeed = 0.0f;

	// 잔량은 연속 사용 가능한 초 단위, 충전 시간은 빈 상태에서 완충까지 걸리는 시간이다.
	float BoostCapacity = 6.0f;
	float BoostRemaining = 6.0f;
	float BoostRechargeTime = 30.0f;
	bool bInfiniteBoost = true;

	float MaxGroundSlopeDegrees = 45.0f;
	FVehicleTuning Tuning;
};
