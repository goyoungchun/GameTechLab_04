#pragma once

#include "Container/Array.h"
#include "Container/Map.h"
#include "PhysicsBodyState.h"
#include "PhysicsCollision.h"

class FPBDRigidsEvolutionGBF;
class UPrimitiveComponent;

// 실제 언리얼 구조
// 추후 필요에 따라 체계화 필요
class FPhysicsSolverEvents
{

};

class FPhysicsSolverBase : public FPhysicsSolverEvents
{

};

class FPBDRigidsSolver : public FPhysicsSolverBase
{
public:
	FPBDRigidsSolver();
	~FPBDRigidsSolver();

	void RegisterBody(uint64 BodyId, UPrimitiveComponent* Component);
	void UnregisterBody(uint64 BodyId);
	void SetBodyStates(const TArray<FPhysicsBodyState>& InBodyStates);
	void ResetBodyVelocity(uint64 BodyId);

	void AdvanceSolverBy(float DeltaTime);

	const TArray<FPhysicsBodyState>& GetResultTransforms() const;
	const TArray<FPhysicsCollisionResult>& GetCollisionResults() const;
private:
	TUniquePtr<FPBDRigidsEvolutionGBF> Evolution;
	TArray<FPhysicsBodyState> BodyStates;
	TArray<FPhysicsBodyState> ResultTransforms;
	// 게임 코드가 움직인 Kinematic의 실제 이동 속도를 다음 Step에서 계산한다.
	TMap<uint64, FVector> PreviousKinematicLocations;
};
