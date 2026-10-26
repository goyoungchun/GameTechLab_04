#include "EnginePCH.h"
#include "VehicleScriptBindings.h"
#include "VehiclePawn.h"
#include "VehicleTypes.h"
#include "VehicleMovementComponent.h"

void RegisterVehicleBindings(sol::state& Lua)
{
	Lua.new_usertype<AVehiclePawn>(
		"Vehicle",
		sol::no_constructor,
		sol::base_classes, sol::bases<APawn, AActor>(),
		"PlayerIndex", sol::readonly_property(&AVehiclePawn::GetPlayerIndex),
		"SetInput", [](AVehiclePawn& Self, float Throttle, float Steer, bool bBoost) 
			{ Self.SetVehicleInput(FVehicleInput{ Throttle, Steer, bBoost, false }); },
		"Jump", &AVehiclePawn::RequestJump,
		"Velocity", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M ? M->GetVelocity() : FVector::ZeroVector; }),
		"Speed", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M ? M->GetSpeed() : 0.0f; }),
		"ForwardSpeed", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M ? M->GetForwardSpeed() : 0.0f; }),
		"IsGrounded", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M && M->IsGrounded(); }),
		"TimeSinceGrounded", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M ? M->GetTimeSinceGrounded() : 0.0f; }),
		"JustLanded", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M && M->HasLandedThisFrame(); }),
		"LastLandingSpeed", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M ? M->GetLastLandingSpeed() : 0.0f; })
	);

	sol::usertype<AActor> ActorType = Lua["Actor"];
	ActorType["AsVehicle"] = [](AActor& Self) -> AVehiclePawn* { return Cast<AVehiclePawn>(&Self); };
}