#pragma once

#include "Container/Array.h"
#include "PhysicsBodyState.h"

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
	FPBDRigidsSolver() = default;
	~FPBDRigidsSolver() = default;

	void OnGravity(float deltatime);

	void SetBodyStates(const TArray<FPhysicsBodyState>& InBodyStates)
	{
		BodyStates = InBodyStates;
		ResultTransforms.Reset();
	}

	void AdvanceSolverBy(float /*DeltaTime*/)
	{
		// 적분/충돌 구현 전에는 입력 Pose를 그대로 반환한다.
		ResultTransforms = BodyStates;
	}

	const TArray<FPhysicsBodyState>& GetResultTransforms() const { return ResultTransforms; }
private:
	TArray<FPhysicsBodyState> BodyStates;
	TArray<FPhysicsBodyState> ResultTransforms;
};
