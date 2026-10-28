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

	// Lua 플레이어 번호는 1부터 시작한다. 지정한 화면과 카메라 효과만 전환한다.
	ActorType["PlaySequenceForPlayer"] = [](AActor& Self, const FString& Name, int32 PlayerIndex, sol::optional<float> BlendTime) -> bool
	{
		UWorld* World = Self.GetWorld();
		return World && PlayerIndex >= 1 && World->GetSequenceManager().Play(World, Name, BlendTime.value_or(0.0f), nullptr, PlayerIndex - 1);
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
