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

void APlayerCameraManager::SetViewTarget(AActor* NewTarget, const FViewTargetTransitionParams& TransitionParams)
{
	if (!NewTarget && PCOwner)
		NewTarget = PCOwner->GetPawn();
	
	// 보여줄 대상이 없음 -> 블렌드 취소 후 비우기
	if (!NewTarget)
	{
		AssignViewTarget(nullptr);
		return;
	}

	if (IsBlending())
	{
		// 이미 해당 타겟으로 가는 중이면 타이머 재시작 안함
		if (NewTarget == PendingViewTarget.Target)
			return;
	}
	else if (NewTarget == ViewTarget.Target)
	{
		return;
	}

	// 블렌드 시간이 없거나, 아직 화면에 보여준 적이 없으면 즉시 전환
	if (TransitionParams.BlendTime <= 0.0f || !bCameraCacheValid)
	{
		AssignViewTarget(NewTarget);
		return;
	}

	// 블렌드 중 끼어들기 (원래 타겟으로 돌아가는 경우 포함) 또는 Outgoing 고정 요청
	// -> 지금 화면에 보이는 POV에서 출발
	if (IsBlending() || TransitionParams.bLockOutgoing)
		SnapshotOutgoing(CameraCachePOV);

	StartBlend(NewTarget, TransitionParams);
}

void APlayerCameraManager::CheckViewTarget()
{
	// 블렌드 중 타겟 없는 Outgoing은 고정된 스냅샷이므로 건드리지 않음
	if (IsBlending() || ViewTarget.Target)
		return;

	// 보여줄 대상이 없으면 Pawn으로 복원
	// 파괴 처리는 OnActorDestroyed 담당
	ViewTarget.SetNewTarget(PCOwner ? PCOwner->GetPawn() : nullptr);
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	CheckViewTarget();

	if (!IsBlending() && !ViewTarget.Target)
	{
		bCameraCacheValid = false;
		return;
	}

	// Outgoing은 타겟이 있을 때만 갱신 (없으면 고정된 스냅샷)
	if (ViewTarget.Target)
		UpdateViewTarget(ViewTarget, DeltaTime);

	if (!IsBlending())
	{
		FillCameraCache(ViewTarget.POV);
		return;
	}

	UpdateViewTarget(PendingViewTarget, DeltaTime);
	BlendTimeToGo -= DeltaTime;

	if (BlendTimeToGo > 0.0f)
	{
		const float TimePct = 1.0f - BlendTimeToGo / BlendParams.BlendTime;
		const float BlendAlpha = BlendParams.GetBlendAlpha(TimePct);

		FMinimalViewInfo BlendedPOV = ViewTarget.POV;
		BlendedPOV.BlendViewInfo(PendingViewTarget.POV, BlendAlpha);
		FillCameraCache(BlendedPOV);
		return;
	}

	// 블렌딩 종료
	ViewTarget.POV = PendingViewTarget.POV;
	AssignViewTarget(PendingViewTarget.Target);
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

AActor* APlayerCameraManager::GetViewTarget() const
{
	return IsBlending() ? PendingViewTarget.Target : ViewTarget.Target;
}

void APlayerCameraManager::SnapshotOutgoing(const FMinimalViewInfo& OnScreenPOV)
{
	ViewTarget.POV = OnScreenPOV;
	ViewTarget.SetNewTarget(nullptr);
}

void APlayerCameraManager::AssignViewTarget(AActor* NewTarget)
{
	ViewTarget.SetNewTarget(NewTarget);
	PendingViewTarget.SetNewTarget(nullptr);
	BlendTimeToGo = 0.0f;
}

void APlayerCameraManager::StartBlend(AActor* NewTarget, const FViewTargetTransitionParams& Params)
{
	PendingViewTarget.SetNewTarget(NewTarget);
	BlendParams = Params;
	BlendTimeToGo = Params.BlendTime;
}

void APlayerCameraManager::OnActorDestroyed(AActor* Actor)
{
	if (!Actor)
		return;

	if (Actor == PCOwner)
		PCOwner = nullptr;

	// 보고 있던 (Outgoing) 타겟이 사라짐
	if (Actor == ViewTarget.Target)
	{
		// 블렌드 중이면 남은 POV가 스냅샷이 되어 블렌드가 사라짐
		ViewTarget.SetNewTarget(nullptr);
		if (!IsBlending())
			bCameraCacheValid = false;
	}

	// 가고 있던 (Pending) 타겟이 사라짐
	if (Actor == PendingViewTarget.Target)
	{
		APawn* Pawn = PCOwner ? PCOwner->GetPawn() : nullptr;
		if (Pawn && Pawn != Actor)
		{
			// 지금 화면에서 Pawn으로 다시 블렌드
			SnapshotOutgoing(CameraCachePOV);
			StartBlend(Pawn, BlendParams);
		}
		else
		{
			// 갈 곳이 없으면 블렌드 취소
			AssignViewTarget(ViewTarget.Target);
		}
	}

	if (ViewTarget.Target != Actor)
		return;

	ViewTarget.SetNewTarget(nullptr);
	bCameraCacheValid = false;
}