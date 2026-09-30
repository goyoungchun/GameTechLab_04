// EngineLib/Core/PropertyEnum.h

#pragma once

#include <variant>

#include "Core/Core.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Color.h"
#include "Core/Name.h"

//enum class EPropertyType
//{
//	None,
//	Bool,
//	Int32,
//	UInt32,
//	Float,
//	String,
//	Name,
//	Vector2,
//	Vector3,
//	Vector4,
//	Rotator,
//	LinearColor,
//	Array,
//};
//
//template<typename T>
//constexpr EPropertyType GetPropertyType()
//{
//	if constexpr (std::is_same_v<T, bool>)
//		return EPropertyType::Bool;
//	else if constexpr (std::is_same_v<T, int32>)
//		return EPropertyType::Int32;
//	else if constexpr (std::is_same_v<T, uint32>)
//		return EPropertyType::UInt32;
//	else if constexpr (std::is_same_v<T, float>)
//		return EPropertyType::Float;
//	else if constexpr (std::is_same_v<T, FString>)
//		return EPropertyType::String;
//	else if constexpr (std::is_same_v<T, FName>)
//		return EPropertyType::Name;
//	else if constexpr (std::is_same_v<T, FVector2>)
//		return EPropertyType::Vector2;
//	else if constexpr (std::is_same_v<T, FVector3>)
//		return EPropertyType::Vector3;
//	else if constexpr (std::is_same_v<T, FVector4>)
//		return EPropertyType::Vector4;
//	else if constexpr (std::is_same_v<T, FRotator>)
//		return EPropertyType::Rotator;
//	else if constexpr (std::is_same_v<T, FLinearColor>)
//		return EPropertyType::LinearColor;
//	else
//		return EPropertyType::None;
//}

using FPropertyValue = std::variant<
	bool,
	int32,
	uint32,
	float,
	FString,
	FName,
	FVector2,
	FVector3,
	FVector4,
	FRotator,
	FLinearColor
>;

enum class EPropertyFlags : uint32
{
	None = 0,
	Serializable = 1 << 0,
	Editable = 1 << 1,
};

constexpr EPropertyFlags operator|(EPropertyFlags lhs, EPropertyFlags rhs)
{
	return static_cast<EPropertyFlags>(static_cast<uint32>(lhs) | static_cast<uint32>(rhs));
}

constexpr EPropertyFlags operator&(EPropertyFlags lhs, EPropertyFlags rhs)
{
	return static_cast<EPropertyFlags>(static_cast<uint32>(lhs) & static_cast<uint32>(rhs));
}
