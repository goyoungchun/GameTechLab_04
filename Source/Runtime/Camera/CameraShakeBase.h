#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"
#include "CameraTypes.h"
#include "Sequence/SequenceData.h"

class UCameraModifier_CameraShake;


// CameraShakeBase : 흔들림 설정과 실행을 관리하는 객체
class UCameraShakeBase : public UObject
{
	DECLARE_CLASS(UCameraShakeBase, UObject)

public:
	~UCameraShakeBase() override;

	void Start(const FCameraShakeParams& InParams);
	void SetOwnerModifier(UCameraModifier_CameraShake* InOwner) { OwnerModifier = InOwner; }

	bool UpdateAndApply(float DeltaTime, FMinimalViewInfo& InOutPOV, float Scale = 1.0f);

private:
	// Type에 따라 -1 ~ 1 사이 흔들림 값을 계산 (Axis: 0=Pitch, 1=Yaw, 2=Roll)
	float Wave(int32 Axis) const;

	UCameraModifier_CameraShake* OwnerModifier = nullptr;
	FCameraShakeParams Params;
	float Elapsed = 0.0f;
	float Phase[3]{};
};