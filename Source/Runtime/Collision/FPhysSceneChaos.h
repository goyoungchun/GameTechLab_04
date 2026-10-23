#pragma once

#include "FChaosScene.h"
#include "Container/Array.h"
#include "Container/Map.h"
#include "Core/Types.h"
#include "Math/Transform.h"

class UPrimitiveComponent;
class FPBDRigidsSolver;
class UWorld;

// 언리얼 엔진에서는 FSingleParticlePhysicsProxy를 사용함.
// 추후 필요시 임시 구조체 제거 및 체계화 필요함
struct FPrimitiveTransformSnapshot
{
	uint64 BodyId = 0;
	UPrimitiveComponent* Component = nullptr;
	FTransform WorldTransformBeforeUpdate;
	FTransform UpdatedWorldTransform;
	bool bHasPhysicsResult = false;
};

class FPhysSceneChaos : public FChaosScene
{

public:
	explicit FPhysSceneChaos(UWorld* InWorld);
	~FPhysSceneChaos();

	bool RegisterPrimitive(UPrimitiveComponent* inPrimitive);
	bool UnRegisterPrimitive(UPrimitiveComponent* inPrimitive);

	void StartFrame(float DeltaTime);
	void EndFrame();
	FPBDRigidsSolver& GetSolver() { return *Solver; }
	int32 GetNumPrimitives() const { return PrimitiveSnapShots.Num(); }

	// 매 프레임 업데이트 직전 : Component -> PrimitiveSnapshots
	void CaptureTransformsBeforeUpdate();
	// Solver 결과 -> PrmitiveSnapshots
	void BufferPhysicsResults();
	// PrimitiveSnapShots -> Component
	void PullFromPhysicsState(); 

private:
	TArray<FPrimitiveTransformSnapshot*> PrimitiveSnapShots = {};
	TMap<uint64, FPrimitiveTransformSnapshot*> BodySnapshots;
	UWorld* OwningWorld = nullptr;
	TUniquePtr<FPBDRigidsSolver> Solver;
	uint64 NextBodyId = 1;
	bool bFrameStarted = false;
};
