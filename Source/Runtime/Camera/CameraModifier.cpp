#include "EnginePCH.h"
#include "CameraModifier.h"

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraOwner)
{
	CameraOwner = InCameraOwner;
}

void UCameraModifier::EnableModifier()
{
	bDisabled = false;
	bPendingDisable = false;
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
		Alpha = 0.0f;
	}
	else if (!bDisabled)
	{
		bPendingDisable = true;
	}
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
	const float TargetAlpha = bPendingDisable ? 0.0f : 1.0f;
	const float BlendTime = bPendingDisable ? AlphaOutTime : AlphaInTime;

	if (BlendTime <= 0.0f)
		Alpha = TargetAlpha;
	else if (Alpha < TargetAlpha)
		Alpha = std::min(Alpha + DeltaTime / BlendTime, TargetAlpha);
	else
		Alpha = std::max(Alpha - DeltaTime / BlendTime, TargetAlpha);

	// 서서히 끄는 중이었고 다 사라졌으면 완전히 끔.
	if (bPendingDisable && Alpha <= 0.0f)
	{
		bDisabled = true;
		bPendingDisable = false;
	}
}