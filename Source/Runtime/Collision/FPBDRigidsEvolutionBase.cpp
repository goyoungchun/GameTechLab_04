#include "EnginePCH.h"

#include "FPBDRigidsEvolutionBase.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "Container/Set.h"
#include "GameFramework/Actor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <span>
#include <utility>

#include "Math/BVH.h"

namespace
{
	bool IsFinitePhysicsVector(const FVector& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	bool IsValidPhysicsBounds(const FBox& Bounds)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (!std::isfinite(Bounds.Min[Axis]) || !std::isfinite(Bounds.Max[Axis])
				|| Bounds.Min[Axis] > Bounds.Max[Axis])
				return false;
		}
		return true;
	}

	bool BoundsOverlap(const FBox& A, const FBox& B)
	{
		return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X
			&& A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y
			&& A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
	}

	// 접촉점 계산에 필요한 박스 정보. 두 박스 모두 A 좌표계로 표현한다.
	struct FContactBox
	{
		FVector Center;
		FVector Axes[3];
		FVector Extent;
	};

	// Normal.Dot(Point - Center) <= Limit인 영역에 다각형을 남긴다.
	void ClipContactPolygon(TArray<FVector>& Polygon, const FVector& Center,
		const FVector& Normal, float Limit)
	{
		if (Polygon.IsEmpty())
			return;

		constexpr float ClipTolerance = 1e-5f;
		TArray<FVector> Clipped;
		Clipped.Reserve(Polygon.Num() + 1);
		FVector Previous = Polygon.Last();
		float PreviousDistance = Normal.Dot(Previous - Center) - Limit - ClipTolerance;
		for (const FVector& Current : Polygon)
		{
			const float CurrentDistance = Normal.Dot(Current - Center) - Limit - ClipTolerance;
			const bool bPreviousInside = PreviousDistance <= 0.0f;
			const bool bCurrentInside = CurrentDistance <= 0.0f;
			if (bPreviousInside != bCurrentInside)
			{
				const float T = PreviousDistance / (PreviousDistance - CurrentDistance);
				Clipped.Add(Previous + (Current - Previous) * T);
			}
			if (bCurrentInside)
				Clipped.Add(Current);

			Previous = Current;
			PreviousDistance = CurrentDistance;
		}
		Polygon = std::move(Clipped);
	}

	// 두 선분에서 가장 가까운 점 쌍을 구하고, 두 번째 선분 위의 점을 반환한다.
	FVector ClosestPointOnSecondSegment(const FVector& P0, const FVector& P1,
		const FVector& Q0, const FVector& Q1)
	{
		constexpr float Epsilon = 1e-12f;
		const FVector D1 = P1 - P0;
		const FVector D2 = Q1 - Q0;
		const FVector R = P0 - Q0;
		const float A = D1.Dot(D1);
		const float E = D2.Dot(D2);
		const float F = D2.Dot(R);
		if (E <= Epsilon)
			return Q0;

		float S = 0.0f;
		float T = 0.0f;
		if (A <= Epsilon)
		{
			T = std::clamp(F / E, 0.0f, 1.0f);
		}
		else
		{
			const float C = D1.Dot(R);
			const float B = D1.Dot(D2);
			const float Denominator = A * E - B * B;
			if (Denominator > Epsilon * A * E)
				S = std::clamp((B * F - C * E) / Denominator, 0.0f, 1.0f);

			T = (B * S + F) / E;
			if (T < 0.0f)
			{
				T = 0.0f;
				S = std::clamp(-C / A, 0.0f, 1.0f);
			}
			else if (T > 1.0f)
			{
				T = 1.0f;
				S = std::clamp((B - C) / A, 0.0f, 1.0f);
			}
		}
		return Q0 + D2 * T;
	}

	// Direction 쪽 모서리를 선택하고, EdgeAxis를 따라 선분을 만든다.
	void GetContactEdge(const FContactBox& Box, int32 EdgeAxis,
		const FVector& Direction, FVector& OutStart, FVector& OutEnd)
	{
		FVector EdgeCenter = Box.Center;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Axis == EdgeAxis)
				continue;
			const float Sign = Box.Axes[Axis].Dot(Direction) >= 0.0f ? 1.0f : -1.0f;
			EdgeCenter += Box.Axes[Axis] * (Box.Extent[Axis] * Sign);
		}
		const FVector HalfEdge = Box.Axes[EdgeAxis] * Box.Extent[EdgeAxis];
		OutStart = EdgeCenter - HalfEdge;
		OutEnd = EdgeCenter + HalfEdge;
	}

	// 단일 접촉점을 B 표면의 대표점으로 정의한다. 면 접촉은 후보점들의 평균이다.
	// BestAxisIndex: 0~2는 A 면, 3~5는 B 면, 6~14는 모서리 조합이다.
	bool ComputeBoxContactPoint(const FContactBox& A, const FContactBox& B,
		const FVector& NormalAtoB, int32 BestAxisIndex, FVector& OutPoint)
	{
		if (BestAxisIndex < 0 || BestAxisIndex >= 15)
			return false;

		if (BestAxisIndex >= 6)
		{
			const int32 PairIndex = BestAxisIndex - 6;
			FVector P0, P1, Q0, Q1;
			GetContactEdge(A, PairIndex / 3, NormalAtoB, P0, P1);
			GetContactEdge(B, PairIndex % 3, -NormalAtoB, Q0, Q1);
			OutPoint = ClosestPointOnSecondSegment(P0, P1, Q0, Q1);
			return true;
		}

		const bool bReferenceIsA = BestAxisIndex < 3;
		const FContactBox& Reference = bReferenceIsA ? A : B;
		const FContactBox& Incident = bReferenceIsA ? B : A;
		const int32 ReferenceAxis = bReferenceIsA ? BestAxisIndex : BestAxisIndex - 3;
		const FVector ReferenceNormal = bReferenceIsA ? NormalAtoB : -NormalAtoB;
		const FVector ReferenceFaceCenter = Reference.Center
			+ ReferenceNormal * Reference.Extent[ReferenceAxis];

		// 기준 면과 가장 마주보는 상대 면을 선택한다.
		int32 IncidentAxis = 0;
		for (int32 Axis = 1; Axis < 3; ++Axis)
		{
			if (std::abs(Incident.Axes[Axis].Dot(ReferenceNormal))
				> std::abs(Incident.Axes[IncidentAxis].Dot(ReferenceNormal)))
				IncidentAxis = Axis;
		}
		const float IncidentSign = Incident.Axes[IncidentAxis].Dot(ReferenceNormal) >= 0.0f ? -1.0f : 1.0f;
		const FVector IncidentFaceCenter = Incident.Center
			+ Incident.Axes[IncidentAxis] * (Incident.Extent[IncidentAxis] * IncidentSign);
		const int32 IncidentU = (IncidentAxis + 1) % 3;
		const int32 IncidentV = (IncidentAxis + 2) % 3;
		const FVector U = Incident.Axes[IncidentU] * Incident.Extent[IncidentU];
		const FVector V = Incident.Axes[IncidentV] * Incident.Extent[IncidentV];
		TArray<FVector> Polygon
		{
			IncidentFaceCenter - U - V,
			IncidentFaceCenter + U - V,
			IncidentFaceCenter + U + V,
			IncidentFaceCenter - U + V
		};

		// 기준 면의 네 변에 대응하는 평면으로 상대 면을 잘라낸다.
		const int32 ReferenceU = (ReferenceAxis + 1) % 3;
		const int32 ReferenceV = (ReferenceAxis + 2) % 3;
		for (int32 SideAxis : { ReferenceU, ReferenceV })
		{
			ClipContactPolygon(Polygon, Reference.Center,
				Reference.Axes[SideAxis], Reference.Extent[SideAxis]);
			ClipContactPolygon(Polygon, Reference.Center,
				-Reference.Axes[SideAxis], Reference.Extent[SideAxis]);
		}
		ClipContactPolygon(Polygon, ReferenceFaceCenter, ReferenceNormal, 0.0f);
		if (Polygon.IsEmpty())
			return false;

		FVector Sum = FVector::ZeroVector;
		for (const FVector& Point : Polygon)
		{
			// A가 기준이면 후보는 B 표면 위에 있다. B가 기준이면 B 면에 투영한다.
			const float Separation = (Point - ReferenceFaceCenter).Dot(ReferenceNormal);
			Sum += bReferenceIsA ? Point : Point - ReferenceNormal * Separation;
		}
		OutPoint = Sum / static_cast<float>(Polygon.Num());
		return true;
	}

	ECollisionResponse GetPairResponse(const FPhysicsBodyState& A, const FPhysicsBodyState& B)
	{
		return ResolveCollisionResponse(A.CollisionResponses.GetResponse(B.ObjectChannel),
			B.CollisionResponses.GetResponse(A.ObjectChannel));
	}
}

FPBDRigidsEvolutionGBF::FPBDRigidsEvolutionGBF()
	: PhysicsBVH(MakeUnique<TBVH<int32>>([this](int32 Index) { return BVHBodies[Index].Bounds; }))
{
}

FPBDRigidsEvolutionGBF::~FPBDRigidsEvolutionGBF() = default;

void FPBDRigidsEvolutionGBF::RegisterBody(uint64 BodyId, UPrimitiveComponent* Component)
{
	if (BodyId != 0 && Component)
		BodyComponents.Add(BodyId, Component);
}

void FPBDRigidsEvolutionGBF::UnregisterBody(uint64 BodyId)
{
	BodyComponents.Remove(BodyId);
}

const TArray<FPhysicsCollisionPair>& FPBDRigidsEvolutionGBF::GetBroadPhasePairs() const
{
	return BroadTestResult;
}

const TArray<FPhysicsCollisionResult>& FPBDRigidsEvolutionGBF::GetCollisionResults() const
{
	return CollisionResults;
}

void FPBDRigidsEvolutionGBF::AdvanceOneTimeStep(TArray<FPhysicsBodyState>& InOutBodyStates, float DeltaTime)
{
	ApplyGravityAndIntegrate(InOutBodyStates, DeltaTime);
	UpdateBVHAndCollectPairs(InOutBodyStates);
	DetectCollisionPairs(InOutBodyStates);
	ResolveContacts(InOutBodyStates, DeltaTime);
}

void FPBDRigidsEvolutionGBF::ApplyGravityAndIntegrate(TArray<FPhysicsBodyState>& InOutBodyStates, float DeltaTime)
{
	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
		return;

	const FVector Gravity(0.0f, 0.0f, -9.8f);
	for (FPhysicsBodyState& State : InOutBodyStates)
	{
		if (State.MotionType == EPhysicsMotionType::Static)
			State.LinearVelocity = FVector::ZeroVector;
		if (!IsFinitePhysicsVector(State.LinearVelocity))
			State.LinearVelocity = FVector::ZeroVector;
		if (State.MotionType != EPhysicsMotionType::Dynamic)
			continue;

		// 반암시적 Euler 적분. 중력을 꺼도 기존 속도에 의한 이동은 유지한다.
		const FVector Velocity = State.LinearVelocity + (State.bEnableGravity ? Gravity * DeltaTime : FVector::ZeroVector);
		const FVector Location = State.WorldTransform.Location + Velocity * DeltaTime;
		if (!IsFinitePhysicsVector(Velocity) || !IsFinitePhysicsVector(Location))
			continue;
		State.LinearVelocity = Velocity;
		State.WorldTransform.Location = Location;
	}
}

void FPBDRigidsEvolutionGBF::UpdateBVHAndCollectPairs(const TArray<FPhysicsBodyState>& InBodyStates)
{
	BroadTestResult.Reset();
	TArray<FPhysicsBVHBody> UpdatedBodies;
	UpdatedBodies.Reserve(InBodyStates.Num());
	TSet<uint64> SeenBodyIds;
	SeenBodyIds.Reserve(InBodyStates.Num());

	for (int32 StateIndex = 0; StateIndex < InBodyStates.Num(); ++StateIndex)
	{
		const FPhysicsBodyState& State = InBodyStates[StateIndex];
		UPrimitiveComponent* const* Found = BodyComponents.Find(State.BodyId);
		if (!Found || !*Found || !(*Found)->IsRegistered())
			continue;

		UPrimitiveComponent* Component = *Found;
		if (const AActor* Owner = Component->GetOwner(); Owner && !Owner->GetActorEnableCollision())
			continue;
		// 현재 Narrow 단계가 지원하는 형상만 등록한다. 표시 여부는 충돌 활성화 여부와 무관하다.
		if (!Component->IsA<UBoxComponent>() && !Component->IsA<USphereComponent>() && !Component->IsA<UCapsuleComponent>())
			continue;

		// Bounds는 적분 이후의 입력 Pose를 기준으로 계산한다.
		const FBox Bounds = Component->CalcLocalBounds().GetWorldAABB(State.WorldTransform.GetLocalMatrix());
		if (!IsValidPhysicsBounds(Bounds) || !SeenBodyIds.Add(State.BodyId))
			continue;
		UpdatedBodies.Add({ State.BodyId, Component, StateIndex, Bounds });
	}

	bool bNeedsBuild = UpdatedBodies.Num() != BVHBodies.Num();
	if (!bNeedsBuild)
	{
		for (int32 Index = 0; Index < UpdatedBodies.Num(); ++Index)
		{
			if (UpdatedBodies[Index].BodyId != BVHBodies[Index].BodyId
				|| UpdatedBodies[Index].Component != BVHBodies[Index].Component)
			{
				bNeedsBuild = true;
				break;
			}
		}
	}
	BVHBodies = std::move(UpdatedBodies);

	if (BVHBodies.IsEmpty())
	{
		PhysicsBVH->Clear();
		return;
	}
	if (bNeedsBuild)
	{
		TArray<int32> BodyIndices;
		BodyIndices.Reserve(BVHBodies.Num());
		for (int32 Index = 0; Index < BVHBodies.Num(); ++Index)
			BodyIndices.Add(Index);
		PhysicsBVH->Build(std::span<const int32>(BodyIndices.GetData(), BodyIndices.Num()));
	}
	else
	{
		// Pose와 크기 변경을 반영한다. BVH는 배열 인덱스를 저장하므로 배열 재할당에도 안전하다.
		PhysicsBVH->Refit();
	}

	for (const FPhysicsBVHBody& BodyA : BVHBodies)
	{
		const FPhysicsBodyState& StateA = InBodyStates[BodyA.BodyStateIndex];
		PhysicsBVH->Query([&](const FBox& Bounds) { return BoundsOverlap(BodyA.Bounds, Bounds); },
			[&](int32 OtherIndex)
		{
			const FPhysicsBVHBody& BodyB = BVHBodies[OtherIndex];
			// 작은 BodyId에서 큰 BodyId로만 등록해 자기 자신과 역순 중복을 제외한다.
			if (BodyA.BodyId >= BodyB.BodyId || BodyA.Component == BodyB.Component)
				return;
			const FPhysicsBodyState& StateB = InBodyStates[BodyB.BodyStateIndex];
			if (StateA.MotionType == EPhysicsMotionType::Static && StateB.MotionType == EPhysicsMotionType::Static)
				return;
			const ECollisionResponse Response = GetPairResponse(StateA, StateB);
			if (Response == ECollisionResponse::Ignore)
				return;
			BroadTestResult.Add({ BodyA.Component, BodyB.Component, BodyA.BodyStateIndex, BodyB.BodyStateIndex, Response });
		});
	}
}

void FPBDRigidsEvolutionGBF::DetectCollisionPairs(const TArray<FPhysicsBodyState>& InBodyStates)
{
	CollisionResults.Reset();
	for (const FPhysicsCollisionPair& Pair : BroadTestResult)
	{
		UPrimitiveComponent* A = Pair.ComponentA;
		UPrimitiveComponent* B = Pair.ComponentB;
		if (!A || !B || A == B || Pair.Response == ECollisionResponse::Ignore
			|| !InBodyStates.IsValidIndex(Pair.BodyIndexA) || !InBodyStates.IsValidIndex(Pair.BodyIndexB))
			continue;

		const FPhysicsBodyState& StateA = InBodyStates[Pair.BodyIndexA];
		const FPhysicsBodyState& StateB = InBodyStates[Pair.BodyIndexB];
		if (StateA.BodyId == StateB.BodyId)
			continue;

		auto* BoxA = Cast<UBoxComponent>(A);
		auto* BoxB = Cast<UBoxComponent>(B);
		auto* SphereA = Cast<USphereComponent>(A);
		auto* SphereB = Cast<USphereComponent>(B);
		auto* CapsuleA = Cast<UCapsuleComponent>(A);
		auto* CapsuleB = Cast<UCapsuleComponent>(B);

		FPhysicsCollisionResult Result;
		bool bHit = false;
		bool bSwapped = false;
		if (BoxA && BoxB)
		{
			bHit = BBCollisionTest(BoxA, StateA.WorldTransform, BoxB, StateB.WorldTransform, Result);
		}
		else if (SphereA && SphereB)
		{
			bHit = SSCollisionTest(SphereA, StateA.WorldTransform, SphereB, StateB.WorldTransform, Result);
		}
		else if (CapsuleA && CapsuleB)
		{
			bHit = CCCollisionTest(CapsuleA, StateA.WorldTransform, CapsuleB, StateB.WorldTransform, Result);
		}
		else if (SphereA && BoxB)
		{
			bHit = SBCollisionTest(SphereA, StateA.WorldTransform, BoxB, StateB.WorldTransform, Result);
		}
		else if (BoxA && SphereB)
		{
			bHit = SBCollisionTest(SphereB, StateB.WorldTransform, BoxA, StateA.WorldTransform, Result);
			bSwapped = true;
		}
		else if (SphereA && CapsuleB)
		{
			bHit = SCCollisionTest(SphereA, StateA.WorldTransform, CapsuleB, StateB.WorldTransform, Result);
		}
		else if (CapsuleA && SphereB)
		{
			bHit = SCCollisionTest(SphereB, StateB.WorldTransform, CapsuleA, StateA.WorldTransform, Result);
			bSwapped = true;
		}
		else if (BoxA && CapsuleB)
		{
			bHit = BCCollisionTest(BoxA, StateA.WorldTransform, CapsuleB, StateB.WorldTransform, Result);
		}
		else if (CapsuleA && BoxB)
		{
			bHit = BCCollisionTest(BoxB, StateB.WorldTransform, CapsuleA, StateA.WorldTransform, Result);
			bSwapped = true;
		}

		if (!bHit)
			continue;

		// 혼합 형상을 검사할 때 바꾼 인자 순서를 원래 후보 쌍의 A -> B 방향으로 복원한다.
		if (bSwapped)
			Result.Normal = -Result.Normal;
		Result.BodyIdA = StateA.BodyId;
		Result.BodyIdB = StateB.BodyId;
		Result.Response = Pair.Response;
		CollisionResults.Add(Result);
	}
}

bool FPBDRigidsEvolutionGBF::SSCollisionTest(const USphereComponent* A, const FTransform& TransformA,
	const USphereComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult)
{
	if (!A || !B)
		return false;

	const float RadiusA = A->GetScaledSphereRadius(TransformA.Scale);
	const float RadiusB = B->GetScaledSphereRadius(TransformB.Scale);
	if (!(RadiusA > 0.0f) || !(RadiusB > 0.0f) || !std::isfinite(RadiusA) || !std::isfinite(RadiusB))
		return false;

	const FVector Delta = TransformB.Location - TransformA.Location;
	const float DistanceSquared = Delta.Dot(Delta);
	const float RadiusSum = RadiusA + RadiusB;
	if (!std::isfinite(DistanceSquared) || !std::isfinite(RadiusSum) || DistanceSquared > RadiusSum * RadiusSum)
		return false;

	const float Distance = std::sqrt(DistanceSquared);
	// 중심이 같아도 분리 가능한 단위 법선을 반환한다.
	const FVector Normal = Distance > 0.0f ? Delta / Distance : FVector(1.0f, 0.0f, 0.0f);
	const FVector PointOnA = TransformA.Location + Normal * RadiusA;
	const FVector PointOnB = TransformB.Location - Normal * RadiusB;
	OutResult.Normal = Normal;
	OutResult.PenetrationDepth = std::max(0.0f, RadiusSum - Distance);
	OutResult.ContactPoint = PointOnA * 0.5f + PointOnB * 0.5f;
	return true;
}

bool FPBDRigidsEvolutionGBF::SBCollisionTest(const USphereComponent* A, const FTransform& TransformA,
	const UBoxComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult)
{
	if (!A || !B)
		return false;

	const float Radius = A->GetScaledSphereRadius(TransformA.Scale);
	if (!(Radius > 0.0f) || !std::isfinite(Radius))
		return false;

	// 회전만 제거한 좌표계에서는 거리가 월드 단위를 유지한다.
	// 크기에 절댓값 Scale을 곱하므로 음수 및 0 Scale에서도 역행렬이 필요 없다.
	const FQuat BoxRotation = TransformB.GetOrientation().Normalized();
	const FVector Extent = B->GetBoxExtent() * TransformB.Scale.GetAbs();
	const FVector SphereCenter = BoxRotation.Inverse().RotateVector(TransformA.Location - TransformB.Location);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (!std::isfinite(Extent[Axis]) || Extent[Axis] < 0.0f || !std::isfinite(SphereCenter[Axis]))
			return false;
	}

	FVector ClosestPoint(
		std::clamp(SphereCenter.X, -Extent.X, Extent.X),
		std::clamp(SphereCenter.Y, -Extent.Y, Extent.Y),
		std::clamp(SphereCenter.Z, -Extent.Z, Extent.Z));
	const FVector Delta = ClosestPoint - SphereCenter;
	const float DistanceSquared = Delta.Dot(Delta);
	if (!std::isfinite(DistanceSquared) || DistanceSquared > Radius * Radius)
		return false;

	if (DistanceSquared > 0.0f)
	{
		const float Distance = std::sqrt(DistanceSquared);
		OutResult.Normal = BoxRotation.RotateVector(Delta / Distance);
		OutResult.PenetrationDepth = std::max(0.0f, Radius - Distance);
	}
	else
	{
		// 구 중심이 내부 또는 표면에 있다. 가장 가까운 면을 통해 구 전체가 빠져나가야 한다.
		const FVector FaceDistances = Extent - SphereCenter.GetAbs();
		int32 NearestAxis = 0;
		if (FaceDistances.Y < FaceDistances[NearestAxis]) NearestAxis = 1;
		if (FaceDistances.Z < FaceDistances[NearestAxis]) NearestAxis = 2;
		const float FaceSign = SphereCenter[NearestAxis] >= 0.0f ? 1.0f : -1.0f;
		FVector OutwardNormal;
		OutwardNormal[NearestAxis] = FaceSign;
		ClosestPoint[NearestAxis] = FaceSign * Extent[NearestAxis];
		// A를 -Normal 방향으로 밀어내면 가장 가까운 면 밖으로 분리된다.
		OutResult.Normal = -BoxRotation.RotateVector(OutwardNormal);
		OutResult.PenetrationDepth = Radius + std::max(0.0f, FaceDistances[NearestAxis]);
	}

	OutResult.ContactPoint = TransformB.Location + BoxRotation.RotateVector(ClosestPoint);
	return true;
}

bool FPBDRigidsEvolutionGBF::SCCollisionTest(const USphereComponent* A, const FTransform& TransformA,
	const UCapsuleComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult)
{
	if (!A || !B)
		return false;

	const float SphereRadius = A->GetScaledSphereRadius(TransformA.Scale);
	const FVector CapsuleScale = TransformB.Scale.GetAbs();
	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		if (!std::isfinite(CapsuleScale[AxisIndex]))
			return false;
	}
	// 구와 같은 최소 절댓값 Scale로 반구의 구형을 유지한다.
	// 전체 반높이는 로컬 Z축 Scale을 적용하며, 비균등 Scale에서도 Bounds 안에 유지한다.
	const float CapsuleRadius = B->GetCapsuleRadius() * std::min({ CapsuleScale.X, CapsuleScale.Y, CapsuleScale.Z });
	const float CapsuleHalfHeight = B->GetCapsuleHalfHeight() * CapsuleScale.Z;
	if (!(SphereRadius > 0.0f) || !(CapsuleRadius > 0.0f)
		|| !std::isfinite(SphereRadius) || !std::isfinite(CapsuleRadius) || !std::isfinite(CapsuleHalfHeight))
		return false;

	// CapsuleHalfHeight는 중심부터 가장 바깥까지의 거리.
	const float SegmentHalfLength = std::max(0.0f, CapsuleHalfHeight - CapsuleRadius);
	const FQuat CapsuleRotation = TransformB.GetOrientation().Normalized();
	const FVector Axis = CapsuleRotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));

	const FVector P0 = TransformB.Location - Axis * SegmentHalfLength;
	const FVector P1 = TransformB.Location + Axis * SegmentHalfLength;
	const FVector S = TransformA.Location;

	const FVector Segment = P1 - P0;
	const float LengthSquared = Segment.Dot(Segment);
	// Cpos1 ~ Cpos2 사이의 점을 만들어주는 0~1 값
	const float T = LengthSquared > 1e-8f ? std::clamp((S - P0).Dot(Segment) / LengthSquared, 0.0f, 1.0f) : 0.0f;
	const FVector Q = P0 + Segment * T;

	const FVector StoQ = Q - S;
	const float MinLen = StoQ.Length();
	const float MaxLen = SphereRadius + CapsuleRadius;
	if (!std::isfinite(MinLen) || !std::isfinite(MaxLen))
		return false;

	if (MinLen <= MaxLen)
	{
		// 중심축 위에서는 분리 방향이 유일하지 않으므로 축에 수직인 로컬 X 방향을 선택한다.
		const FVector normal = MinLen > 1e-6f ? StoQ / MinLen : CapsuleRotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
		const float depth = MaxLen - MinLen;

		OutResult.Normal = normal;
		OutResult.PenetrationDepth = depth;
		OutResult.ContactPoint = Q - normal * CapsuleRadius;

		return true;
	}

	return false;
}

bool FPBDRigidsEvolutionGBF::BBCollisionTest(const UBoxComponent* A, const FTransform& TransformA,
	const UBoxComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult)
{
	if (!A || !B)
		return false;

	const FQuat RotationA = TransformA.GetOrientation().Normalized();
	const FQuat RotationB = TransformB.GetOrientation().Normalized();
	const FQuat InvA = RotationA.Inverse();

	// A 좌표계에서 표현한 B의 세 축
	const FVector BAxisXInA = InvA.RotateVector(RotationB.RotateVector(FVector(1, 0, 0)));
	const FVector BAxisYInA = InvA.RotateVector(RotationB.RotateVector(FVector(0, 1, 0)));
	const FVector BAxisZInA = InvA.RotateVector(RotationB.RotateVector(FVector(0, 0, 1)));

	const FVector AExtent = A->GetBoxExtent() * TransformA.Scale.GetAbs();
	const FVector BExtent = B->GetBoxExtent() * TransformB.Scale.GetAbs();

	const FVector BPositionInA = InvA.RotateVector(TransformB.Location - TransformA.Location);

	FVector AxesInA[15] =
	{
		// A의 세 축
		FVector(1, 0, 0),
		FVector(0, 1, 0),
		FVector(0, 0, 1),

		// B의 세 축
		BAxisXInA,
		BAxisYInA,
		BAxisZInA
	};

	// A의 축 3개 × B의 축 3개 = 외적 축 9개
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 j = 0; j < 3; ++j)
		{
			AxesInA[6 + i * 3 + j] =
				AxesInA[i].Cross(AxesInA[3 + j]);
		}
	}

	constexpr float AxisLengthSquaredEpsilon = 1e-12f;

	float MinDepth = FLT_MAX;
	FVector BestNormalInA = FVector::ZeroVector;
	int32 BestAxisIndex = -1;

	for (int32 AxisIndex = 0; AxisIndex < 15; ++AxisIndex)
	{
		FVector AxisInA = AxesInA[AxisIndex];
		const float LengthSquared = AxisInA.Dot(AxisInA);

		if (!std::isfinite(LengthSquared))
			return false;

		// 거의 평행한 두 축의 외적은 검사 방향으로 사용할 수 없다.
		if (LengthSquared <= AxisLengthSquaredEpsilon)
			continue;

		// 모든 축에서 침투 깊이를 같은 거리 단위로 비교한다.
		AxisInA /= std::sqrt(LengthSquared);

		// A를 검사 축에 투영한 반크기
		const float ARadius =
			AExtent.X * std::abs(AxisInA.X) +
			AExtent.Y * std::abs(AxisInA.Y) +
			AExtent.Z * std::abs(AxisInA.Z);

		// B를 검사 축에 투영한 반크기
		const float BRadius =
			BExtent.X * std::abs(BAxisXInA.Dot(AxisInA)) +
			BExtent.Y * std::abs(BAxisYInA.Dot(AxisInA)) +
			BExtent.Z * std::abs(BAxisZInA.Dot(AxisInA));

		const float SignedDistance = BPositionInA.Dot(AxisInA);
		const float Distance = std::abs(SignedDistance);
		const float Depth = ARadius + BRadius - Distance;

		if (!std::isfinite(Depth))
			return false;

		// 한 축에서라도 분리되어 있으면 충돌하지 않는다.
		// Depth == 0이면 접촉한 상태로 취급한다.
		if (Depth < 0.0f)
			return false;

		// 최소 침투 깊이와 해당 축을 저장한다.
		if (Depth < MinDepth)
		{
			MinDepth = Depth;
			// A → B 방향으로 법선의 부호를 맞춘다.
			BestNormalInA = SignedDistance >= 0.0f ? AxisInA : -AxisInA;
			BestAxisIndex = AxisIndex;
		}
	}

	const FContactBox BoxA
	{
		FVector::ZeroVector,
		{ AxesInA[0], AxesInA[1], AxesInA[2] },
		AExtent
	};
	const FContactBox BoxB
	{
		BPositionInA,
		{ BAxisXInA, BAxisYInA, BAxisZInA },
		BExtent
	};
	FVector ContactInA;
	if (!ComputeBoxContactPoint(BoxA, BoxB, BestNormalInA, BestAxisIndex, ContactInA)
		|| !IsFinitePhysicsVector(ContactInA))
		return false;

	// Scale은 Extent에 이미 반영했으므로 회전과 이동만 적용한다.
	const FVector WorldContactPoint = TransformA.Location + RotationA.RotateVector(ContactInA);
	if (!IsFinitePhysicsVector(WorldContactPoint))
		return false;

	// A 좌표계에서 구한 법선을 월드 좌표계로 변환
	OutResult.Normal = RotationA.RotateVector(BestNormalInA).Normalized();
	OutResult.PenetrationDepth = MinDepth;
	OutResult.ContactPoint = WorldContactPoint;
	return true;
}

bool FPBDRigidsEvolutionGBF::BCCollisionTest(const UBoxComponent* A, const FTransform& TransformA,
	const UCapsuleComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult)
{
	// TODO: Box-Capsule 판정 성공 시 OutResult의 접촉점, A -> B 단위 법선, 침투 깊이를 채우고 true를 반환한다.
	return false;
}

bool FPBDRigidsEvolutionGBF::CCCollisionTest(const UCapsuleComponent* A, const FTransform& TransformA,
	const UCapsuleComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult)
{
	// TODO: Capsule-Capsule 판정 성공 시 OutResult의 접촉점, A -> B 단위 법선, 침투 깊이를 채우고 true를 반환한다.
	return false;
}

void FPBDRigidsEvolutionGBF::ResolveContacts(TArray<FPhysicsBodyState>& InOutBodyStates, float DeltaTime)
{
	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f || CollisionResults.IsEmpty())
		return;

	TMap<uint64, int32> BodyIndices;
	BodyIndices.Reserve(InOutBodyStates.Num());
	TArray<FVector> PredictedLocations;
	PredictedLocations.Reserve(InOutBodyStates.Num());
	for (int32 Index = 0; Index < InOutBodyStates.Num(); ++Index)
	{
		BodyIndices.Add(InOutBodyStates[Index].BodyId, Index);
		PredictedLocations.Add(InOutBodyStates[Index].WorldTransform.Location);
	}

	// 여러 접촉을 순차적으로 보정한다. 이미 분리한 양을 빼서 같은 깊이를 반복 적용하지 않는다.
	constexpr int32 SolverIterations = 16;
	for (int32 Iteration = 0; Iteration < SolverIterations; ++Iteration)
	{
		for (const FPhysicsCollisionResult& Contact : CollisionResults)
		{
			if (Contact.Response != ECollisionResponse::Block || Contact.BodyIdA == Contact.BodyIdB
				|| !IsFinitePhysicsVector(Contact.Normal) || !std::isfinite(Contact.PenetrationDepth)
				|| Contact.PenetrationDepth < 0.0f)
				continue;
			const int32* IndexA = BodyIndices.Find(Contact.BodyIdA);
			const int32* IndexB = BodyIndices.Find(Contact.BodyIdB);
			if (!IndexA || !IndexB)
				continue;

			FPhysicsBodyState& A = InOutBodyStates[*IndexA];
			FPhysicsBodyState& B = InOutBodyStates[*IndexB];
			// 질량 설정이 없는 현재 단계에서는 모든 Dynamic을 동일 질량으로 취급한다.
			const float InvMassA = A.MotionType == EPhysicsMotionType::Dynamic ? 1.0f : 0.0f;
			const float InvMassB = B.MotionType == EPhysicsMotionType::Dynamic ? 1.0f : 0.0f;
			const float InvMassSum = InvMassA + InvMassB;
			const float NormalLengthSquared = Contact.Normal.Dot(Contact.Normal);
			if (InvMassSum == 0.0f || !std::isfinite(NormalLengthSquared) || NormalLengthSquared <= 1e-8f)
				continue;
			const FVector Normal = Contact.Normal / std::sqrt(NormalLengthSquared);
			const FVector CorrectionA = A.WorldTransform.Location - PredictedLocations[*IndexA];
			const FVector CorrectionB = B.WorldTransform.Location - PredictedLocations[*IndexB];
			const float RemainingDepth = Contact.PenetrationDepth - (CorrectionB - CorrectionA).Dot(Normal);
			if (RemainingDepth > 0.0f)
			{
				const FVector Correction = Normal * (RemainingDepth / InvMassSum);
				if (InvMassA > 0.0f) A.WorldTransform.Location -= Correction * InvMassA;
				if (InvMassB > 0.0f) B.WorldTransform.Location += Correction * InvMassB;
			}

			// 반발 계수 0: 서로 접근하는 법선 속도만 제거하고 접선 속도와 분리 속도는 유지한다.
			const float NormalVelocity = (B.LinearVelocity - A.LinearVelocity).Dot(Normal);
			if (NormalVelocity < 0.0f)
			{
				const FVector Impulse = Normal * (-NormalVelocity / InvMassSum);
				if (InvMassA > 0.0f) A.LinearVelocity -= Impulse * InvMassA;
				if (InvMassB > 0.0f) B.LinearVelocity += Impulse * InvMassB;
			}
		}
	}
}

