#include "EnginePCH.h"
#include "SequenceScriptBindings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void RegisterSequenceBindings(sol::state& Lua)
{
	sol::usertype<AActor> ActorType = Lua["Actor"];

	ActorType["PlaySequence"] = [](AActor& Self, const FString& Name, sol::optional<float> BlendTime) -> bool
	{
		UWorld* World = Self.GetWorld();
		return World && World->GetSequenceManager().Play(World, Name, BlendTime.value_or(0.0f));
	};

	ActorType["StopSequence"] = [](AActor& Self, const FString& Name, sol::optional<float> BlendTime)
	{
		if (UWorld* World = Self.GetWorld())
			World->GetSequenceManager().Stop(World, Name, BlendTime.value_or(0.0f));
	};

	ActorType["IsSequencePlaying"] = [](AActor& Self, const FString& Name) -> bool
	{
		UWorld* World = Self.GetWorld();
		return World && World->GetSequenceManager().IsPlaying(Name);
	};
}
