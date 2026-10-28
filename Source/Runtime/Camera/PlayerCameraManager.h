#pragma once

#include "GameFramework/Actor.h"
#include "CameraTypes.h"

enum class EViewTargetBlendFunction : uint8
{
	Linear,		// 단순 선형 보간
	Cubic,		// 약간의 Ease In / Out
	EaseIn,		// 부드럽게 가속, 타겟에 가까워져도 감속 안함
	EaseOut,	// 즉시 가속, 타겟에 가까워지면서 부드럽게 감속
	EaseInOut,	// 부드럽게 가속하고 감속
};

struct FViewTargetTransitionParams
{
	// 블렌딩 총 시간. 0은 블렌딩이 없다는 의미.
	float BlendTime = 0.0f;

	// 파라미터들 블렌딩할 때 사용할 함수
	EViewTargetBlendFunction BlendFunction = EViewTargetBlendFunction::Cubic;

	// 블렌딩 함수 곡선 모양을 조절하는 지수
	float BlendExp = 2.0f;

	// true면 블렌딩이 끝날 때까지 이전 ViewTarget을 직전 프레임의 카메라 POV로 고정
	// 이전 ViewTarget을 순간 이동시킬 예정이지만 블렌딩에는 영향을 주고 싶지 않을 때 유용
	bool bLockOutgoing = false;

	// 선형 블렌드 값 (진행률) 에 지정한 블렌드 함수를 적용하여 최종 블렌드 알파 반환
	float GetBlendAlpha(const float& TimePct) const
	{
		const float T = FMath::Clamp(TimePct, 0.0f, 1.0f);
		const float Exp = BlendExp > 0.0f ? BlendExp : 1.0f;

		switch (BlendFunction) 
		{
		case EViewTargetBlendFunction::Linear:
			return FMath::Lerp(0.f, 1.f, T);
		case EViewTargetBlendFunction::Cubic:
			return FMath::CubicInterpolation(0.f, 0.f, 1.f, 0.f, T);
		case EViewTargetBlendFunction::EaseInOut:
			return FMath::InterpEaseInOut(0.f, 1.f, T, Exp);
		case EViewTargetBlendFunction::EaseIn:
			return FMath::InterpEaseIn(0.f, 1.f, T, Exp);
		case EViewTargetBlendFunction::EaseOut:
			return FMath::InterpEaseOut(0.0f, 1.0f, T, Exp);
		}

		return T;
	}
};

struct FViewTarget
{
	AActor* Target = nullptr;
	FMinimalViewInfo POV;

	void SetNewTarget(AActor* NewTarget) { Target = NewTarget; }
	bool Equal(const FViewTarget& Other) const { return Target == Other.Target; }
};

class APlayerController;

class APlayerCameraManager : public AActor
{
	DECLARE_CLASS(APlayerCameraManager, AActor)

public:
	APlayerCameraManager();

	void OnActorDestroyed(AActor* Actor);

	void InitializeFor(APlayerController* PC);
	void SetViewTarget(AActor* NewTarget);
	void UpdateCamera(float DeltaTime);

	AActor* GetViewTarget() const { return ViewTarget.Target; }
	bool IsCameraCacheValid() const { return bCameraCacheValid; }
	const FMinimalViewInfo& GetCameraCacheView() const { return CameraCachePOV; }
	APlayerController* GetOwningPlayerController() const { return PCOwner; }

protected:
	void UpdateViewTarget(FViewTarget& OutVT, float DeltaTime);
	void FillCameraCache(const FMinimalViewInfo& NewInfo);
	void CheckViewTarget();

	APlayerController* PCOwner = nullptr;
	FViewTarget ViewTarget;
	FMinimalViewInfo CameraCachePOV;
	bool bCameraCacheValid = false;
};