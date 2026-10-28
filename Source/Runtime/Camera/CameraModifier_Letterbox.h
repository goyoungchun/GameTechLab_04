#pragma once
#include "CameraModifier.h"

class UCameraModifier_CameraLetterbox : public UCameraModifier
{
	DECLARE_CLASS(UCameraModifier_CameraLetterbox, UCameraModifier)

public:
	UCameraModifier_CameraLetterbox()
	{
		Priority = 210;
		AlphaInTime = 0.5f;
		AlphaOutTime = 0.5f;
	}

	void ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override
	{
		InOutPOV.LetterboxRatio = TargetRatio * GetAlpha();
	}

	float TargetRatio = 0.12;
};