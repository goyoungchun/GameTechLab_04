#include "EnginePCH.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "PlayerCameraManager.h"

APlayerCameraManager::APlayerCameraManager()
{
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
	PCOwner = PC;
	bCameraCacheValid = false;
	SetViewTarget(PC ? PC->GetPawn() : nullptr);
}

void APlayerCameraManager::SetViewTarget(AActor* NewTarget)
{
	if (!NewTarget && PCOwner)
		NewTarget = PCOwner->GetPawn();
	
	if (NewTarget == ViewTarget.Target)
		return;

	ViewTarget.SetNewTarget(NewTarget);
}

void APlayerCameraManager::CheckViewTarget()
{
	// 타겟이 없거나 파괴 대기 중이면 Pawn으로 복원
	if (ViewTarget.Target && !ViewTarget.Target->IsActorBeingDestroyed())
		return;

	APawn* Pawn = PCOwner ? PCOwner->GetPawn() : nullptr;

	if (Pawn && Pawn->IsActorBeingDestroyed())
		Pawn = nullptr;

	ViewTarget.SetNewTarget(Pawn);
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	CheckViewTarget();

	if (!ViewTarget.Target)
	{
		bCameraCacheValid = false;
		return;
	}

	UpdateViewTarget(ViewTarget, DeltaTime);
	FillCameraCache(ViewTarget.POV);
}

void APlayerCameraManager::UpdateViewTarget(FViewTarget& OutVT, float DeltaTime)
{
	FMinimalViewInfo POV;
	OutVT.Target->CalcCamera(POV);
	OutVT.POV = POV;
}

void APlayerCameraManager::FillCameraCache(const FMinimalViewInfo& NewInfo)
{
	CameraCachePOV = NewInfo;
	bCameraCacheValid = true;
}

void APlayerCameraManager::OnActorDestroyed(AActor* Actor)
{
	if (!Actor || ViewTarget.Target != Actor)
		return;

	ViewTarget.SetNewTarget(nullptr);
	bCameraCacheValid = false;
}