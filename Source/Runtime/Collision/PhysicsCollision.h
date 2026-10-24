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
	// Sweep에서 생성한 이벤트에 한해 이동 경로의 시간과 거리를 보존한다.
	bool bFromSweep = false;
	float SweepTime = 0.0f;
	float SweepDistance = 0.0f;
};

// 이동체와 검사 반지름으로 최종 위치의 겹침 유지 여부를 확인한다.
struct FPhysicsSweepResult
{
	FPhysicsCollisionResult Contact;
	uint64 MovingBodyId = 0;
	float Radius = 0.0f;
};
