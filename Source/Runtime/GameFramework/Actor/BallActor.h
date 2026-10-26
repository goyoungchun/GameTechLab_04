#pragma once

#include "GameFramework/Actor.h"

class USphereComponent;
class UStaticMeshComponent;

// 매 턴 새로 생성하는 물리 공. 움직임은 Solver가 처리한다.
class ABallActor : public AActor
{
	DECLARE_CLASS(ABallActor, AActor)
	REFLECT_START(ABallActor)
	REFLECT_END()

public:
	ABallActor();
	virtual ~ABallActor() override = default;

	USphereComponent* GetCollisionComponent() const { return Collision; }
	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }
	void Retire();

private:
	USphereComponent* Collision = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
};
