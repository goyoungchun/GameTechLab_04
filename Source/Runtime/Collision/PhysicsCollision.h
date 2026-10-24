#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"
#include "PhysicsTypes.h"

class UPrimitiveComponent;

// Broad 단계에서 채운다. 인덱스는 같은 Step의 InBodyStates를 가리킨다.
struct FPhysicsCollisionPair
{
	UPrimitiveComponent* ComponentA = nullptr;
	UPrimitiveComponent* ComponentB = nullptr;
	int32 BodyIndexA = -1;
	int32 BodyIndexB = -1;
	ECollisionResponse Response = ECollisionResponse::Block;
};

// 정밀 검사에 성공한 쌍의 결과. 접촉점과 법선은 월드 좌표 기준이다.
struct FPhysicsCollisionResult
{
	uint64 BodyIdA = 0;
	uint64 BodyIdB = 0;
	FVector ContactPoint;
	// A를 -Normal, B를 +Normal 방향으로 밀어내는 단위 접촉 법선.
	FVector Normal;
	float PenetrationDepth = 0.0f;
	ECollisionResponse Response = ECollisionResponse::Block;
};
