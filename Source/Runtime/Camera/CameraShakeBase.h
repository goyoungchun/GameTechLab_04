#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"
#include "CameraTypes.h"
#include "Sequence/SequenceData.h"


// CameraShakeBase : 흔들림 설정과 실행을 관리하는 객체
class UCameraShakeBase : public UObject
{
	DECLARE_CLASS(UCameraShakeBase, UObject)

public:
	void Start(const FCameraShakeParams& InParams);

	bool UpdateAndApply(float DeltaTime, FMinimalViewInfo& InOutPOV);

private:
	// Type에 따라 -1 ~ 1 사이 흔들림 값을 계산 (Axis: 0=Pitch, 1=Yaw, 2=Roll)
	float Wave(int32 Axis) const;

	FCameraShakeParams Params;
	float Elapsed = 0.0f;
	float Phase[3]{};
};