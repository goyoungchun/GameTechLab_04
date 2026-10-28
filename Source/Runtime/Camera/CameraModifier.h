#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"
#include "Camera/CameraTypes.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
	DECLARE_CLASS(UCameraModifier, UObject)

public:
	UCameraModifier() = default;
	~UCameraModifier() override = default;

	virtual void AddedToCamera(APlayerCameraManager* InCameraOwner);

	// 블렌드가 끝난 최종 POV를 수정. 앞선 Modifier의 결과에 더하는 방식으로 사용.
	virtual void ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) {}

	virtual bool IsFinished() const { return false; }

	virtual void EnableModifier();
	// bImmediate가 false면 AlphaOutTime 동안 사라진 뒤 꺼짐
	virtual void DisableModifier(bool bImmediate = false);

	// Modify 함수들보다 먼저 호출
	virtual void UpdateAlpha(float DeltaTime);

	bool IsDisabled() const { return bDisabled; }
	float GetAlpha() const { return Alpha; }
	uint8 GetPriority() const { return Priority; }
	APlayerCameraManager* GetCameraOwner() const { return CameraOwner; }

protected:
	APlayerCameraManager* CameraOwner = nullptr;

	// 작을수록 먼저 적용. 생성자에서 결정.
	uint8 Priority = 127;

	float AlphaInTime = 0.0f;
	float AlphaOutTime = 0.0f;
	float Alpha = 0.0f;
	bool bDisabled = false;
	bool bPendingDisable = false;
};
