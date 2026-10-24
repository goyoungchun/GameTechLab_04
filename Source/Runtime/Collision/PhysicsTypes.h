#pragma once

#include "Core/Types.h"

// Static은 고정, Kinematic은 게임 코드로 이동, Dynamic은 물리 시뮬레이션으로 이동한다.
enum class EPhysicsMotionType : uint8
{
	Static,
	Kinematic,
	Dynamic,
	Count
};

// 이동 유형과 독립적인 충돌체 분류다.
enum class ECollisionChannel : uint8
{
	WorldStatic,
	WorldDynamic,
	Count
};

enum class ECollisionResponse : uint8
{
	Ignore,
	Overlap,
	Block,
	Coount
};

// 기본값을 모두 block으로 설정 
// 언리얼엔진에서는 이런 기본값 프리셋이 존재하며 사용자가 직접 만들 수도 있음
struct FCollisionResponseContainer
{
	ECollisionResponse WorldStatic = ECollisionResponse::Block;
	ECollisionResponse WorldDynamic = ECollisionResponse::Block;

	ECollisionResponse GetResponse(ECollisionChannel Channel) const
	{
		switch (Channel)
		{
		case ECollisionChannel::WorldStatic: return WorldStatic;
		case ECollisionChannel::WorldDynamic: return WorldDynamic;
		default: return ECollisionResponse::Ignore;
		}
	}
};

// 양쪽 응답을 합친다. Ignore가 우선이며, Block은 양쪽 모두 Block일 때만 성립한다.
inline ECollisionResponse ResolveCollisionResponse(ECollisionResponse A, ECollisionResponse B)
{
	if (A == ECollisionResponse::Ignore || B == ECollisionResponse::Ignore)
		return ECollisionResponse::Ignore;
	if (A == ECollisionResponse::Overlap || B == ECollisionResponse::Overlap)
		return ECollisionResponse::Overlap;
	if (A == ECollisionResponse::Block && B == ECollisionResponse::Block)
		return ECollisionResponse::Block;
	return ECollisionResponse::Ignore;
}
