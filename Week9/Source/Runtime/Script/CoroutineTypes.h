#pragma once

#include <sol/sol.hpp>
#include "Core/Types.h"

enum class EWaitType : uint8
{
	None,
	Seconds,
	Until,
	While
};

enum class ECoroutineState : uint8
{
	Running,
	Waiting,
	Finished,
	Stopped,
	Errored
};

struct FCoroutineRecord
{
	uint32 Id = 0;
	sol::thread Thread;
	sol::coroutine Coroutine;
	EWaitType WaitType = EWaitType::None;
	float Remaining = 0.0f;
	sol::protected_function Predicate;
	ECoroutineState State = ECoroutineState::Waiting;
	int32 LastFrame = -1;

	bool IsActive()
	{
		return State == ECoroutineState::Running || State == ECoroutineState::Waiting;
	}
};