#pragma once
#include "CameraModifier.h"
#include "Sequence/SequenceData.h"

class UCameraModifier_CameraFade : public UCameraModifier
{
	DECLARE_CLASS(UCameraModifier_CameraFade, UCameraModifier)

public:
	UCameraModifier_CameraFade();

	void ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

	void StartFade(float FromAlpha, float ToAlpha, float Duration, const FVector& Color, bool bHoldWhenFinished, const Bezier& InEase = Bezier{});
	void StopFade();

private:
	FVector FadeColor = FVector(0.0f, 0.0f, 0.0f);
	float FromAlpha{ 0.0f };
	float ToAlpha{ 0.0f };
	float Duration{ 0.0f };
	float Elapsed{ 0.0f };
	bool bFading{ false };
	bool bHold{ false };
	Bezier Ease;

};
