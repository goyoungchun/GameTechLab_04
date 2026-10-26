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
	// 선택 UI와 차량 Lua가 네이티브의 동일한 분류/주행 설정을 참조한다.
	sol::table Catalog = Lua.create_table();
	for (EVehicleCategory Category : { EVehicleCategory::Speed, EVehicleCategory::Balanced, EVehicleCategory::Heavy })
	{
		const auto& Info = GetVehicleCategoryInfo(Category);
		const auto Tuning = MakeVehicleCategoryTuning(Category);
		sol::table Data = Lua.create_table_with("Id", Info.Id, "Label", Info.Label, "Mass", Info.Mass,
			"SpeedWeight", Info.SpeedWeight, "Tuning", Lua.create_table_with(
				"MaxForwardSpeed", Tuning.MaxForwardSpeed, "MaxReverseSpeed", Tuning.MaxReverseSpeed,
				"BoostMaxSpeed", Tuning.BoostMaxSpeed, "Acceleration", Tuning.Acceleration));
		Catalog[Info.Id] = Data;
	}
	Catalog["Default"] = Catalog["Balanced"];
	for (const auto& Model : GetVehicleModelCatalog()) Catalog[Model.Name] = Catalog[GetVehicleCategoryInfo(Model.Category).Id];
	Lua["VehicleCatalog"] = Catalog;

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
			{ if (auto* M = Self.GetMovement()) M->SetTuning(ReadTuning(Table)); },
		"ResetTo", [](AVehiclePawn& Self, const FVector& Location, float Yaw)
			{
				Self.SetActorLocation(Location);
				Self.SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
				if (auto* M = Self.GetMovement()) M->ResetMotion();
			},
		"BoostRatio", sol::readonly_property([](const AVehiclePawn& Self)
			{ const auto* M = Self.GetMovement(); return M ? M->GetBoostRatio() : 0.0f; }),
		"SetBoostCapacity", [](AVehiclePawn& Self, float Seconds)
			{ if (auto* M = Self.GetMovement()) M->SetBoostCapacity(Seconds); },
		"SetBoostRechargeTime", [](AVehiclePawn& Self, float Seconds)
			{ if (auto* M = Self.GetMovement()) M->SetBoostRechargeTime(Seconds); },
		"RefillBoost", [](AVehiclePawn& Self)
			{ if (auto* M = Self.GetMovement()) M->RefillBoost(); },
		"SetInfiniteBoost", [](AVehiclePawn& Self, bool bInfinite)
			{ if (auto* M = Self.GetMovement()) M->SetInfiniteBoost(bInfinite); }
	);

	sol::usertype<AActor> ActorType = Lua["Actor"];
	ActorType["AsVehicle"] = [](AActor& Self) -> AVehiclePawn* { return Cast<AVehiclePawn>(&Self); };
}
