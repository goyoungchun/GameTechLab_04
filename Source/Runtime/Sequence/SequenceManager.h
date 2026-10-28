#pragma once

#include "Container/Array.h"
#include "Core/EngineString.h"
#include "Sequence/SequenceData.h"

class UWorld;
class AActor;
class ACameraActor;
class USequencePlayer;

class FSequenceManager
{
public:
	FSequenceManager() = default;
	~FSequenceManager();

	FSequenceManager(const FSequenceManager&) = delete;
	FSequenceManager& operator=(const FSequenceManager&) = delete;

	bool Play(UWorld* World, const FString& Name, float BlendTime);
	void Stop(UWorld* World, const FString& Name, float BlendTime);
	void StopAll(UWorld* World);
	bool IsPlaying(const FString& Name) const;
	void Tick(UWorld* World, double GameDeltaTime, double UnscaledDeltaTime);

private:
	struct FActiveSequence
	{
		FString Name;
		FSequence Sequence;
		USequencePlayer* Player = nullptr;
		TArray<ACameraActor*> Cameras;
		TArray<AActor*> PreviousViewTargets;
		float BlendTime = 0.0f;
		double LastTime = 0.0;
	};

	int32 FindIndex(const FString& Name) const;
	void Finish(UWorld* World, int32 Index, float BlendTime);
	static bool IsActorAlive(UWorld* World, const AActor* Actor);
	static ACameraActor* FindViewCamera(const FActiveSequence& Active);

	TArray<FActiveSequence*> ActiveSequences;
};
