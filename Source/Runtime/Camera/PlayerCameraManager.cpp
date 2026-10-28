#include "EnginePCH.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "PlayerCameraManager.h"
#include "CameraModifier_CameraShake.h"
#include "CameraModifier_CameraFade.h"
#include "CameraModifier_Letterbox.h"
#include "CameraModifier_Vignette.h"

APlayerCameraManager::APlayerCameraManager()
{
}

APlayerCameraManager::~APlayerCameraManager()
{
	ClearCameraModifiers();
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
		SnapshotOutgoing(BaseCachePOV);

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

	FMinimalViewInfo NewPOV = ViewTarget.POV;

	if (IsBlending())
	{
		UpdateViewTarget(PendingViewTarget, DeltaTime);
		BlendTimeToGo -= DeltaTime;

		if (BlendTimeToGo > 0.0f)
		{
			const float TimePct = 1.0f - BlendTimeToGo / BlendParams.BlendTime;
			const float BlendAlpha = BlendParams.GetBlendAlpha(TimePct);

			NewPOV.BlendViewInfo(PendingViewTarget.POV, BlendAlpha);
		}
		else
		{
			// 블렌딩 종료
			ViewTarget.POV = PendingViewTarget.POV;
			AssignViewTarget(PendingViewTarget.Target);
			NewPOV = ViewTarget.POV;
		}
	}

	BaseCachePOV = NewPOV;
	ApplyCameraModifiers(DeltaTime, NewPOV);
	FillCameraCache(NewPOV);
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
		// 블렌드 중이면 남은 POV가 스냅샷이 됨
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
			SnapshotOutgoing(BaseCachePOV);
			StartBlend(Pawn, BlendParams);
		}
		else
		{
			// 갈 곳이 없으면 블렌드 취소
			AssignViewTarget(ViewTarget.Target);
		}
	}
}

UCameraModifier* APlayerCameraManager::AddNewCameraModifier(UClass* ModifierClass)
{
	if (!ModifierClass || !ModifierClass->IsChildOf(UCameraModifier::StaticClass()))
		return nullptr;

	UCameraModifier* NewModifier = Cast<UCameraModifier>(FObjectFactory::ConstructObject(ModifierClass, this));

	if (!NewModifier)
		return nullptr;

	// Priority가 작은 것이 앞. 같을 시 먼저 추가된 것 앞.
	uint32 InsertIndex = 0;
	while (InsertIndex < static_cast<uint32>(ModifierList.Num())
		&& ModifierList[InsertIndex]->GetPriority() <= NewModifier->GetPriority())
		++InsertIndex;

	ModifierList.Insert(NewModifier, InsertIndex);
	NewModifier->AddedToCamera(this);
	return NewModifier;
}

bool APlayerCameraManager::RemoveCameraModifier(UCameraModifier* Modifier)
{
	const int32 Index = ModifierList.Find(Modifier);
	if (Index >= ModifierList.Num())
		return false;

	ModifierList.RemoveAt(Index, 1);
	delete Modifier;
	return true;
}

void APlayerCameraManager::ClearCameraModifiers()
{
	TArray<UCameraModifier*> ToDelete = ModifierList;
	ModifierList.Reset();

	for (UCameraModifier* Modifier : ToDelete)
		delete Modifier;
}

void APlayerCameraManager::RemoveOwnedModifier(UCameraModifier* Modifier)
{
	const int32 Index = ModifierList.Find(Modifier);
	if (Index >= 0 && Index < ModifierList.Num())
		ModifierList.RemoveAt(Index, 1);
}

void APlayerCameraManager::StartCameraShake(const FCameraShakeParams& Params)
{
	UCameraModifier_CameraShake* ShakeModifier = FindCameraModifier<UCameraModifier_CameraShake>();
	if (!ShakeModifier)
	{
		ShakeModifier = AddCameraModifier<UCameraModifier_CameraShake>();
	}

	if (ShakeModifier)
	{
		ShakeModifier->AddCameraShake(Params);
	}
}

void APlayerCameraManager::StopAllCameraShakes()
{
	if (UCameraModifier_CameraShake* ShakeModifier = FindCameraModifier <UCameraModifier_CameraShake>())
	{
		ShakeModifier->StopAllCameraShakes();
	}
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FVector& Color, bool bHoldWhenFinished, const Bezier& Ease)
{
	UCameraModifier_CameraFade* FadeModifier = FindCameraModifier<UCameraModifier_CameraFade>();
	if (!FadeModifier)
	{
		FadeModifier = AddCameraModifier<UCameraModifier_CameraFade>();
	}

	if (FadeModifier)
	{
		FadeModifier->StartFade(FromAlpha, ToAlpha, Duration, Color, bHoldWhenFinished, Ease);
	}
}

void APlayerCameraManager::StopCameraFade()
{
	if (UCameraModifier_CameraFade* FadeModifier = FindCameraModifier<UCameraModifier_CameraFade>())
	{
		FadeModifier->StopFade();
	}
}

void APlayerCameraManager::SetLetterbox(bool bEnable, float Ratio)
{
	UCameraModifier_CameraLetterbox* Letterbox = FindCameraModifier< UCameraModifier_CameraLetterbox>();

	if (bEnable)
	{
		if (!Letterbox)
		{
			Letterbox = AddCameraModifier<UCameraModifier_CameraLetterbox>();
		}
		else
		{
			Letterbox->EnableModifier();
		}

		Letterbox->TargetRatio = FMath::Clamp(Ratio, 0.0f, 0.5f);
	}
	else if (Letterbox)
	{
		Letterbox->DisableModifier();
	}
}

void APlayerCameraManager::SetVignette(bool bEnable, float Intensity, float Radius, float Softness)
{
	UCameraModifier_CameraVignette* Vignette = FindCameraModifier<UCameraModifier_CameraVignette>();

	if (bEnable)
	{
		if (!Vignette)
		{
			Vignette = AddCameraModifier<UCameraModifier_CameraVignette>();
		}
		else
		{
			Vignette->EnableModifier();
		}

		if (Vignette)
		{
			Vignette->Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
			Vignette->Radius = FMath::Clamp(Radius, 0.0f, 1.0f);
			Vignette->Softness = std::max(Softness, 0.001f);
		}
	}
	else if (Vignette)
	{
		Vignette->DisableModifier();
	}
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier->IsDisabled())
			continue;

		Modifier->UpdateAlpha(DeltaTime);
		Modifier->ModifyCamera(DeltaTime, InOutPOV);
	}

	// 끝난 일회성 Modifier들은 순회가 끝난 뒤 삭제
	for (int32 Index = ModifierList.Num() - 1; Index >= 0; --Index)
	{
		if (ModifierList[Index]->IsFinished())
		{
			UCameraModifier* Finished = ModifierList[Index];
			ModifierList.RemoveAt(Index, 1);
			delete Finished;
		}
	}
}
