#include "EnginePCH.h"
#include "CameraScriptBindings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// nil이면 즉시 전환, 숫자면 BlendTime, 테이블이면 각 필드를 읽는다
	FViewTargetTransitionParams ReadTransitionParams(const sol::object& Object)
	{
		FViewTargetTransitionParams Params;

		if (Object.get_type() == sol::type::number)
		{
			Params.BlendTime = Object.as<float>();
		}
		else if (Object.get_type() == sol::type::table)
		{
			const sol::table Table = Object.as<sol::table>();
			Params.BlendTime = Table.get_or("BlendTime", Params.BlendTime);
			Params.BlendFunction = Table.get_or("BlendFunction", Params.BlendFunction);
			Params.BlendExp = Table.get_or("BlendExp", Params.BlendExp);
			Params.bLockOutgoing = Table.get_or("bLockOutgoing", Params.bLockOutgoing);
		}

		return Params;
	}
}

void RegisterCameraBindings(sol::state& Lua)
{
	Lua.new_enum("ViewTargetBlend",
		"Linear", EViewTargetBlendFunction::Linear,
		"Cubic", EViewTargetBlendFunction::Cubic,
		"EaseIn", EViewTargetBlendFunction::EaseIn,
		"EaseOut", EViewTargetBlendFunction::EaseOut,
		"EaseInOut", EViewTargetBlendFunction::EaseInOut
	);

	Lua.new_usertype<ACameraActor>(
		"CameraActor",
		sol::no_constructor,
		sol::base_classes, sol::bases<AActor>(),
		"FieldOfView", sol::property(
			[](const ACameraActor& Self)
			{
				const UCameraComponent* Camera = Self.GetCameraComponent();
				return Camera ? Camera->GetFieldOfView() : 0.0f;
			},
			[](ACameraActor& Self, float InFieldOfView)
			{
				if (UCameraComponent* Camera = Self.GetCameraComponent())
					Camera->SetFieldOfView(InFieldOfView);
			})
	);

	// 카메라 효과(흔들림, 페이드 등)를 Lua에서 켜는 함수도 이 타입에 추가한다
	Lua.new_usertype<APlayerCameraManager>(
		"CameraManager",
		sol::no_constructor,
		sol::base_classes, sol::bases<AActor>(),
		// NewTarget이 nil이면 Pawn으로 돌아간다. TransitionParams: nil(즉시) / 숫자(BlendTime) / 테이블
		"SetViewTarget", [](APlayerCameraManager& Self, AActor* NewTarget, sol::object TransitionParams)
			{ Self.SetViewTarget(NewTarget, ReadTransitionParams(TransitionParams)); },
		// 블렌드 중이면 가는 중인 타겟
		"ViewTarget", sol::readonly_property(&APlayerCameraManager::GetViewTarget),
		"IsBlending", sol::readonly_property(&APlayerCameraManager::IsBlending)
	);

	sol::usertype<AActor> ActorType = Lua["Actor"];

	// Lua의 PlayerIndex는 1부터 (1 = P1). 그 플레이어가 없으면 nil
	ActorType["GetCameraManager"] = [](AActor& Self, int32 PlayerIndex) -> APlayerCameraManager*
	{
		UWorld* World = Self.GetWorld();
		if (!World || PlayerIndex < 1)
			return nullptr;

		APlayerController* PC = World->GetPlayerController(static_cast<uint32>(PlayerIndex - 1));
		return PC ? PC->GetCameraManager() : nullptr;
	};

	// 레벨에 고정 카메라를 스폰한다. 월드가 끝날 때 레벨과 함께 삭제된다
	ActorType["SpawnCamera"] = [](AActor& Self, const FVector& Location, const FRotator& Rotation) -> ACameraActor*
	{
		UWorld* World = Self.GetWorld();
		if (!World)
			return nullptr;

		const FTransform SpawnTransform(Rotation, Location);
		return World->SpawnActor<ACameraActor>(NAME_None, &SpawnTransform);
	};
}
