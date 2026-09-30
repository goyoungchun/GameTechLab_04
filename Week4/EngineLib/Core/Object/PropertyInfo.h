// EngineLib/Core/Object/PropertyInfo.h

#pragma once

#include <functional>
#include <type_traits>

#include "Core/enum.h"
#include "Core/PropertyEnum.h"
#include "Engine/Serialization/PropertyJson.h"

namespace json
{
	class JSON;
}

class UObject;


struct FPropertyInfo
{
	using SerializeFunc = void (*)(const FPropertyInfo& Property, const UObject* Object, json::JSON& OutProperties);
	using DeserializeFunc = void (*)(const FPropertyInfo& Property, UObject* Object, const json::JSON& InProperties);
	using GetValueFunc = FPropertyValue(*)(const UObject* Object);
	using SetValueFunc = void (*)(const FPropertyInfo& Property, UObject* Object, const FPropertyValue& Value);

	const char* JsonKey = nullptr;

	//EPropertyType PropertyType = EPropertyType::None;
	EPropertyFlags PropertyFlags = EPropertyFlags::Serializable;

	GetValueFunc GetValue = nullptr;
	SetValueFunc SetValue = nullptr;

	/* (De)Serialize function */
	SerializeFunc Serialize = nullptr;
	DeserializeFunc Deserialize = nullptr;
};

#define REFLECT_PROPERTY(OwnerType, MemberName, ...)					\
    MakeProperty<														\
        OwnerType,														\
        decltype(OwnerType::MemberName),								\
        &OwnerType::MemberName											\
	>(#MemberName, ##__VA_ARGS__)

#define REFLECT_PROPERTY_SETTER(OwnerType, MemberName, SetterFunc, ...)		\
	MakeProperty<															\
		OwnerType,															\
		decltype(OwnerType::MemberName),									\
		&OwnerType::MemberName,												\
		SetterFunc															\
>(#MemberName, ##__VA_ARGS__)

template<
	typename TOwner,
	typename TValue,
	TValue TOwner::* Member,
	auto Setter = nullptr>
FPropertyInfo MakeProperty(const char* JsonKey, EPropertyFlags PropertyFlags = EPropertyFlags::Serializable)
{
	FPropertyInfo Property;

	Property.JsonKey = JsonKey;
	Property.PropertyFlags = PropertyFlags;

	// Skip GetValue if TValue is not constructible from const TValue&
	if constexpr (
		std::is_constructible_v<FPropertyValue,
		std::in_place_type_t<TValue>, const TValue&>)
	{
		Property.GetValue =
			[](const UObject* Object) -> FPropertyValue
			{
				const TOwner* Owner = static_cast<const TOwner*>(Object);

				return FPropertyValue{
					std::in_place_type<TValue>,
					Owner->*Member
				};
			};

		Property.SetValue =
			[](const FPropertyInfo& Property, UObject* Object, const FPropertyValue& value) -> void
			{
				if ((Property.PropertyFlags & EPropertyFlags::Editable) == EPropertyFlags::None)
				{
					throw std::runtime_error("Property is not editable");
				}

				const TValue* typedValue = std::get_if<TValue>(&value);

				if (!typedValue)
				{
					throw std::runtime_error("Property value type mismatch");
				}

				TOwner* Owner = static_cast<TOwner*>(Object);

				if constexpr (std::is_same_v<decltype(Setter), std::nullptr_t>)
				{
					Owner->*Member = std::get<TValue>(value);
				}
				else
				{
					static_assert(std::is_invocable_r_v<
						void,
						decltype(Setter),
						TOwner*,
						const TValue&>,
						"Setter must be invocable with (TOwner*, const TValue&)");

					std::invoke(Setter, Owner, *typedValue);
				}
			};
	}

	//Property.PropertyType = GetPropertyType<TValue>();

	Property.Serialize =
		[](const FPropertyInfo& Property,
			const UObject* Object,
			json::JSON& OutJson)
		{
			const TOwner* Owner = static_cast<const TOwner*>(Object);

			const TValue& Value = Owner->*Member;

			TPropertyJsonSerializer<TValue>::Serialize(
				OutJson,
				Property.JsonKey,
				Value);
		};

	Property.Deserialize =
		[](const FPropertyInfo& Property,
			UObject* Object,
			const json::JSON& InJson)
		{
			TOwner* Owner = static_cast<TOwner*>(Object);

			TValue& Value = Owner->*Member;

			TPropertyJsonSerializer<TValue>::Deserialize(
				InJson,
				Property.JsonKey,
				Value);
		};

	return Property;
}
