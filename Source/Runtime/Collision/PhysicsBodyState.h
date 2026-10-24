#pragma once

#include "Core/Types.h"
#include "Math/Transform.h"
#include "PhysicsTypes.h"

// Scene과 Solver 사이에 전달하는 값 데이터. Component 포인터는 포함하지 않는다.
struct FPhysicsBodyState
{
	uint64 BodyId = 0;
	FTransform WorldTransform;
	// 월드 좌표의 선속도. Dynamic의 이전 Step 결과는 Solver가 BodyId로 이어받는다.
	FVector LinearVelocity = FVector::ZeroVector;

	// Scene에서 입력 상태를 구성할 때 Component의 설정을 값으로 복사한다.
	EPhysicsMotionType MotionType = EPhysicsMotionType::Static;
	ECollisionChannel ObjectChannel = ECollisionChannel::WorldStatic;
	FCollisionResponseContainer CollisionResponses;
	bool bEnableGravity = true;
};
