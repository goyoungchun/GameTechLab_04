#pragma once

#include "Container/Array.h"
#include "Container/Map.h"
#include "Core/EngineString.h"
#include "Sequence/SequenceData.h"

class UWorld;
class AActor;
class ACameraActor;
class USequencePlayer;
class USceneComponent;

class FSequenceManager
{
public:
	FSequenceManager() = default;
	~FSequenceManager();

	FSequenceManager(const FSequenceManager&) = delete;
	FSequenceManager& operator=(const FSequenceManager&) = delete;

	// Target이 있으면 미지정 트랙을 연결하고, 같은 파일도 대상별로 독립 재생한다.
	bool Play(UWorld* World, const FString& Name, float BlendTime, USceneComponent* Target = nullptr, int32 ViewPlayerIndex = -1,
		const TMap<FString, AActor*>* ActorOverrides = nullptr);
	void Stop(UWorld* World, const FString& Name, float BlendTime, USceneComponent* Target = nullptr);
	void StopAll(UWorld* World);
	bool IsPlaying(const FString& Name, const USceneComponent* Target = nullptr) const;
	double GetPlaybackTime(const FString& Name) const;
	void Tick(UWorld* World, double GameDeltaTime, double UnscaledDeltaTime);

private:
	struct FActiveSequence
	{
		FString Name;
		USceneComponent* Target = nullptr;
		uint32 TargetId = 0;
		FSequence Sequence;
		USequencePlayer* Player = nullptr;
		TArray<ACameraActor*> Cameras;
		TArray<AActor*> PreviousViewTargets;
		float BlendTime = 0.0f;
		int32 ViewPlayerIndex = -1;
		double LastTime = 0.0;
	};

	int32 FindIndex(const FString& Name, const USceneComponent* Target = nullptr) const;
	void Finish(UWorld* World, int32 Index, float BlendTime);
	static bool IsActorAlive(UWorld* World, const AActor* Actor);
	static bool IsTargetAlive(UWorld* World, const FActiveSequence& Active);
	static ACameraActor* FindViewCamera(const FActiveSequence& Active);

	TArray<FActiveSequence*> ActiveSequences;
	// 카메라를 생성하지 않는 대상 지정 Sequence는 첫 로드 결과를 재사용한다.
	TMap<FString, FSequence> ComponentSequenceTemplates;
};
