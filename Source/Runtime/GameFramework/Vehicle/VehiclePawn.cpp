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
#include <cmath>

AVehiclePawn::AVehiclePawn()
{
	SetActorEnableCollision(true);
	PrimaryActorTick.TickGroup = ETickingGroup::PostPhysics;

	Box = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Script = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	Movement = CreateDefaultSubobject<UVehicleMovementComponent>("MovementComponent");
	Camera = CreateDefaultSubobject<UCameraComponent>("CameraComponent");

	Box->SetMotionType(EPhysicsMotionType::Dynamic);
	Box->SetEnableGravity(true);
	Box->SetGravityScale(2.0f);
	Box->SetObjectChannel(ECollisionChannel::WorldDynamic);
	Box->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Block);
	Box->SetCOF(0.0f);
	Box->SetRestitution(0.0f);
	SetRootComponent(Box);

	StaticMesh->SetupAttachment(Box);
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

	Camera->SetupAttachment(Box);

	Camera->SetRelativeLocation({ -4, 0, 4 });
	Camera->SetRelativeRotation({ 20, 0, 0 });

	Camera->SetExternalInputManaged(true);
	SetBody("italia");
}

void AVehiclePawn::BeginPlay()
{
	// 컴포넌트 역직렬화/PIE 복제 후 선택한 모델로 이전 씬의 Box 치수를 갱신한다.
	FitBodyCollision();
	Super::BeginPlay();

	if (Camera)
		BaseFOV = Camera->GetFieldOfView();
	RefreshVehicleVisuals();
	UpdateFollowCamera();
}

void AVehiclePawn::EndPlay()
{
	ClearVehicleVisuals();
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
	UpdateWheelVisuals(DeltaTime);
	UpdateFollowCamera();

	if (!Camera || !Movement)
		return;

	const float TargetFOV = Movement->IsBoosting() ? BaseFOV + BoostFOVOffset : BaseFOV;
	const float CurrentFOV = Camera->GetFieldOfView();
	Camera->SetFieldOfView(CurrentFOV + (TargetFOV - CurrentFOV) * std::min(DeltaTime * FOVInterpSpeed, 1.0f));
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

	StaticMesh->SetStaticMesh(Mesh);
	FitBodyCollision();
	if (HasBegunPlay()) RefreshVehicleVisuals();
	UpdateFollowCamera();

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
	Box->SetMass(GetVehicleCategoryInfo(Category).Mass);
	if (Movement)
	{
		FVehicleTuning Tuning = Movement->GetTuning();
		const FVehicleTuning ClassTuning = MakeVehicleCategoryTuning(Category);
		Tuning.MaxForwardSpeed = ClassTuning.MaxForwardSpeed;
		Tuning.MaxReverseSpeed = ClassTuning.MaxReverseSpeed;
		Tuning.BoostMaxSpeed = ClassTuning.BoostMaxSpeed;
		Tuning.Acceleration = ClassTuning.Acceleration;
		Movement->SetTuning(Tuning);
	}
	// 피벗 오프셋을 Box 크기에 더하면 바퀴 아래에 빈 충돌 영역이 생긴다.
	// 메시를 Box 중심에 맞추고, 회전/스케일이 반영된 크기의 절반을 사용한다.
	FTransform Shape = StaticMesh->GetTransform();
	Shape.Location = FVector::ZeroVector;
	const FBox Bounds = StaticMesh->GetMeshData()->AABB.GetWorldAABB(Shape.GetLocalMatrix());
	StaticMesh->SetRelativeLocation((Bounds.Min + Bounds.Max) * -0.5f);
	Box->SetBoxExtent((Bounds.Max - Bounds.Min) * 0.5f);
	UpdateFollowCamera();
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
		? Movement->GetSteeringInput() * 25.0f : 0.0f;
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

void AVehiclePawn::UpdateFollowCamera()
{
	if (!Camera || !Box || !StaticMesh) return;
	const FStaticMeshData* Data = StaticMesh->GetMeshData();
	const FVector Center = Data ? StaticMesh->GetWorldMatrix().TransformPosition((Data->AABB.Min + Data->AABB.Max) * 0.5f)
		: Box->GetWorldLocation();
	const FVector Extent = Data ? (Data->AABB.Max - Data->AABB.Min) * 0.5f * StaticMesh->GetWorldScale3D().GetAbs()
		: Box->GetBoxExtent();
	const FVector Forward = FRotator(0.0f, Box->GetWorldRotation().Yaw, 0.0f).Quaternion().GetForwardVector();
	const float DistanceScale = std::isfinite(CameraDistanceScale) ? std::max(2.0f, CameraDistanceScale) : 5.0f;
	const float MinimumDistance = std::isfinite(MinCameraDistance) ? std::max(1.0f, MinCameraDistance) : 10.0f;
	const FVector Position = Center - Forward * std::max(MinimumDistance, Extent.X * DistanceScale)
		+ FVector(0,0,std::max(3.0f, Extent.Z * 3.0f));
	const FVector Direction = Center + Forward + FVector(0,0,0.3f) - Position;
	const FRotator View(FMath::RadiansToDegrees(std::atan2(-Direction.Z, std::hypot(Direction.X, Direction.Y))),
		FMath::RadiansToDegrees(std::atan2(Direction.Y, Direction.X)), 0.0f);
	Camera->SetWorldLocation(Position);
	Camera->SetRelativeRotation((Box->GetWorldRotation().Quaternion().Inverse() * View.Quaternion()).ToFRotator());
	Camera->SetExternalInputManaged(true);
}
