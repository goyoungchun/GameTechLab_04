#include "EnginePCH.h"
#include "CameraModifier_CameraFade.h"

UCameraModifier_CameraFade::UCameraModifier_CameraFade()
{
	Priority = 200;
}

void UCameraModifier_CameraFade::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	if (!bFading)
	{
		return;
	}

	Elapsed += DeltaTime;
	const float T = Duration > 0.0f ? FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f) : 1.0f;

	if (T >= 1.0f && !bHold)
	{
		bFading = false;
		return;
	}

	InOutPOV.FadeColor = FadeColor;
	InOutPOV.FadeAmount = FMath::Lerp(FromAlpha, ToAlpha, T) * GetAlpha();
}

void UCameraModifier_CameraFade::StartFade(float InFromAlpha, float InToAlpha, float InDuration, const FVector& Color, bool bHoldWhenFinished)
{
	FromAlpha = FMath::Clamp(InFromAlpha, 0.0f, 1.0f);
	ToAlpha = FMath::Clamp(InToAlpha, 0.0f, 1.0f);
	Duration = std::max(InDuration, 0.0f);
	FadeColor = Color;
	bHold = bHoldWhenFinished;
	Elapsed = 0.0f;
	bFading = true;
}

void UCameraModifier_CameraFade::StopFade()
{
	bFading = false;
}
