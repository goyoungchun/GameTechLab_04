#include "EnginePCH.h"
#include "VehicleTypes.h"

namespace
{
	constexpr FVehicleModelEntry Models[] = {
		{ "armor", EVehicleCategory::Heavy },
		{ "coupe", EVehicleCategory::Balanced },
		{ "fenyr", EVehicleCategory::Speed },
		{ "ghini", EVehicleCategory::Speed },
		{ "italia", EVehicleCategory::Speed },
		{ "jeep", EVehicleCategory::Heavy },
		{ "kamaro", EVehicleCategory::Speed },
		{ "lamb", EVehicleCategory::Speed },
		{ "mobil", EVehicleCategory::Balanced },
		{ "police", EVehicleCategory::Balanced },
		{ "rally", EVehicleCategory::Balanced },
		{ "van", EVehicleCategory::Heavy },
	};
	constexpr FVehicleCategoryInfo Speed = { "Speed", "스피드형", 1.0f, 3.5f };
	constexpr FVehicleCategoryInfo Balanced = { "Balanced", "밸런스형", 2.0f, 2.5f };
	constexpr FVehicleCategoryInfo Heavy = { "Heavy", "중량형", 3.5f, 1.5f };
}

std::span<const FVehicleModelEntry> GetVehicleModelCatalog() { return Models; }

EVehicleCategory GetVehicleCategory(const FString& ModelName)
{
	for (const auto& Model : Models)
		if (ModelName == Model.Name) return Model.Category;
	return EVehicleCategory::Balanced;
}

const FVehicleCategoryInfo& GetVehicleCategoryInfo(EVehicleCategory Category)
{
	switch (Category)
	{
	case EVehicleCategory::Speed: return Speed;
	case EVehicleCategory::Heavy: return Heavy;
	default: return Balanced;
	}
}

FVehicleTuning MakeVehicleCategoryTuning(EVehicleCategory Category)
{
	FVehicleTuning Tuning;
	// 밸런스형의 현재 속도대(30/45)와 가속도(22.5)를 기준으로 비율만 적용한다.
	const float Scale = GetVehicleCategoryInfo(Category).SpeedWeight / Balanced.SpeedWeight;
	Tuning.MaxForwardSpeed *= Scale;
	Tuning.MaxReverseSpeed *= Scale;
	Tuning.BoostMaxSpeed *= Scale;
	Tuning.Acceleration *= Scale;
	return Tuning;
}
