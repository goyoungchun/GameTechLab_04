#pragma once

#include "HitResult.h"
#include "Ray.h"

class UPrimitiveComponent;

// 이동체는 기존 Movement 경로와 같이 구로 근사한다. 거리 0은 현재 위치의 겹침 검사다.
// Box/Sphere/Capsule은 실제 형상, 그 외 메시 컴포넌트는 기존과 같이 월드 Bounds를 사용한다.
bool SweepSphereAgainstComponent(const FRay& Ray, float MaxDistance, float Radius,
	UPrimitiveComponent* Target, FHitResult& OutHit);
