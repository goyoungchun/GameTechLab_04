#include "EnginePCH.h"
#include "VehicleScriptBindings.h"
#include "VehiclePawn.h"
#include "VehicleTypes.h"

void RegisterVehicleBindings(sol::state& Lua)
{
	Lua.new_usertype<AVehiclePawn>(
		"Vehicle",
		sol::no_constructor,
		sol::base_classes, sol::bases<APawn, AActor>(),
		"PlayerIndex", sol::readonly_property(&AVehiclePawn::GetPlayerIndex),
		"SetInput", [](AVehiclePawn& Self, float Throttle, float Steer, bool bBoost) { Self.SetVehicleInput(FVehicleInput{ Throttle, Steer, bBoost, false }); },
		"Jump", &AVehiclePawn::RequestJump
	);

	sol::usertype<AActor> ActorType = Lua["Actor"];
	ActorType["AsVehicle"] = [](AActor& Self) -> AVehiclePawn* { return Cast<AVehiclePawn>(&Self); };
}