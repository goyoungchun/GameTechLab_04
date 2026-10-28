#pragma once

#include "CameraModifier.h"
#include "Sequence/SequenceData.h"

class UCameraShakeBase;

class UCameraModifier_CameraShake : public UCameraModifier
{
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

public: 
	UCameraModifier_CameraShake();
	~UCameraModifier_CameraShake() override;

	void ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

	void AddCameraShake(const FCameraShakeParams& Params);
	void StopAllCameraShakes();
	void RemoveOwnedShake(UCameraShakeBase* Shake);

private:
	TArray<UCameraShakeBase*> ActiveShakes;
};