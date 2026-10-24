#include "EnginePCH.h"

#include "FPBDRigidsSolver.h"
#include "FPBDRigidsEvolutionBase.h"

#include <cmath>
#include <utility>

FPBDRigidsSolver::FPBDRigidsSolver()
	: Evolution(MakeUnique<FPBDRigidsEvolutionGBF>())
{
}

FPBDRigidsSolver::~FPBDRigidsSolver() = default;

void FPBDRigidsSolver::RegisterBody(uint64 BodyId, UPrimitiveComponent* Component)
{
	Evolution->RegisterBody(BodyId, Component);
}

void FPBDRigidsSolver::UnregisterBody(uint64 BodyId)
{
	Evolution->UnregisterBody(BodyId);
	PreviousKinematicLocations.Remove(BodyId);
	for (int32 Index = BodyStates.Num() - 1; Index >= 0; --Index)
		if (BodyStates[Index].BodyId == BodyId)
			BodyStates.RemoveAtSwap(Index);
	for (int32 Index = ResultTransforms.Num() - 1; Index >= 0; --Index)
		if (ResultTransforms[Index].BodyId == BodyId)
			ResultTransforms.RemoveAtSwap(Index);
}

void FPBDRigidsSolver::SetBodyStates(const TArray<FPhysicsBodyState>& InBodyStates)
{
	TMap<uint64, const FPhysicsBodyState*> PreviousStates;
	PreviousStates.Reserve(BodyStates.Num());
	for (const FPhysicsBodyState& State : BodyStates)
		PreviousStates.Add(State.BodyId, &State);

	TArray<FPhysicsBodyState> UpdatedStates = InBodyStates;
	for (FPhysicsBodyState& State : UpdatedStates)
	{
		// Scene은 매 프레임 Pose와 설정을 새로 수집하므로 속도만 이전 결과에서 복원한다.
		const FPhysicsBodyState* const* Previous = PreviousStates.Find(State.BodyId);
		if (State.MotionType == EPhysicsMotionType::Dynamic && Previous
			&& (*Previous)->MotionType == EPhysicsMotionType::Dynamic)
			State.LinearVelocity = (*Previous)->LinearVelocity;
		else if (State.MotionType == EPhysicsMotionType::Static)
			State.LinearVelocity = FVector::ZeroVector;
	}
	BodyStates = std::move(UpdatedStates);
	ResultTransforms.Reset();
}

void FPBDRigidsSolver::AdvanceSolverBy(float DeltaTime)
{
	if (std::isfinite(DeltaTime) && DeltaTime > 0.0f)
	{
		TMap<uint64, FVector> CurrentKinematicLocations;
		for (FPhysicsBodyState& State : BodyStates)
		{
			if (State.MotionType != EPhysicsMotionType::Kinematic)
				continue;
			if (const FVector* Previous = PreviousKinematicLocations.Find(State.BodyId))
				State.LinearVelocity = (State.WorldTransform.Location - *Previous) / DeltaTime;
			CurrentKinematicLocations.Add(State.BodyId, State.WorldTransform.Location);
		}
		PreviousKinematicLocations = std::move(CurrentKinematicLocations);
	}
	Evolution->AdvanceOneTimeStep(BodyStates, DeltaTime);
	ResultTransforms = BodyStates;
}

const TArray<FPhysicsBodyState>& FPBDRigidsSolver::GetResultTransforms() const
{
	return ResultTransforms;
}

const TArray<FPhysicsCollisionResult>& FPBDRigidsSolver::GetCollisionResults() const
{
	return Evolution->GetCollisionResults();
}
