#pragma once

#include "Core/Types.h"
#include "Math/Transform.h"

// Scene과 Solver 사이에 전달하는 값 데이터. Component는 Scene에서만 참조한다.
struct FPhysicsBodyState
{
	uint64 BodyId = 0;
	FTransform WorldTransform;
};
