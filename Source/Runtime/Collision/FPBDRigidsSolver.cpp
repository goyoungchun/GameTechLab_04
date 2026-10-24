#include "EnginePCH.h"

#include "FPBDRigidsSolver.h"
#include "FPBDRigidsEvolutionBase.h"

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
}

void FPBDRigidsSolver::SetBodyStates(const TArray<FPhysicsBodyState>& InBodyStates)
{
	BodyStates = InBodyStates;
	ResultTransforms.Reset();
}

void FPBDRigidsSolver::AdvanceSolverBy(float DeltaTime)
{
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
