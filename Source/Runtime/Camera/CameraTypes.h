#pragma once

#include "EnginePCH.h"

enum class ECameraProjectionMode : uint8
{
	Perspective,
	Orthographic
};

struct FMinimalViewInfo
{
	FVector Location = FVector(-5.0f, -10.0f, 10.0f);;
	FRotator Rotation = FRotator(30.0f, 45.0f, 0.0f);
	ECameraProjectionMode ProjectionMode = ECameraProjectionMode::Perspective;
	float AspectRatio = 16.0f / 9.0f;
	float FOV = 60.0f; // vertical field of view (unreal is horizontal)
	float OrthoWidth = 16.0f;
	float NearClipPlane = 0.1f;
	float FarClipPlane = 1000.0f;
	FVector FadeColor = FVector(0.0f, 0.0f, 0.0f);
	float FadeAmount = 0.0f;
	float LetterboxRatio = 0.0f;
	float VignetteIntensity = 0.0f;  // 0: 없음, 1: 가장자리가 완전히 검정
	float VignetteRadius = 0.4f;     // 어두워지기 시작하는 거리 (0: 중심, 1: 모서리)
	float VignetteSoftness = 0.6f;   // 어두워지는 구간의 폭

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

	void BlendViewInfo(const FMinimalViewInfo& Other, float Alpha)
	{
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

		Location = FMath::Lerp(Location, Other.Location, Alpha);
		Rotation = FMath::Slerp(Rotation, Other.Rotation, Alpha);
		FOV = FMath::Lerp(FOV, Other.FOV, Alpha);
		OrthoWidth = FMath::Lerp(OrthoWidth, Other.OrthoWidth, Alpha);
		NearClipPlane = FMath::Lerp(NearClipPlane, Other.NearClipPlane, Alpha);
		FarClipPlane = FMath::Lerp(FarClipPlane, Other.FarClipPlane, Alpha);
		AspectRatio = FMath::Lerp(AspectRatio, Other.AspectRatio, Alpha);

		if (Alpha >= 0.5f)
			ProjectionMode = Other.ProjectionMode;
	}
};