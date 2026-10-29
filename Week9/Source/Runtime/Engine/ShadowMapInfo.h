#pragma once

#include "Core/Types.h"
#include "Math\Frustum.h"

struct FShadowViewCapture
{
	FMatrix ViewProjection;
	float ShadowBias;
	float ShadowSlopeBias;
};

struct FShadowView
{
	// 광원으로의 변환 행렬.
	FVector Forward;
	FMatrix View;
	FMatrix Projection;
	FMatrix ViewProjection;

	// 해상도 영역
	float Near;
	float Far;

	// 렌더링 플래그
	bool bIsOrthographic;

	// Bias
	float ShadowBias = 0.5f;
	float ShadowSlopeBias = 0.5f;
};

struct FShadowMapInfo
{
	// 광원의 기준점
	FVector Location;

	// 각 면 정보들
	TArray<FShadowView> ShadowViews;
};
