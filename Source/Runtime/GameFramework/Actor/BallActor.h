#pragma once

#include "GameFramework/Actor.h"
#include "Math/Quat.h"

class USphereComponent;
class UStaticMeshComponent;
class UScriptComponent;

// 매 턴 새로 생성하는 물리 공. 움직임은 Solver가 처리한다.
class ABallActor : public AActor
{
	DECLARE_CLASS(ABallActor, AActor)
	REFLECT_START(ABallActor)
	REFLECT_END()

public:
	ABallActor();
	virtual ~ABallActor() override = default;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	USphereComponent* GetCollisionComponent() const { return Collision; }
	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }
	void Retire();

	int32 GetLastTouchPlayerIndex() const { return LastTouchPlayerIndex; }

private:
	void HandleHit(UPrimitiveComponent* Self, AActor* OtherActor, UPrimitiveComponent* Other,
		FVector NormalImpulse, const FHitResult& Hit);

	USphereComponent* Collision = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	UScriptComponent* Script = nullptr;
	int32 LastTouchPlayerIndex = -1;

	FVector MeshCenterOffset = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	FQuat RollRotation = FQuat::Identity();
};
