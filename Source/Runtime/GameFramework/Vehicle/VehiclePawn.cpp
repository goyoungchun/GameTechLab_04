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
	SetRootComponent(Box);

	StaticMesh->SetStaticMesh(UAssetManager::GetMesh("italia"));
	StaticMesh->SetupAttachment(Box);
	StaticMesh->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Ignore);
	StaticMesh->SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);
	Box->SetBoxExtent(StaticMesh->GetMeshData()->AABB.Max);

	Script->SetScriptPath("Assets/Scripts/Vehicle/VehicleController.lua");

	Camera->SetupAttachment(Box);
	Camera->SetRelativeLocation({ -2.5, 0, 4 });
	Camera->SetRelativeRotation({ 30, 0, 0 });
	Camera->SetExternalInputManaged(true);
}

void AVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
}