#include "EnginePCH.h"
#include "VehiclePawn.h"
#include "VehicleTypes.h"
#include "VehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/BoxComponent.h"
#include "Asset/AssetManager.h"

AVehiclePawn::AVehiclePawn()
{
	SetActorEnableCollision(true);

	Box = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Script = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	Movement = CreateDefaultSubobject<UVehicleMovementComponent>("MovementComponent");
	Camera = CreateDefaultSubobject<UCameraComponent>("CameraComponent");

	Box->SetMotionType(EPhysicsMotionType::Dynamic);
	Box->SetEnableGravity(true);
	Box->SetObjectChannel(ECollisionChannel::WorldDynamic);
	Box->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Block);
	Box->SetCOF(0.0f);
	Box->SetRestitution(0.0f);
	SetRootComponent(Box);

	StaticMesh->SetupAttachment(Box);
	StaticMesh->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Ignore);
	StaticMesh->SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);
	SetBody("italia");

	Script->SetScriptPath("Assets/Scripts/Vehicle/VehicleController.lua");

	Camera->SetupAttachment(Box);
	Camera->SetRelativeLocation({ -4, 0, 4 });
	Camera->SetRelativeRotation({ 20, 0, 0 });
	Camera->SetExternalInputManaged(true);
}

void AVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
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

	if (const FStaticMeshData* MeshData = StaticMesh->GetMeshData())
		Box->SetBoxExtent(MeshData->AABB.Max);

	return true;
}

FString AVehiclePawn::GetBodyName() const
{
	const UStaticMesh* Mesh = StaticMesh ? StaticMesh->GetStaticMesh() : nullptr;
	if (!Mesh)
		return FString();

	return fs::path(Mesh->GetPath()).stem().string();
}