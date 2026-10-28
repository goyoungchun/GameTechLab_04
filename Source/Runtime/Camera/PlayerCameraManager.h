#pragma once

#include "GameFramework/Actor.h"
#include "CameraTypes.h"

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