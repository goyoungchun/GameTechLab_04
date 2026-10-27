#include "EnginePCH.h"
#include "VehicleMovementComponent.h"
#include "Component/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "VehicleTypes.h"
#include "Engine/World.h"
#include "Collision/FPhysSceneChaos.h"
#include "Collision/FPBDRigidsSolver.h"
#include "Collision/Ray.h"

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
	PrimaryComponentTick.TickGroup = ETickingGroup::PrePhysics;
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
	}
}

void UVehicleMovementComponent::EndPlay()
{
	ResetMotion();
	if (UBoxComponent* Body = GetBody())
		Body->OnComponentHit.Remove(BodyHitHandle);

	Super::EndPlay();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
		return;
	UBoxComponent* Body = GetBody();
	if (!Body) return;

	FVehicleInput Input = PendingInput;
	PendingInput = {};
	const bool bBoostRequested = Input.bBoost && Input.Throttle != 0.0f && Tuning.BoostMultiplier > 1.0f;
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
	UpdateVelocityFromPhysics();
	UpdateGround(DeltaTime);
	const FVector PreviousForward = FRotator(0.0f, Body->GetWorldRotation().Yaw, 0.0f).Quaternion().GetForwardVector();
	UpdateSteering(Input, DeltaTime);
	UpdateDrive(Input, DeltaTime, PreviousForward);
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
		&Tuning.TurnRate, &Tuning.MaxSteeringAngle, &Tuning.FullTurnSpeed, &Tuning.SteeringSpeedScale, &Tuning.BoostMultiplier, &Tuning.JumpSpeed })
	{
		*Value = std::isfinite(*Value) ? std::max(0.0f, *Value) : 0.0f;
	}

	Tuning.BoostMaxSpeed = std::max(Tuning.BoostMaxSpeed, Tuning.MaxForwardSpeed);
	Tuning.MaxSteeringAngle = std::min(Tuning.MaxSteeringAngle, 89.0f);
	Tuning.BoostMultiplier = std::max(1.0f, Tuning.BoostMultiplier);
}

void UVehicleMovementComponent::UpdateVelocityFromPhysics()
{
	const UBoxComponent* Body = GetBody();
	Velocity = FVector::ZeroVector;
	// ResetPrimitiveVelocity 직후에는 직전 프레임의 ResultTransforms가 아직 남아 있다.
	if (bSkipVelocityRead)
	{
		bSkipVelocityRead = false;
		return;
	}
	if (!Body || !GetWorld()) return;
	auto& Physics = GetWorld()->GetPhysicsScene();
	for (const auto& State : Physics.GetSolver().GetResultTransforms())
		if (Physics.FindPrimitive(State.BodyId) == Body)
		{
			Velocity = State.LinearVelocity;
			break;
		}
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

	const UBoxComponent* Body = GetBody();
	bool bGrounded = bGroundContactThisStep;
	bGroundContactThisStep = false;
	const float MinNormalZ = std::cos(FMath::DegreesToRadians(MaxGroundSlopeDegrees));
	if (Body && GetWorld() && Body->GetMotionType() == EPhysicsMotionType::Dynamic && Velocity.Z <= 0.1f)
	{
		// 첫 프레임과 접촉 경계에서도 테스트 차량처럼 바닥을 확인한다.
		if (!bGrounded)
		{
			const FBox Bounds = Body->CalcBounds();
			const FVector Position = Body->GetWorldLocation();
			FHitResult Hit;
			bGrounded = GetWorld()->SweepSingle(
				FRay{ FVector(Position.X, Position.Y, Bounds.Min.Z + 0.12f), FVector(0,0,-1) },
				0.22f, 0.04f, Hit, Body->GetOwner(), Body) && Hit.Normal.Z >= MinNormalZ;
		}
	}
	else bGrounded = false;

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
	const float Scale = Tuning.SteeringSpeedScale > 0.0f ? 1.0f / (1.0f + GetSpeed() / Tuning.SteeringSpeedScale) : 1.0f;
	SteeringInput = Input.Steer * Scale;
	if (!Body || SteeringInput == 0.0f) return;
	float SteeringDirection = 1.0f;
	if (IsGrounded())
	{
		// 전진 -> 정지 -> 후진에 따라 회전량도 연속적으로 줄었다가 반대로 커진다.
		const float FullSteeringSpeed = std::max(1.0f, Tuning.FullTurnSpeed);
		SteeringDirection = std::clamp(GetForwardSpeed() / FullSteeringSpeed, -1.0f, 1.0f);
	}
	FRotator Rotation = Body->GetWorldRotation();
	// 앞바퀴 표시용 SteeringInput은 유지하고, 후진할 때 차체의 회전 방향만 반전한다.
	Rotation.Yaw = std::remainder(Rotation.Yaw + SteeringInput * SteeringDirection * Tuning.TurnRate * DeltaTime, 360.0f);
	Body->MoveComponent(FVector::ZeroVector, Rotation, false);
}

void UVehicleMovementComponent::UpdateDrive(const FVehicleInput& Input, float DeltaTime, const FVector& PreviousForward)
{
	UBoxComponent* Body = GetBody();
	if (!Body || DeltaTime <= 0.0f)
		return;

	const FVector Forward = FRotator(0.0f, Body->GetWorldRotation().Yaw, 0.0f).Quaternion().GetForwardVector();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float Speed = HorizontalVelocity.Length();
	const float BoostScale = Input.bBoost ? Tuning.BoostMultiplier : 1.0f;
	float DriveAcceleration = Input.Throttle * Tuning.Acceleration * BoostScale;
	// 가속/조향 방식은 테스트 브랜치와 같고, 게임의 차종별 최고속도 제한을 추가한다.
	if (Input.Throttle != 0.0f)
	{
		const float ForwardSpeed = Velocity.Dot(Forward);
		// 후진 부스트도 전진 부스트와 같은 최고속도 배율을 사용한다.
		const float ReverseBoostScale = Tuning.MaxForwardSpeed > 0.0f
			? Tuning.BoostMaxSpeed / Tuning.MaxForwardSpeed : 1.0f;
		const float Limit = Input.Throttle > 0.0f ? (Input.bBoost ? Tuning.BoostMaxSpeed : Tuning.MaxForwardSpeed)
			: Tuning.MaxReverseSpeed * (Input.bBoost ? ReverseBoostScale : 1.0f);
		const float Remaining = std::max(0.0f, Limit - (Input.Throttle > 0.0f ? ForwardSpeed : -ForwardSpeed));
		DriveAcceleration = std::copysign(std::min(std::abs(DriveAcceleration), Remaining / DeltaTime), Input.Throttle);
	}
	FVector Accel = Forward * DriveAcceleration;
	if (IsGrounded() && Speed > 0.001f)
	{
		const float Sign = HorizontalVelocity.Dot(PreviousForward) < 0.0f ? -1.0f : 1.0f;
		const FVector TargetVelocity = Forward * (Speed * Sign);
		const float Alpha = 1.0f - std::exp(-Tuning.Grip * DeltaTime);
		Accel += (TargetVelocity - HorizontalVelocity) * (Alpha / DeltaTime);
	}
	if (Input.bJumpRequested && IsGrounded() && Body->IsGravityEnabled())
		Accel.Z = std::max(0.0f, Tuning.JumpSpeed - Velocity.Z) / DeltaTime;

	if (Body->GetAcceleration() != Accel)
		Body->SetAcceleration(Accel);
}

void UVehicleMovementComponent::ResetMotion()
{
	PendingInput = {};
	bBoosting = false;
	SteeringInput = 0.0f;
	Velocity = FVector::ZeroVector;
	bSkipVelocityRead = true;
	bGroundContactThisStep = false;
	MoveMode = EVehicleMoveMode::Airborne;
	bGroundStateValid = false;
	bLandedThisFrame = false;
	TimeSinceGrounded = 0.0f;
	LastLandingSpeed = 0.0f;
	LastAirborneVerticalSpeed = 0.0f;

	UBoxComponent* Body = GetBody();
	if (!Body)
		return;

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
