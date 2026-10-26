#include "EnginePCH.h"

#include "FPhysSceneChaos.h"
#include "FPBDRigidsSolver.h"
#include "FEventManger.h"
#include "PhysicsSweep.h"
#include "Component/PrimitiveComponent.h"
#include "Component/MovementComponent.h"
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

void FPhysSceneChaos::ResetPrimitiveVelocity(const UPrimitiveComponent* Primitive)
{
	for (const FPrimitiveTransformSnapshot* Snapshot : PrimitiveSnapShots)
		if (Snapshot->Component == Primitive)
			Solver->ResetBodyVelocity(Snapshot->BodyId);
}

void FPhysSceneChaos::GatherSweepCandidates(const FBox& Bounds, TArray<UPrimitiveComponent*>& OutCandidates) const
{
	OutCandidates.Reset();
	for (const FPrimitiveTransformSnapshot* Snapshot : PrimitiveSnapShots)
	{
		UPrimitiveComponent* Component = Snapshot->Component;
		if (!Component || !Component->IsRegistered() || Component->GetWorld() != OwningWorld)
			continue;
		const FBox Candidate = Component->CalcBounds();
		if (Candidate.Min.X <= Bounds.Max.X && Candidate.Max.X >= Bounds.Min.X
			&& Candidate.Min.Y <= Bounds.Max.Y && Candidate.Max.Y >= Bounds.Min.Y
			&& Candidate.Min.Z <= Bounds.Max.Z && Candidate.Max.Z >= Bounds.Min.Z)
			OutCandidates.Add(Component);
	}
}

void FPhysSceneChaos::RecordSweepResult(UPrimitiveComponent* MovingComponent, const FHitResult& Hit, float Radius)
{
	if (!MovingComponent || !Hit.HitComponent || MovingComponent == Hit.HitComponent)
		return;
	FPhysicsSweepResult Result;
	for (const FPrimitiveTransformSnapshot* Snapshot : PrimitiveSnapShots)
	{
		if (Snapshot->Component == MovingComponent) Result.Contact.BodyIdA = Snapshot->BodyId;
		if (Snapshot->Component == Hit.HitComponent) Result.Contact.BodyIdB = Snapshot->BodyId;
	}
	if (Result.Contact.BodyIdA == 0 || Result.Contact.BodyIdB == 0)
		return;
	Result.Contact.ContactPoint = Hit.ImpactPoint;
	Result.Contact.Normal = -Hit.Normal;
	Result.Contact.PenetrationDepth = Hit.PenetrationDepth;
	Result.Contact.Response = ResolveCollisionResponse(
		MovingComponent->GetCollisionResponseToChannel(Hit.HitComponent->GetObjectChannel()),
		Hit.HitComponent->GetCollisionResponseToChannel(MovingComponent->GetObjectChannel()));
	Result.Contact.bFromSweep = true;
	Result.Contact.SweepTime = Hit.Time;
	Result.Contact.SweepDistance = Hit.Distance;
	Result.MovingBodyId = Result.Contact.BodyIdA;
	Result.Radius = Radius;
	EventManager->QueueSweepResult(Result);
}

bool FPhysSceneChaos::IsSweepOverlapping(const FPhysicsSweepResult& Result) const
{
	UPrimitiveComponent* Moving = FindPrimitive(Result.MovingBodyId);
	const uint64 OtherId = Result.MovingBodyId == Result.Contact.BodyIdA ? Result.Contact.BodyIdB : Result.Contact.BodyIdA;
	UPrimitiveComponent* Other = FindPrimitive(OtherId);
	if (!Moving || !Other) return false;
	FHitResult Hit;
	return SweepSphereAgainstComponent(FRay{ Moving->GetWorldLocation(), FVector::ZeroVector }, 0.0f, Result.Radius, Other, Hit);
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
		State.Mass = Component->GetMass();
		State.ObjectChannel = Component->GetObjectChannel();
		State.CollisionResponses = Component->GetCollisionResponses();
		State.bEnableGravity = Component->IsGravityEnabled();
		State.GravityScale = Component->GetGravityScale();
		State.Acceleration = Component->GetAcceleration();
		State.COF = Component->GetCOF();
		State.Restitution = Component->GetRestitution();
		// MovementComponent가 같은 Component나 부모를 이동하면 물리가 중복 적분하지 않는다.
		if (const AActor* Owner = Component->GetOwner())
		{
			for (UActorComponent* OwnedComponent : Owner->GetComponents())
			{
				const auto* Movement = Cast<UMovementComponent>(OwnedComponent);
				if (!Movement || !Movement->IsRegistered() || !Movement->IsActive() || !Movement->IsComponentTickEnabled())
					continue;
				for (const USceneComponent* Target = Component; Target; Target = Target->GetAttachParent())
				{
					if (Movement->GetUpdatedComponent() != Target)
						continue;
					State.MotionType = EPhysicsMotionType::Kinematic;
					State.LinearVelocity = Movement->GetVelocity();
					break;
				}
			}
		}
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
		// Static/Kinematic 자식은 부모 Transform을 따른다. 입력 Pose를 다시 쓰면
		// Dynamic 부모가 이동한 뒤 자식 메시만 이전 월드 위치로 되돌아간다.
		if (Result.MotionType != EPhysicsMotionType::Dynamic)
			continue;

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
