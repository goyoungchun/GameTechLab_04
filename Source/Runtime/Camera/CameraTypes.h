#pragma once

#include "EnginePCH.h"

enum class ECameraProjectionMode : uint8
{
	Perspective,
	Orthographic
};

struct FMinimalViewInfo
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::Identitiy;
	ECameraProjectionMode ProjectionMode = ECameraProjectionMode::Perspective;
	float AspectRatio = 16.0f / 9.0f;
	float FOV = 60.0f; // vertical field of view (unreal is horizontal)
	float OrthoWidth = 16.0f;
	float NearClipPlane = 0.1f;
	float FarClipPlane = 1000.0f;

	FMatrix CalculateViewMatrix() const
	{
		return FMatrix::MakeView(Location, Rotation);
	}

	FMatrix CalculateProjectionMatrix() const 
	{
		if (ProjectionMode == ECameraProjectionMode::Perspective)
			return FMatrix::MakePerspective(FOV, AspectRatio, NearClipPlane, FarClipPlane);
		else
			return FMatrix::MakeOrthographic(OrthoWidth, OrthoWidth / AspectRatio, NearClipPlane, FarClipPlane);
	}
};