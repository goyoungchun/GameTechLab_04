#pragma once

#include "Container/Array.h"
#include "Core/EngineString.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

class AActor;

enum class EDummyTrackType : uint8
{
	Transform,
	Float,
	CameraCut,
	Event,
};

struct FDummyKey
{
	float Time = 0.0f;
	float Ease[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

	FVector Location;
	FRotator Rotation;
	float FloatValue = 0.0f;
};

struct FDummyTrack
{
	FString Name;
	EDummyTrackType Type = EDummyTrackType::Transform;
	TArray<FDummyKey> Keys;
	AActor* BoundActor = nullptr;
};

struct FDummySequence
{
	float Length = 5.0f;
	float FrameRate = 30.0f;
	TArray<FDummyTrack> Tracks;
};
