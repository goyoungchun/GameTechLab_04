#pragma once
#include "SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
	DECLARE_CLASS(USpringArmComponent, USceneComponent)

	REFLECT_START(ClassName)
		PROPERTY(TargetArmLength)
		PROPERTY(SocketOffset)
		PROPERTY(TargetOffset)
		PROPERTY(bDoCollisionTest)
		PROPERTY(ProbeSize)

		PROPERTY(bUsePawnControlRotation)
		PROPERTY(bInheritPitch)
		PROPERTY(bInheritYaw)
		PROPERTY(bInheritRoll)

		PROPERTY(CameraLagSpeed)
		PROPERTY(CameraRotationLagSpeed)

		PROPERTY(bEnableCameraLag)
		PROPERTY(bEnableCameraRotationLag)

		REFLECT_END()

public:
	USpringArmComponent() { PrimaryComponentTick.bCanEverTick = true; }

	void TickComponent(float DeltaTime) override;

	FRotator GetTargetRotation() const;

	virtual FRotator GetDesiredRotation() const;

public:
	// 충돌이 없을 때 기본 팔의 길이
	float TargetArmLength{ 300 };

	// SpringArm의 끝점에서 카메라 이동 Offset
	FVector SocketOffset;

	// SpringArm의 시작점 이동 -> 월드 기준으로 이동
	FVector TargetOffset;

	// 충돌 검사 여부
	bool bDoCollisionTest{ true };
	// 충돌 검사에 사용할 구체의 반지름
	float ProbeSize{ 12.f };

	// Pawn의 컨트롤러 회전 사용 여부
	bool bUsePawnControlRotation{ false };
	bool bInheritPitch{ true };
	bool bInheritYaw{ true };
	bool bInheritRoll{ true };

	// 지연에 사용될 값들
	// 이전 회전값
	FRotator PreviousDesiredRot;
	// 이전 위치값
	FVector PreviousDesiredLoc;

	bool bEnableCameraLag{ false };
	bool bEnableCameraRotationLag{ false };

	// 카메라 위치 보간 속도
	float CameraLagSpeed{ 10.f };
	// 카메라 회전 보간속도
	float CameraRotationLagSpeed{ 10.f };

	bool bLagInitialized{ false };

protected:
	// 최종 카메라 트랜스폼 갱신
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime);
};