#pragma once

#include "Math/Matrix.h"

constexpr uint32 ObjectSlotBytes = 256;

struct FPerObjectConstants
{
	FMatrix World;
	FMatrix NormalMatrix;

	uint32 bReceiverShadow = 1;
	uint32 Padding[3] = {};
};

inline FPerObjectConstants MakePerObjectConstants(const FMatrix& World, bool ReceiverShadow)
{
	FPerObjectConstants Result{};
	Result.World = World;
	Result.NormalMatrix = World.Inverse().GetTransposed();
	Result.bReceiverShadow = ReceiverShadow ? 1u : 0u;
	return Result;
}

static_assert(sizeof(FPerObjectConstants) == 144);
static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

