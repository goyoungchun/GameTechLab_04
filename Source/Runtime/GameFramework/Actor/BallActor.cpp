#include "EnginePCH.h"
#include "BallActor.h"

#include "Asset/AssetManager.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "GameFramework/Vehicle/VehiclePawn.h"

#include <algorithm>
#include <cmath>

ABallActor::ABallActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::PostPhysics;
	Collision = CreateDefaultSubobject<USphereComponent>("BallCollision");
	SetRootComponent(Collision);
	Collision->SetMotionType(EPhysicsMotionType::Dynamic);
	Collision->SetObjectChannel(ECollisionChannel::WorldDynamic);
	Collision->SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Block);
	Collision->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Block);
	Collision->SetEnableGravity(true);
	Collision->SetGravityScale(4.5f);
	Collision->SetMass(0.45f);
	Collision->SetCOF(0.05f);
	Collision->SetRestitution(0.5f);
	Collision->SetDrawShape(false);
	Collision->OnComponentHit.AddRaw(this, &ABallActor::HandleHit);

	Script = CreateDefaultSubobject<UScriptComponent>("ScriptComponent");
	Script->SetScriptPath("Assets/Scripts/Ball.lua");

	VisualRoot = CreateDefaultSubobject<USceneComponent>("BallVisualRoot");
	VisualRoot->SetupAttachment(Collision);
	VisualRoot->SetFlags(EObjectFlags::RF_Transient);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("BallMesh");
	Mesh->SetupAttachment(VisualRoot);
	Mesh->SetEnableGravity(false);
	Mesh->SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);
	Mesh->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Ignore);

	// 표시 메시와 충돌 반지름에 같은 배율을 적용하고, 모델 중심에 맞춘다.
	constexpr float SizeMultiplier = 1.2f;
	constexpr float MeshScale = 2.5f * SizeMultiplier;
	Mesh->SetRelativeScale3D(FVector(MeshScale));
	UStaticMesh* BallMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Models/SoccerBall/ball.obj");
	Mesh->SetStaticMesh(BallMesh);
	Collision->SetSphereRadius(1.833f * SizeMultiplier);
	if (BallMesh)
	{
		const FStaticMeshData& Data = BallMesh->GetMeshData();
		const FVector Center = (Data.AABB.Min + Data.AABB.Max) * 0.5f;
		float RadiusSquared = 0.0f;
		for (const FVertexPNCT& Vertex : Data.Vertices)
		{
			const FVector Offset = Vertex.Position - Center;
			RadiusSquared = std::max(RadiusSquared, Offset.Dot(Offset));
		}
		Mesh->SetRelativeLocation(-Center * MeshScale);
		if (RadiusSquared > 0.0f)
			Collision->SetSphereRadius(std::sqrt(RadiusSquared) * MeshScale);
	}
}

void ABallActor::BeginPlay()
{
	ResetHitSquash();
	Super::BeginPlay();
}

void ABallActor::EndPlay()
{
	ResetHitSquash();
	Super::EndPlay();
}

void ABallActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f) return;
	ContactTime += DeltaTime;
	++ContactTick;
	if (VisualRoot && HitSquashAnimation.IsActive())
		VisualRoot->SetRelativeScale3D(FVector(HitSquashAnimation.Update(DeltaTime)));
}

void ABallActor::ResetHitSquash()
{
	HitSquashAnimation.Reset();
	RecentContacts.Empty();
	ContactTime = 0.0;
	ContactTick = 0;
	if (VisualRoot) VisualRoot->SetRelativeScale3D(FVector::OneVector);
}

void ABallActor::HandleHit(UPrimitiveComponent* Self, AActor* OtherActor, UPrimitiveComponent* Other,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (const AVehiclePawn* Vehicle = Cast<AVehiclePawn>(OtherActor))
		LastTouchPlayerIndex = Vehicle->GetPlayerIndex();

	if (!Other || !Hit.bBlockingHit || !VisualRoot || IsActorBeingDestroyed()) return;
	const uint32 OtherId = Other->GetUUID();
	const FRecentContact* Previous = RecentContacts.FindOrNull(OtherId);
	// 지속 접촉은 프레임 간격과 무관하게 제외하고, 빠른 재접촉도 제한한다.
	const bool bNewContact = !Previous || (ContactTick > Previous->Tick + 1
		&& ContactTime - Previous->Time >= 0.15);
	RecentContacts[OtherId] = { ContactTime, ContactTick };
	if (bNewContact)
		HitSquashAnimation.Play(VisualRoot->GetRelativeScale3D().X);
}

void ABallActor::Retire()
{
	if (IsActorBeingDestroyed())
		return;
	ResetHitSquash();
	// 지연 삭제되는 프레임에도 이전 공이 새 공과 충돌하거나 화면에 남지 않게 한다.
	SetActorEnableCollision(false);
	UnregisterAllComponents();
	Destroy();
}
