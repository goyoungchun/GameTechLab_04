#include "EnginePCH.h"
#include "VehicleMovementComponent.h"
#include "Component/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "VehicleTypes.h"
#include "Engine/World.h"
#include "Collision/FPhysSceneChaos.h"

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
	SteeringInput = 0.0f;
	if (UBoxComponent* Body = GetBody())
		Body->OnComponentHit.Remove(BodyHitHandle);

	Super::EndPlay();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
		return;

	FVehicleInput Input = PendingInput;
	PendingInput = {};
	const bool bBoostRequested = Input.bBoost && Input.Throttle > 0.0f;
	bBoosting = bBoostRequested && (bInfiniteBoost || BoostRemaining > 0.0f);
	Input.bBoost = bBoosting;
	if (!bInfiniteBoost)
	{
		if (bBoosting)
			BoostRemaining = std::max(0.0f, BoostRemaining - DeltaTime);
		else if (!bBoostRequested)
			BoostRemaining = std::min(BoostCapacity, BoostRemaining + BoostCapacity / BoostRechargeTime * DeltaTime);
		// 소진된 채 키를 계속 누르면 충전과 사용이 매 프레임 번갈아 일어나지 않게 한다.
	}
	SteeringInput = Input.Steer;

	UpdateVelocityEstimate(DeltaTime);
	UpdateGround(DeltaTime);
	UpdateSteering(Input, DeltaTime);
	UpdateDrive(Input, DeltaTime);
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

void UVehicleMovementComponent::SetTuning(const FVehicleTuning& InTuning)
{
	Tuning = InTuning;

	for (float* Value : { &Tuning.MaxForwardSpeed, &Tuning.MaxReverseSpeed, &Tuning.BoostMaxSpeed,
		&Tuning.Acceleration, &Tuning.Deceleration, &Tuning.Grip,
		&Tuning.TurnRate, &Tuning.FullTurnSpeed, &Tuning.JumpSpeed })
	{
		*Value = std::isfinite(*Value) ? std::max(0.0f, *Value) : 0.0f;
	}

	Tuning.BoostMaxSpeed = std::max(Tuning.BoostMaxSpeed, Tuning.MaxForwardSpeed);

	// 속도 추정은 MaxTeleportSpeed보다 빠른 이동을 순간이동으로 보고 버린다.
	if (Tuning.BoostMaxSpeed >= MaxTeleportSpeed)
	{
		HTR_LOG(Warning, "[Vehicle] BoostMaxSpeed({}) >= MaxTeleportSpeed({}): speed estimate will drop to 0",
			Tuning.BoostMaxSpeed, MaxTeleportSpeed);
	}
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

void UVehicleMovementComponent::UpdateSteering(const FVehicleInput& Input, float DeltaTime)
{
	UBoxComponent* Body = GetBody();
	if (!Body || Input.Steer == 0.0f || !IsGrounded())
		return;

	const float ForwardSpeed = GetForwardSpeed();
	const float SpeedRatio = Tuning.FullTurnSpeed > 0.0f
		? std::clamp(std::abs(ForwardSpeed) / Tuning.FullTurnSpeed, 0.0f, 1.0f)
		: 1.0f;
	const float Direction = ForwardSpeed < 0.0f ? -1.0f : 1.0f;
	const float YawDelta = Input.Steer * Tuning.TurnRate * SpeedRatio * Direction * DeltaTime;
	if (YawDelta == 0.0f)
		return;

	const float CurrentYaw = Body->GetRelativeRotation().Yaw;
	Body->SetRelativeRotation({ 0.0f, CurrentYaw + YawDelta, 0.0f });
}

void UVehicleMovementComponent::UpdateDrive(const FVehicleInput& Input, float DeltaTime)
{
	UBoxComponent* Body = GetBody();
	if (!Body || DeltaTime <= 0.0f)
		return;

	FVector Accel = FVector::ZeroVector;

	if (IsGrounded())
	{
		const FQuat Rotation = Body->GetWorldRotation().Quaternion();
		const FVector Forward = Rotation.GetForwardVector();
		const FVector Right = Rotation.GetRightVector();
		const float ForwardSpeed = Velocity.Dot(Forward);
		const float LateralSpeed = Velocity.Dot(Right);

		float TargetSpeed = 0.0f;
		if (Input.Throttle > 0.0f)
			TargetSpeed = Input.Throttle * (Input.bBoost ? Tuning.BoostMaxSpeed : Tuning.MaxForwardSpeed);
		else if (Input.Throttle < 0.0f)
			TargetSpeed = Input.Throttle * Tuning.MaxReverseSpeed;

		const bool bSpeedingUp = TargetSpeed * ForwardSpeed >= 0.0f && std::abs(TargetSpeed) > std::abs(ForwardSpeed);
		const float Rate = bSpeedingUp ? Tuning.Acceleration : Tuning.Deceleration;
		const float ForwardAccel = std::clamp((TargetSpeed - ForwardSpeed) / DeltaTime, -Rate, Rate);
		const float LateralAccel = -LateralSpeed * std::min(Tuning.Grip, 1.0f / DeltaTime);

		Accel = Forward * ForwardAccel + Right * LateralAccel;

		if (Input.bJumpRequested)
			Accel.Z += Tuning.JumpSpeed / DeltaTime;
	}

	if (Body->GetAcceleration() != Accel)
		Body->SetAcceleration(Accel);
}

void UVehicleMovementComponent::ResetMotion()
{
	PendingInput = {};
	bBoosting = false;
	SteeringInput = 0.0f;
	Velocity = FVector::ZeroVector;
	PreviousDeltaTime = 0.0f;
	bGroundContactThisStep = false;
	LastAirborneVerticalSpeed = 0.0f;

	UBoxComponent* Body = GetBody();
	if (!Body)
		return;

	PreviousLocation = Body->GetWorldLocation();
	bHasPreviousLocation = true;
	Body->SetAcceleration(FVector::ZeroVector);
	if (UWorld* World = Body->GetWorld())
		World->GetPhysicsScene().ResetPrimitiveVelocity(Body);
}

void UVehicleMovementComponent::SetBoostCapacity(float Seconds)
{
	BoostCapacity = std::isfinite(Seconds) ? std::max(0.0f, Seconds) : 0.0f;
	BoostRemaining = std::min(BoostRemaining, BoostCapacity);
}

void UVehicleMovementComponent::SetBoostRechargeTime(float Seconds)
{
	BoostRechargeTime = std::isfinite(Seconds) && Seconds > 0.0f ? Seconds : 30.0f;
}

float UVehicleMovementComponent::GetBoostRatio() const
{
	if (bInfiniteBoost)
		return 1.0f;
	return BoostCapacity > 0.0f ? BoostRemaining / BoostCapacity : 0.0f;
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
