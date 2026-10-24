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
	// TODO: BodyId별 속도와 이동 유형(Static/Kinematic/Dynamic), 중력 설정을 준비한다.
	// TODO: Dynamic Body에만 Velocity += Gravity * DeltaTime을 적용하고 예측 위치를 계산한다.
	// TODO: 속도는 프레임 사이에 유지하며, MovementComponent가 제어하는 Body는 적분하지 않는다.
	// TODO: 계산한 예측 Pose를 InOutBodyStates의 WorldTransform에 기록한다.
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

	const FVector AxesInA[6] =
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

	float MinDepth = FLT_MAX;
	FVector BestNormalInA = FVector::ZeroVector;

	for (const FVector& AxisInA : AxesInA)
	{
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

		// 한 축에서라도 분리되어 있으면 충돌하지 않는다.
		if (Depth < 0.0f)
			return false;

		// 가장 적게 이동해서 분리할 수 있는 축을 기억한다.
		if (Depth < MinDepth)
		{
			MinDepth = Depth;
			BestNormalInA =
				SignedDistance >= 0.0f ? AxisInA : -AxisInA;
		}
	}

	OutResult.Normal = RotationA.RotateVector(BestNormalInA).Normalized();
	OutResult.PenetrationDepth = MinDepth;
	
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
	// TODO: CollisionResults 중 Response가 Block인 결과를 순회한다. BodyId로 상태를 찾는다.
	// TODO: 물리적으로 막히는 접촉에 대해 침투 위치와 충돌 방향 속도를 보정한다.
	// TODO: Dynamic Body만 수정하고 Static/Kinematic Body의 위치는 유지한다.
	// TODO: 필요한 경우 역질량, 반발 계수, 마찰과 반복 횟수를 추가한다.
	// TODO: 보정한 Pose를 InOutBodyStates의 WorldTransform에 반영한다.
}

