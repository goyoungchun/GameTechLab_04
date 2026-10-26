#include "EnginePCH.h"
#include "VehicleScriptBindings.h"
#include "VehiclePawn.h"
#include "VehicleTypes.h"
#include "VehicleMovementComponent.h"

#include <algorithm>

namespace
{
	FVehicleTuning ReadTuning(const sol::table& Table)
	{
		struct FField
		{
			const char* Name;
			float FVehicleTuning::* Member;
		};

		static const FField Fields[] = {
			{ "MaxForwardSpeed", &FVehicleTuning::MaxForwardSpeed },
			{ "MaxReverseSpeed", &FVehicleTuning::MaxReverseSpeed },
			{ "BoostMaxSpeed",   &FVehicleTuning::BoostMaxSpeed },
			{ "Acceleration",    &FVehicleTuning::Acceleration },
			{ "Deceleration",    &FVehicleTuning::Deceleration },
			{ "Grip",            &FVehicleTuning::Grip },
			{ "TurnRate",        &FVehicleTuning::TurnRate },
			{ "FullTurnSpeed",   &FVehicleTuning::FullTurnSpeed },
			{ "JumpSpeed",       &FVehicleTuning::JumpSpeed },
		};

		FVehicleTuning Tuning;
		for (const FField& Field : Fields)
		{
			const sol::object Value = Table[Field.Name];
			if (Value.is<float>())
				Tuning.*Field.Member = Value.as<float>();
			else if (Value.get_type() != sol::type::lua_nil)
				HTR_LOG(Warning, "[Vehicle] Tuning '{}' is not a number", Field.Name);
		}

		for (const auto& Pair : Table)
		{
			if (Pair.first.get_type() != sol::type::string)
				continue;

			const std::string Name = Pair.first.as<std::string>();
			const bool bKnown = std::any_of(std::begin(Fields), std::end(Fields),
				[&Name](const FField& Field) { return Name == Field.Name; });
			if (!bKnown)
				HTR_LOG(Warning, "[Vehicle] Unknown tuning key '{}'", Name);
		}

		return Tuning;
	}
}

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
			{ const auto* M = Self.GetMovement(); return M ? M->GetLastLandingSpeed() : 0.0f; }),
		"BodyName", sol::readonly_property(&AVehiclePawn::GetBodyName),
		"SetBody", &AVehiclePawn::SetBody,
		"SetTuning", [](AVehiclePawn& Self, sol::table Table)
			{ if (auto* M = Self.GetMovement()) M->SetTuning(ReadTuning(Table)); }
	);

	sol::usertype<AActor> ActorType = Lua["Actor"];
	ActorType["AsVehicle"] = [](AActor& Self) -> AVehiclePawn* { return Cast<AVehiclePawn>(&Self); };
}