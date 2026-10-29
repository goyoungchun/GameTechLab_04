#pragma once

#include "Container/Array.h"
#include "Container/Map.h"
#include "PhysicsBodyState.h"
#include "PhysicsCollision.h"
#include "Math/Box.h"

class UPrimitiveComponent;
class UBoxComponent;
class UCapsuleComponent;
class USphereComponent;
template <typename T> class TBVH;

// 실제 언리얼 구조
// 추후 필요에 따라 체계화 필요
class FPBDRigidsEvolutionBase
{

};

class FPBDRigidsEvolutionGBF : public FPBDRigidsEvolutionBase
{
public:
	FPBDRigidsEvolutionGBF();
	~FPBDRigidsEvolutionGBF();

	// Scene이 소유한 Component의 비소유 참조를 BodyId에 연결한다.
	void RegisterBody(uint64 BodyId, UPrimitiveComponent* Component);
	void UnregisterBody(uint64 BodyId);
	void AdvanceOneTimeStep(TArray<FPhysicsBodyState>& InOutBodyStates, float DeltaTime);
	const TArray<FPhysicsCollisionPair>& GetBroadPhasePairs() const;
	const TArray<FPhysicsCollisionResult>& GetCollisionResults() const;

private:
	void ApplyGravityAndIntegrate(TArray<FPhysicsBodyState>& InOutBodyStates, float DeltaTime);
	void UpdateBVHAndCollectPairs(const TArray<FPhysicsBodyState>& InBodyStates); // broad
	void DetectCollisionPairs(const TArray<FPhysicsBodyState>& InBodyStates); // narrow
	void ResolveContacts(TArray<FPhysicsBodyState>& InOutBodyStates, float DeltaTime);

	// 충돌하면 접촉점, 인자 A에서 B를 향하는 단위 법선, 침투 깊이를 OutResult에 기록하고 true를 반환한다.
	// Pose는 Component에서 읽지 않고 인자로 받은 이번 물리 Step의 Transform을 사용한다.
	bool SSCollisionTest(const USphereComponent* A, const FTransform& TransformA,
		const USphereComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult);
	bool SBCollisionTest(const USphereComponent* A, const FTransform& TransformA,
		const UBoxComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult);
	bool SCCollisionTest(const USphereComponent* A, const FTransform& TransformA,
		const UCapsuleComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult);
	bool BBCollisionTest(const UBoxComponent* A, const FTransform& TransformA,
		const UBoxComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult);
	bool BCCollisionTest(const UBoxComponent* A, const FTransform& TransformA,
		const UCapsuleComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult);
	bool CCCollisionTest(const UCapsuleComponent* A, const FTransform& TransformA,
		const UCapsuleComponent* B, const FTransform& TransformB, FPhysicsCollisionResult& OutResult);

	struct FPhysicsBVHBody
	{
		uint64 BodyId = 0;
		UPrimitiveComponent* Component = nullptr;
		int32 BodyStateIndex = -1;
		FBox Bounds;
	};

	TMap<uint64, UPrimitiveComponent*> BodyComponents;
	TArray<FPhysicsBVHBody> BVHBodies;
	TUniquePtr<TBVH<int32>> PhysicsBVH;
	TArray<FPhysicsCollisionPair> BroadTestResult;
	TArray<FPhysicsCollisionResult> CollisionResults;
};
