#pragma once

#include "Container/Array.h"
#include "PhysicsCollision.h"

#include <map>
#include <utility>

class FPhysSceneChaos;

class FEventManger
{
public:
	FEventManger();
	~FEventManger();

	// 물리 결과를 복사한 뒤, Scene이 Transform을 반영한 EndFrame에서 통지한다.
	void BufferCollisionResults(const TArray<FPhysicsCollisionResult>& InResults);
	void DispatchEvents(const FPhysSceneChaos& Scene);

private:
	using FBodyPair = std::pair<uint64, uint64>;
	struct FOverlapState
	{
		bool bNotifiedA = false;
		bool bNotifiedB = false;
	};

	void NotifyBeginOverlap(const FBodyPair& Pair, bool bForA,
		const FPhysicsCollisionResult& Result, const FPhysSceneChaos& Scene);
	void NotifyEndOverlap(const FBodyPair& Pair, const FOverlapState& State, const FPhysSceneChaos& Scene);
	void NotifyHit(const FBodyPair& Pair, bool bForA,
		const FPhysicsCollisionResult& Result, const FPhysSceneChaos& Scene);
	void EndInvalidOverlaps(const FPhysSceneChaos& Scene);

	TArray<FPhysicsCollisionResult> PendingResults;
	// BodyId만 보관하므로 제거된 컴포넌트 포인터가 다음 프레임까지 남지 않는다.
	std::map<FBodyPair, FOverlapState> ActiveOverlaps;
	bool bHasBufferedResults = false;
	bool bDispatching = false;
};
