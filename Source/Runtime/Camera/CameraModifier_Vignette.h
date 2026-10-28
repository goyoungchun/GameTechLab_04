#pragma once
#include "CameraModifier.h"

// 화면 가장자리를 어둡게 하는 Modifier. 켜고 끌 때 Alpha In/Out으로 서서히 변한다.
class UCameraModifier_CameraVignette : public UCameraModifier
{
	DECLARE_CLASS(UCameraModifier_CameraVignette, UCameraModifier)

public:
	UCameraModifier_CameraVignette()
	{
		Priority = 220;        // LetterBox 다음
		AlphaInTime = 0.3f;    // 서서히 어두워짐
		AlphaOutTime = 0.5f;   // 서서히 풀림
	}

	void ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override
	{
		InOutPOV.VignetteIntensity = Intensity * GetAlpha();
		InOutPOV.VignetteRadius = Radius;
		InOutPOV.VignetteSoftness = Softness;
	}

	float Intensity = 0.5f;
	float Radius = 0.4f;
	float Softness = 0.6f;
};
