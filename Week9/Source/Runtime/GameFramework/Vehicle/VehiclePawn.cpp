#include "EnginePCH.h"
#include "VehiclePawn.h"
#include "VehicleTypes.h"
#include "VehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/BoxComponent.h"
#include "Asset/AssetManager.h"
#include "Collision/FPhysSceneChaos.h"
#include "Collision/FPBDRigidsSolver.h"
#include "Engine/World.h"
#include "Component/SpringArmComponent.h"
#include <cmath>

AVehiclePawn::AVehiclePawn()
{
	SetActorEnableCollision(true);
	PrimaryActorTick.TickGroup = ETickingGroup::PostPhysics;

	Box = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	VisualRoot = CreateDefaultSubobject<USceneComponent>("VehicleVisualRoot");
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Script = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	Movement = CreateDefaultSubobject<UVehicleMovementComponent>("MovementComponent");
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArmComponent");
	Camera = CreateDefaultSubobject<UCameraComponent>("CameraComponent");

	Box->SetMotionType(EPhysicsMotionType::Dynamic);
	Box->SetEnableGravity(true);
	Box->SetGravityScale(5.5f);
	Box->SetObjectChannel(ECollisionChannel::WorldDynamic);
	Box->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Block);
	Box->SetCOF(0.0f);
	Box->SetRestitution(0.0f);
	SetRootComponent(Box);

	// 외형 효과는 이 부모에만 적용한다. 모델의 원래 Transform과 Box 크기는 유지한다.
	VisualRoot->SetupAttachment(Box);
	VisualRoot->SetFlags(EObjectFlags::RF_Transient);
	StaticMesh->SetupAttachment(VisualRoot);
	StaticMesh->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Ignore);
	StaticMesh->SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);


	// 원본 컴포넌트는 모델 선택/저장을 담당하고, 플레이할 때 분리 메시로 표시한다.
	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>("VehicleBodyVisual");
	const char* WheelNames[] = { "WheelFrontLeft", "WheelFrontRight", "WheelRearLeft", "WheelRearRight" };
	for (int32 Index = 0; Index < 4; ++Index)
		Wheels[Index] = CreateDefaultSubobject<UStaticMeshComponent>(WheelNames[Index]);
	for (UStaticMeshComponent* Part : { BodyVisual, Wheels[0], Wheels[1], Wheels[2], Wheels[3] })
	{
		Part->SetupAttachment(StaticMesh);
		Part->SetStaticMesh(nullptr);
		Part->SetVisible(false);
		Part->SetFlags(EObjectFlags::RF_Transient);
		Part->SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);
		Part->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Ignore);
	}

	Script->SetScriptPath("Assets/Scripts/Vehicle/VehicleController.lua");

	// SpringArmComponent
	SpringArmComponent->SetupAttachment(Box);
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritRoll = false;
	SpringArmComponent->bEnableCameraLag = true;
	SpringArmComponent->bEnableCameraRotationLag = true;
	SpringArmComponent->CameraLagSpeed = PositionFollowSpeed;
	SpringArmComponent->CameraRotationLagSpeed = RotationFollowSpeed;
	SpringArmComponent->ProbeSize = 0.5f;

	Camera->SetupAttachment(SpringArmComponent);
	Camera->SetExternalInputManaged(true);
	SetBody("italia");
}

void AVehiclePawn::BeginPlay()
{
	ResetHitSquash();
	// 모델 보정/스크립트 초기화가 카메라를 갱신하기 전에 씬의 기본 FOV를 보관한다.
	if (Camera)
		BaseFOV = Camera->GetFieldOfView();
	// 컴포넌트 역직렬화/PIE 복제 후 선택한 모델로 이전 씬의 Box 치수를 갱신한다.
	FitBodyCollision();
	Super::BeginPlay();

	RefreshVehicleVisuals();
	ResetFollowCamera();
}

void AVehiclePawn::EndPlay()
{
	ResetHitSquash();
	ClearVehicleVisuals();
	bCameraInitialized = false;
	if (Camera) Camera->SetFieldOfView(BaseFOV);
	Super::EndPlay();
}

void AVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (StaticMesh && StaticMesh->GetStaticMesh() != AppliedBodyMesh)
	{
		FitBodyCollision();
		RefreshVehicleVisuals();
	}
	UpdateHitSquash(DeltaTime);
	UpdateWheelVisuals(DeltaTime);
	UpdateFollowCamera(DeltaTime);
}

void AVehiclePawn::SetVehicleInput(const FVehicleInput& Input)
{
	if (!Movement)
		return;

	Movement->SetInput(Input.Throttle, Input.Steer, Input.bBoost);
	
	if (Input.bJumpRequested)
		Movement->RequestJump();
}

void AVehiclePawn::RequestJump()
{
	if (!Movement)
		return;

	Movement->RequestJump();
}

bool AVehiclePawn::SetBody(const FString& MeshName)
{
	UStaticMesh* Mesh = UAssetManager::GetMesh(MeshName);
	if (!Mesh || !StaticMesh || !Box)
		return false;

	ResetHitSquash();
	StaticMesh->SetStaticMesh(Mesh);
	FitBodyCollision();
	if (HasBegunPlay()) RefreshVehicleVisuals();
	ResetFollowCamera();

	return true;
}

FString AVehiclePawn::GetBodyName() const
{
	const UStaticMesh* Mesh = StaticMesh ? StaticMesh->GetStaticMesh() : nullptr;
	if (!Mesh)
		return FString();

	return fs::path(Mesh->GetPath()).stem().string();
}

void AVehiclePawn::FitBodyCollision()
{
	if (!StaticMesh || !Box || !StaticMesh->GetMeshData()) return;
	const EVehicleCategory Category = GetVehicleCategory(GetBodyName());
	const FVehicleCategoryInfo& ClassInfo = GetVehicleCategoryInfo(Category);
	Box->SetMass(ClassInfo.Mass);
	if (Movement)
	{
		FVehicleTuning Tuning = Movement->GetTuning();
		const FVehicleTuning ClassTuning = MakeVehicleCategoryTuning(Category);
		Tuning.MaxForwardSpeed = ClassTuning.MaxForwardSpeed;
		Tuning.MaxReverseSpeed = ClassTuning.MaxReverseSpeed;
		Tuning.BoostMaxSpeed = ClassTuning.BoostMaxSpeed;
		Tuning.Acceleration = ClassTuning.Acceleration;
		Tuning.TurnRate = ClassTuning.TurnRate;
		Tuning.MaxSteeringAngle = ClassTuning.MaxSteeringAngle;
		Movement->SetTuning(Tuning);
		Movement->SetBoostRechargeTime(ClassInfo.BoostRechargeTime);
	}
	// 피벗 오프셋을 Box 크기에 더하면 바퀴 아래에 빈 충돌 영역이 생긴다.
	// 메시를 Box 중심에 맞추고, 회전/스케일이 반영된 크기의 절반을 사용한다.
	FTransform Shape = StaticMesh->GetTransform();
	Shape.Location = FVector::ZeroVector;
	const FBox Bounds = StaticMesh->GetMeshData()->AABB.GetWorldAABB(Shape.GetLocalMatrix());
	StaticMesh->SetRelativeLocation((Bounds.Min + Bounds.Max) * -0.5f);
	Box->SetBoxExtent((Bounds.Max - Bounds.Min) * 0.5f);
	UpdateFollowCamera(0.0f);
}

void AVehiclePawn::ClearVehicleVisuals()
{
	if (bUsingParts) StaticMesh->SetVisible(bSourceWasVisible);
	bUsingParts = false;
	BodyVisual->SetVisible(false);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Wheels[Index]->SetVisible(false);
		WheelSpin[Index] = 0.0f;
	}
	WheelSteering = 0.0f;
}

void AVehiclePawn::RefreshVehicleVisuals()
{
	ClearVehicleVisuals();
	AppliedBodyMesh = StaticMesh->GetStaticMesh();
	TArray<FStaticMeshPart> Parts;
	if (!AppliedBodyMesh || !UAssetManager::GetObjMeshParts(AppliedBodyMesh, Parts)) return;
	const FStaticMeshPart* Body = nullptr;
	int32 WheelCount = 0;
	float MinWheelX = FLT_MAX, MaxWheelX = -FLT_MAX;
	for (const FStaticMeshPart& Part : Parts)
	{
		if (Part.Name.starts_with("wheel"))
		{
			++WheelCount;
			MinWheelX = std::min(MinWheelX, Part.Pivot.X);
			MaxWheelX = std::max(MaxWheelX, Part.Pivot.X);
		}
		else if (!Body) Body = &Part;
		else return; // 복수 차체 등 다른 구조는 원본 메시 그대로 표시한다.
	}
	if (!Body || WheelCount != 4) return;
	bool Assigned[4] = {};
	for (const FStaticMeshPart& Part : Parts)
	{
		UStaticMeshComponent* Visual = BodyVisual;
		if (Part.Name.starts_with("wheel"))
		{
			const bool bFront = Part.Pivot.X > (MinWheelX + MaxWheelX) * 0.5f;
			const int32 Index = (bFront ? 0 : 2) + (Part.Pivot.Y < 0.0f ? 0 : 1);
			if (Assigned[Index]) { ClearVehicleVisuals(); return; }
			Assigned[Index] = true;
			Visual = Wheels[Index];
		}
		Visual->SetStaticMesh(Part.Mesh);
		Visual->SetRelativeLocation(Part.Pivot);
		Visual->SetRelativeRotation(FRotator());
		Visual->SetRelativeScale3D(FVector::OneVector);
		for (int32 Slot = 0; Slot < StaticMesh->GetNumMaterials(); ++Slot)
			Visual->SetMaterial(Slot, StaticMesh->GetOverrideMaterial(Slot));
		Visual->SetVisible(StaticMesh->IsVisible());
	}
	bSourceWasVisible = StaticMesh->IsVisible();
	StaticMesh->SetVisible(false);
	bUsingParts = true;
}

void AVehiclePawn::UpdateWheelVisuals(float DeltaTime)
{
	if (!bUsingParts || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f || !GetWorld()) return;
	FVector Velocity = FVector::ZeroVector;
	auto& Physics = GetWorld()->GetPhysicsScene();
	for (const auto& State : Physics.GetSolver().GetResultTransforms())
		if (Physics.FindPrimitive(State.BodyId) == Box) Velocity = State.LinearVelocity;
	// o/wheel 데이터도 엔진 +X 전방으로 변환된 원본과 같은 좌표계를 사용한다.
	const float ForwardSpeed = Velocity.Dot(Box->GetWorldRotation().Quaternion().GetForwardVector());
	const float TargetSteer = Movement && Movement->IsActive() && Movement->IsComponentTickEnabled()
		? Movement->GetSteeringInput() * Movement->GetTuning().MaxSteeringAngle : 0.0f;
	WheelSteering += (TargetSteer - WheelSteering) * (1.0f - std::exp(-12.0f * DeltaTime));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Wheel = Wheels[Index];
		const FBox& Bounds = Wheel->GetMeshData()->AABB;
		const float Radius = (Bounds.Max.Z - Bounds.Min.Z) * 0.5f * std::abs(Wheel->GetWorldScale3D().Z);
		if (Radius <= 0.001f || !std::isfinite(Radius) || !std::isfinite(ForwardSpeed)) continue;
		WheelSpin[Index] = std::remainder(WheelSpin[Index] + ForwardSpeed * DeltaTime / Radius, 2.0f * PI);
		const FQuat Steer = FQuat::MakeFromAxisAngle(FVector(0,0,1), Index < 2 ? FMath::DegreesToRadians(WheelSteering) : 0.0f);
		const FQuat Spin = FQuat::MakeFromAxisAngle(FVector(0,1,0), WheelSpin[Index]);
		Wheel->SetRelativeRotation((Steer * Spin).Normalized().ToFRotator());
	}
}

void AVehiclePawn::PlayHitSquash()
{
	if (!VisualRoot) return;
	// 연속해서 다른 충돌이 발생해도 현재 외형에서 이어서 재생한다.
	HitSquashAnimation.Play(VisualRoot->GetRelativeScale3D().X);
}

void AVehiclePawn::ResetHitSquash()
{
	HitSquashAnimation.Reset();
	if (VisualRoot) VisualRoot->SetRelativeScale3D(FVector::OneVector);
}

void AVehiclePawn::UpdateHitSquash(float DeltaTime)
{
	if (!HitSquashAnimation.IsActive() || !VisualRoot || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f) return;
	const float Scale = HitSquashAnimation.Update(DeltaTime);
	VisualRoot->SetRelativeScale3D(FVector(Scale));
}

void AVehiclePawn::ResetFollowCamera()
{
	bCameraInitialized = false;
	if (SpringArmComponent) SpringArmComponent->ResetLag();
	UpdateFollowCamera(0.0f);
}

void AVehiclePawn::UpdateFollowCamera(float DeltaTime)
{
	if (!Camera || !Box || !StaticMesh) return;
	auto FollowAlpha = [DeltaTime](float Speed)
	{
		if (!std::isfinite(Speed) || Speed <= 0.0f) return 1.0f;
		return std::isfinite(DeltaTime) && DeltaTime > 0.0f ? 1.0f - std::exp(-Speed * DeltaTime) : 0.0f;
	};

	const FStaticMeshData* Data = StaticMesh->GetMeshData();
	// Squash에 따라 추적 카메라 거리까지 흔들리지 않도록 모델의 원래 크기를 사용한다.
	const FVector ModelWorldScale = StaticMesh->GetRelativeScale3D() * Box->GetWorldScale3D();
	const FVector Extent = Data ? (Data->AABB.Max - Data->AABB.Min) * 0.5f * ModelWorldScale.GetAbs()
		: Box->GetBoxExtent() * Box->GetWorldScale3D().GetAbs();
	
	// 테스트 브랜치처럼 Yaw만 지연 추적한다. 점프/기울어짐에도 수평선을 유지한다.
	const float DistanceScale = std::isfinite(CameraDistanceScale) ? std::max(2.0f, CameraDistanceScale) : 5.0f;
	const float MinimumDistance = std::isfinite(MinCameraDistance) ? std::max(1.0f, MinCameraDistance) : 10.0f;
	const float Back = std::max({ std::abs(CameraOffset.X), MinimumDistance, Extent.X * DistanceScale });
	const float Up = std::max(CameraOffset.Z, Extent.Z * 3.0f);

	const float DX = Back + LookAtOffset.X;
	const float DZ = Up - LookAtOffset.Z;
	SpringArmComponent->SetRelativeLocation({ LookAtOffset.X, LookAtOffset.Y, LookAtOffset.Z });
	SpringArmComponent->SetRelativeRotation({ FMath::RadiansToDegrees(std::atan2(DZ, DX)), 0.0f, 0.0f });
	SpringArmComponent->TargetArmLength = std::hypot(DX, DZ);
	SpringArmComponent->CameraLagSpeed = RotationFollowSpeed;
	SpringArmComponent->CameraRotationLagSpeed = RotationFollowSpeed;

	// 에디터 모델 교체/로드 중에는 직렬화된 카메라 FOV를 유지한다.
	if (HasBegunPlay())
	{
		const float NormalFOV = std::isfinite(BaseFOV) ? std::clamp(BaseFOV, 1.0f, 170.0f) : 60.0f;
		const bool bBoosting = Movement && Movement->IsActive() && Movement->IsComponentTickEnabled() && Movement->IsBoosting();
		const float TargetFOV = bBoosting ? std::clamp(NormalFOV + BoostFOVOffset, NormalFOV, 170.0f) : NormalFOV;
		const float CurrentFOV = bCameraInitialized && std::isfinite(Camera->GetFieldOfView()) ? Camera->GetFieldOfView() : NormalFOV;
		Camera->SetFieldOfView(CurrentFOV + (TargetFOV - CurrentFOV) * FollowAlpha(FOVFollowSpeed));
	}
	bCameraInitialized = true;
}
