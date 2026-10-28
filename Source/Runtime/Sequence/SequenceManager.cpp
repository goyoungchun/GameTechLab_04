#include "EnginePCH.h"
#include "Sequence/SequenceManager.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Sequence/SequencePlayer.h"
#include "Sequence/SequenceSerializer.h"

#include <algorithm>

FSequenceManager::~FSequenceManager()
{
	for (FActiveSequence* Active : ActiveSequences)
	{
		delete Active->Player;
		delete Active;
	}
	ActiveSequences.Reset();
}

bool FSequenceManager::Play(UWorld* World, const FString& Name, float BlendTime)
{
	if (!World)
		return false;

	const int32 ExistingIndex = FindIndex(Name);
	if (ExistingIndex >= 0)
	{
		FActiveSequence* Existing = ActiveSequences[ExistingIndex];
		Existing->Player->Stop();
		Existing->Player->Play(true);
		Existing->LastTime = 0.0;
		return true;
	}

	FActiveSequence* Active = new FActiveSequence();
	if (!FSequenceSerializer::Load(FSequenceSerializer::GetSequencePath(Name), World, Active->Sequence, Active->Cameras))
	{
		delete Active;
		return false;
	}

	Active->Name = Name;
	Active->BlendTime = BlendTime;
	Active->Player = FObjectFactory::ConstructObject<USequencePlayer>();
	Active->Player->SetSequence(&Active->Sequence);
	Active->Player->SetPlaybackWorld(World);
	Active->Player->Seek(0.0, false);
	Active->Player->Play(true);

	if (ACameraActor* ViewCamera = FindViewCamera(*Active))
	{
		FViewTargetTransitionParams Params;
		Params.BlendTime = BlendTime;
		for (uint32 Index = 0; Index < World->GetPlayerCount(); Index++)
		{
			APlayerController* PC = World->GetPlayerController(Index);
			APlayerCameraManager* CameraManager = PC ? PC->GetCameraManager() : nullptr;
			Active->PreviousViewTargets.Add(CameraManager ? CameraManager->GetViewTarget() : nullptr);
			if (CameraManager)
				CameraManager->SetViewTarget(ViewCamera, Params);
		}
	}

	ActiveSequences.Add(Active);
	return true;
}

void FSequenceManager::Stop(UWorld* World, const FString& Name, float BlendTime)
{
	const int32 Index = FindIndex(Name);
	if (Index >= 0)
		Finish(World, Index, BlendTime);
}

bool FSequenceManager::IsPlaying(const FString& Name) const
{
	return FindIndex(Name) >= 0;
}

void FSequenceManager::StopAll(UWorld* World)
{
	while (!ActiveSequences.IsEmpty())
		Finish(World, ActiveSequences.Num() - 1, 0.0f);
}

void FSequenceManager::Tick(UWorld* World, double GameDeltaTime, double UnscaledDeltaTime)
{
	for (int32 Index = ActiveSequences.Num() - 1; Index >= 0; Index--)
	{
		FActiveSequence* Active = ActiveSequences[Index];

		const bool bCamerasAlive = std::all_of(Active->Cameras.begin(), Active->Cameras.end(),
			[World](const ACameraActor* Camera) { return IsActorAlive(World, Camera); });
		if (!bCamerasAlive || Active->Sequence.Length <= 0.0)
		{
			Finish(World, Index, Active->BlendTime);
			continue;
		}

		const double PreviousTime = Active->LastTime;
		Active->Player->Update(GameDeltaTime, UnscaledDeltaTime);
		const double CurrentTime = Active->Player->GetCurrentTime();

		const bool bAdvanced = GameDeltaTime * Active->Player->GetPlayRate() > 0.0;
		const bool bWrapped = CurrentTime < PreviousTime || (CurrentTime <= 0.0 && bAdvanced);
		if (bWrapped || Active->Player->GetPlaybackState() == ESequencePlaybackState::Stopped)
		{
			Active->Player->Seek(Active->Sequence.Length, false);
			Finish(World, Index, Active->BlendTime);
			continue;
		}

		Active->LastTime = CurrentTime;
	}
}

int32 FSequenceManager::FindIndex(const FString& Name) const
{
	for (int32 Index = 0; Index < ActiveSequences.Num(); Index++)
	{
		if (ActiveSequences[Index]->Name == Name)
			return Index;
	}
	return -1;
}

void FSequenceManager::Finish(UWorld* World, int32 Index, float BlendTime)
{
	FActiveSequence* Active = ActiveSequences[Index];

	if (World)
	{
		FViewTargetTransitionParams Params;
		Params.BlendTime = BlendTime;
		for (int32 PlayerIndex = 0; PlayerIndex < Active->PreviousViewTargets.Num(); PlayerIndex++)
		{
			APlayerController* PC = World->GetPlayerController(static_cast<uint32>(PlayerIndex));
			APlayerCameraManager* CameraManager = PC ? PC->GetCameraManager() : nullptr;
			if (!CameraManager)
				continue;

			AActor* CurrentTarget = CameraManager->GetViewTarget();
			const bool bViewingSequence = std::find(Active->Cameras.begin(), Active->Cameras.end(), CurrentTarget) != Active->Cameras.end();
			if (!bViewingSequence)
				continue;

			AActor* Previous = Active->PreviousViewTargets[PlayerIndex];
			CameraManager->SetViewTarget(IsActorAlive(World, Previous) ? Previous : nullptr, Params);
		}

		for (ACameraActor* Camera : Active->Cameras)
		{
			if (IsActorAlive(World, Camera))
				Camera->Destroy();
		}
	}

	delete Active->Player;
	delete Active;
	ActiveSequences.RemoveAt(static_cast<uint32>(Index), 1);
}

bool FSequenceManager::IsActorAlive(UWorld* World, const AActor* Actor)
{
	if (!World || !Actor || !World->GetPersistentLevel())
		return false;

	const TArray<AActor*>& Actors = World->GetPersistentLevel()->GetActors();
	auto Found = std::find(Actors.begin(), Actors.end(), Actor);
	return Found != Actors.end() && !(*Found)->IsActorBeingDestroyed();
}

ACameraActor* FSequenceManager::FindViewCamera(const FActiveSequence& Active)
{
	for (const FSequenceTrack& Track : Active.Sequence.Tracks)
	{
		const FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track);
		if (!TransformTrack || !TransformTrack->Binding)
			continue;

		for (ACameraActor* Camera : Active.Cameras)
		{
			if (Camera && Camera->GetCameraComponent() == TransformTrack->Binding)
				return Camera;
		}
	}

	for (ACameraActor* Camera : Active.Cameras)
	{
		if (Camera)
			return Camera;
	}
	return nullptr;
}
