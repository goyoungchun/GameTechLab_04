#include "EnginePCH.h"

#include "FPhysSceneChaos.h"
#include "FPBDRigidsSolver.h"
#include "FEventManger.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/World.h"

#include <algorithm>
#include <cmath>

namespace
{
	FTransform GetWorldTransform(const USceneComponent& Component)
	{
		return FTransform(Component.GetWorldRotation(), Component.GetWorldLocation(), Component.GetWorldScale3D());
	}

	bool AreTransformsEqual(const FTransform& A, const FTransform& B)
	{
		constexpr float Tolerance = 1e-4f;
		return (A.Location - B.Location).Size() <= Tolerance
			&& (A.Scale - B.Scale).Size() <= Tolerance
			&& A.GetOrientation().Equals(B.GetOrientation(), Tolerance);
	}

	int32 GetAttachmentDepth(const USceneComponent* Component)
	{
		int32 Depth = 0;
		for (const USceneComponent* Parent = Component->GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
			++Depth;
		return Depth;
	}

	bool MakeRelativeTransform(const USceneComponent& Component, const FTransform& WorldTransform, FTransform& OutRelative)
	{
		OutRelative = WorldTransform;
		const USceneComponent* Parent = Component.GetAttachParent();
		if (!Parent)
			return true;

		const FMatrix ParentWorld = Parent->GetWorldMatrix();
		const FVector ParentScale = Parent->GetWorldScale3D();
		const float Determinant = ParentWorld.Determinant();
		if (!std::isfinite(Determinant) || std::abs(Determinant) <= 1e-8f
			|| std::abs(ParentScale.X) <= 1e-8f || std::abs(ParentScale.Y) <= 1e-8f || std::abs(ParentScale.Z) <= 1e-8f)
			return false;

		OutRelative.Location = ParentWorld.Inverse().TransformPosition(WorldTransform.Location);
		OutRelative.Rotation = (Parent->GetWorldRotation().Quaternion().Inverse() * WorldTransform.GetOrientation()).ToFRotator();
		OutRelative.Scale = WorldTransform.Scale / ParentScale;
		return true;
	}
}

FPhysSceneChaos::FPhysSceneChaos(UWorld* InWorld)
	: OwningWorld(InWorld)
	, Solver(MakeUnique<FPBDRigidsSolver>())
	, EventManager(MakeUnique<FEventManger>())
{
	assert(OwningWorld);
}

bool FPhysSceneChaos::RegisterPrimitive(UPrimitiveComponent* inPrimitive)
{
	if (!inPrimitive || !inPrimitive->IsRegistered() || inPrimitive->GetWorld() != OwningWorld)
		return false;

	for (const FPrimitiveTransformSnapshot* component : PrimitiveSnapShots)
	{
		if (component->Component == inPrimitive)
		{
			return false;
		}
	}
	FPrimitiveTransformSnapshot* Snapshot = new FPrimitiveTransformSnapshot{};
	Snapshot->BodyId = NextBodyId++;
	Snapshot->Component = inPrimitive;
	Snapshot->WorldTransformBeforeUpdate = GetWorldTransform(*inPrimitive);
	Snapshot->UpdatedWorldTransform = Snapshot->WorldTransformBeforeUpdate;

	PrimitiveSnapShots.Add(Snapshot);
	BodySnapshots.Add(Snapshot->BodyId, Snapshot);
	Solver->RegisterBody(Snapshot->BodyId, inPrimitive);
	return true;
}

bool FPhysSceneChaos::UnRegisterPrimitive(UPrimitiveComponent* inPrimitive)
{
	if (!inPrimitive)
		return false;

	for (int i = 0; i < PrimitiveSnapShots.Num(); i++)
	{
		FPrimitiveTransformSnapshot* Snapshot = PrimitiveSnapShots[i];
		if (Snapshot && Snapshot->Component == inPrimitive)
		{
			Solver->UnregisterBody(Snapshot->BodyId);
			BodySnapshots.Remove(Snapshot->BodyId);
			PrimitiveSnapShots.RemoveAtSwap(i);
			delete Snapshot;
			return true;
		}
	}
	return false;
}

FPhysSceneChaos::~FPhysSceneChaos()
{
	for (FPrimitiveTransformSnapshot* primitive : PrimitiveSnapShots)
	{
		delete primitive;
	}
}

UPrimitiveComponent* FPhysSceneChaos::FindPrimitive(uint64 BodyId) const
{
	FPrimitiveTransformSnapshot* const* Found = BodySnapshots.Find(BodyId);
	return Found ? (*Found)->Component : nullptr;
}

// 매 프레임 업데이트 직전 : Snapshot 갱신 및 Component의 물리 입력 수집
TArray<FPhysicsBodyState> FPhysSceneChaos::CapturePhysicsInputs()
{
	TArray<FPhysicsBodyState> InputStates;
	InputStates.Reserve(PrimitiveSnapShots.Num());
	for (FPrimitiveTransformSnapshot* Snapshot : PrimitiveSnapShots)
	{
		const UPrimitiveComponent* Component = Snapshot->Component;
		Snapshot->WorldTransformBeforeUpdate = GetWorldTransform(*Component);
		Snapshot->UpdatedWorldTransform = Snapshot->WorldTransformBeforeUpdate;
		Snapshot->bHasPhysicsResult = false;

		FPhysicsBodyState State;
		State.BodyId = Snapshot->BodyId;
		State.WorldTransform = Snapshot->WorldTransformBeforeUpdate;
		State.MotionType = Component->GetMotionType();
		State.ObjectChannel = Component->GetObjectChannel();
		State.CollisionResponses = Component->GetCollisionResponses();
		State.bEnableGravity = Component->IsGravityEnabled();
		InputStates.Add(State);
	}
	return InputStates;
}
// Solver 결과 -> PrmitiveSnapshots
void FPhysSceneChaos::BufferPhysicsResults()
{
	EventManager->BufferCollisionResults(Solver->GetCollisionResults());
	for (const FPhysicsBodyState& Result : Solver->GetResultTransforms())
	{
		// 프레임 도중 제거된 Body의 결과는 버린다. 배열 순서는 대응에 사용하지 않는다.
		FPrimitiveTransformSnapshot** Found = BodySnapshots.Find(Result.BodyId);
		if (!Found)
			continue;
		(*Found)->UpdatedWorldTransform = Result.WorldTransform;
		(*Found)->bHasPhysicsResult = true;
	}
}
// PrimitiveSnapShots -> Component
void FPhysSceneChaos::PullFromPhysicsState()
{
	TArray<uint64> PendingBodies;
	for (FPrimitiveTransformSnapshot* Snapshot : PrimitiveSnapShots)
	{
		if (!Snapshot->bHasPhysicsResult)
			continue;

		// DuringPhysics에서 게임 코드가 갱신한 Transform을 오래된 결과로 되돌리지 않는다.
		if (AreTransformsEqual(GetWorldTransform(*Snapshot->Component), Snapshot->WorldTransformBeforeUpdate))
			PendingBodies.Add(Snapshot->BodyId);
		else
			Snapshot->bHasPhysicsResult = false;
	}

	// 부모 결과가 반영된 상태에서 자식의 상대 Transform을 계산한다.
	std::stable_sort(PendingBodies.begin(), PendingBodies.end(), [this](uint64 A, uint64 B)
	{
		return GetAttachmentDepth((*BodySnapshots.Find(A))->Component)
			< GetAttachmentDepth((*BodySnapshots.Find(B))->Component);
	});

	for (uint64 BodyId : PendingBodies)
	{
		FPrimitiveTransformSnapshot** Found = BodySnapshots.Find(BodyId);
		if (!Found)
			continue;
		FPrimitiveTransformSnapshot* Snapshot = *Found;
		Snapshot->bHasPhysicsResult = false;
		UPrimitiveComponent* Component = Snapshot->Component;
		FTransform Relative;
		if (MakeRelativeTransform(*Component, Snapshot->UpdatedWorldTransform, Relative)
			&& !AreTransformsEqual(Component->GetTransform(), Relative))
			Component->SetTransform(Relative);
	}
}

void FPhysSceneChaos::StartFrame(float DeltaTime)
{
	assert(!bFrameStarted);

	Solver->SetBodyStates(CapturePhysicsInputs());
	Solver->AdvanceSolverBy(DeltaTime);
	BufferPhysicsResults();
	bFrameStarted = true;
}
void FPhysSceneChaos::EndFrame()
{
	if (!bFrameStarted)
		return;
	PullFromPhysicsState();
	bFrameStarted = false;
	// 콜백에서는 최신 Pose를 읽을 수 있다. 컴포넌트 제거도 이 시점 이후에 허용한다.
	EventManager->DispatchEvents(*this);
}
