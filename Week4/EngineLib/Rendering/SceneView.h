#pragma once

#include "../Core/Math/Matrix.h"
#include "../Core/Math/Rotator.h"
#include "../Core/Math/Vector.h"
#include "../Core/enum.h"

class FCamera;

struct FViewRect
{
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;

	bool isValid() const { return Width > 0.0f && Height > 0.0f; }
	float getAspectRatio() const { return isValid() ? Width / Height : 1.0f; }
};

struct FSceneView
{
	FViewRect Rect;

	FMatrix viewMatrix = FMatrix::Identity;
	FMatrix projectionMatrix = FMatrix::Identity;
	FMatrix viewProjectionMatrix = FMatrix::Identity;
	FMatrix inverseViewprojectionMatrix = FMatrix::Identity;

	FVector cameraLocation;
	FVector cameraForward;
	FVector cameraRight;
	FVector cameraUp;
	FRotator cameraRotation;

	float fovDegree = 60.0f;
	float orthoDistance = 5.0f;
	float projectionRatio = 1.0f;

	float nearZ = 0.1f;
	float farZ = 1000.0f;

	bool isValid() const { return Rect.isValid(); }


	EViewModeIndex viewMode = EViewModeIndex::VMI_Lit;
	uint32 showFlags = 0;

	bool HasShowFlag(EEngineShowFlags flag) const { return (showFlags & static_cast<uint32>(flag)) != 0; }
};

FSceneView makeSceneView(const FCamera& camera, const FViewRect& rect, float projectionratio);
