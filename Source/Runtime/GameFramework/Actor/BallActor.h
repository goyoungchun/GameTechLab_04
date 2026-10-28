#pragma once

#include "GameFramework/Actor.h"
#include "Container/Map.h"
#include "GameFramework/HitSquashAnimation.h"

class USphereComponent;
class UStaticMeshComponent;
class UScriptComponent;
class USceneComponent;

// 매 턴 새로 생성하는 물리 공. 움직임은 Solver가 처리한다.
class ABallActor : public AActor
{
	DECLARE_CLASS(ABallActor, AActor)
	REFLECT_START(ABallActor)
	REFLECT_END()

public:
	ABallActor();
	virtual ~ABallActor() override = default;
	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;

	USphereComponent* GetCollisionComponent() const { return Collision; }
	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }
	void Retire();

	int32 GetLastTouchPlayerIndex() const { return LastTouchPlayerIndex; }

private:
	void HandleHit(UPrimitiveComponent* Self, AActor* OtherActor, UPrimitiveComponent* Other,
		FVector NormalImpulse, const FHitResult& Hit);
	void ResetHitSquash();
	struct FRecentContact
	{
		double Time = 0.0;
		uint64 Tick = 0;
	};

	USphereComponent* Collision = nullptr;
	USceneComponent* VisualRoot = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	UScriptComponent* Script = nullptr;
	int32 LastTouchPlayerIndex = -1;
	FHitSquashAnimation HitSquashAnimation;
	TMap<uint32, FRecentContact> RecentContacts;
	double ContactTime = 0.0;
	uint64 ContactTick = 0;
};
