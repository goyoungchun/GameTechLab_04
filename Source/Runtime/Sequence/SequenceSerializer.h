#pragma once

#include "Container/Array.h"
#include "Core/EngineString.h"
#include "Sequence/SequenceData.h"

class UWorld;
class ACameraActor;

class FSequenceSerializer
{
public:
	static bool Save(const FSequence& Sequence, const FString& Path, const FString& ScenePath = FString());
	static bool Load(const FString& Path, UWorld* World, FSequence& OutSequence, TArray<ACameraActor*>& OutCameras, FString* OutScenePath = nullptr);

	static ACameraActor* SpawnCamera(UWorld* World, const FTransform& Transform);
	static bool IsSequenceCamera(const ACameraActor* Camera);
	static FString GetSequencePath(const FString& Name);

	static constexpr const char* SequenceDirectory = "Assets/Sequences";
	static constexpr const char* SequenceExtension = ".sequence";
	static constexpr const char* CameraIconPath = "Editor/Icon/CineCameraActor_64x.png";
};
