#pragma once

#include <functional>
#include <span>
#include "PropertyInfo.h"

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "Core/Container/TSparseArray.h"
#include "ObjectFactory.h"
#include "Core/Name.h"


namespace json { class JSON; }

class UObject;

//using ConstructorFunc = UObject * (*)();

struct FClassInfo
{
	FString Name;
	const FClassInfo* SuperClass;
	std::function<UObject* ()> Constructor;

	std::span<const FPropertyInfo> DeclaredProperties = {};

	FClassInfo(FString name, const FClassInfo* superClass,
		std::function<UObject* ()> constructor, std::span<const FPropertyInfo> declaredProperties = {})
		: Name(std::move(name)), SuperClass(superClass), Constructor(std::move(constructor)),
		DeclaredProperties(declaredProperties) {
	}

	template<typename TVisitor>
	void ForEachProperty(TVisitor&& Visitor) const
	{
		if (SuperClass)
		{
			SuperClass->ForEachProperty(Visitor);
		}

		for (const FPropertyInfo& property : DeclaredProperties)
		{
			Visitor(property);
		}
	}

	const FPropertyInfo* FindProperty(std::string_view Key) const
	{
		// Find property in the current class's declared properties
		for (const FPropertyInfo& property : DeclaredProperties)
		{
			if (property.JsonKey && Key == std::string_view(property.JsonKey))
			{
				return &property;
			}
		}

		// If not found, check the superclass
		return SuperClass ? SuperClass->FindProperty(Key) : nullptr;
	}



	UObject* CreateInstance() const;

private:
};

struct FObjectID
{
	int32 UUID;
	uint32 InternalIndex;
};

class UObject
{
public:
	// Todo: Fix
	int32 UUID;
	uint32 InternalIndex;

	const FName& GetName() const
	{
		return mName;
	}

	virtual void SetName(const FName& name)
	{
		mName = name;
	}

	inline FObjectID GetObjectID() const
	{
		return { UUID, InternalIndex };
	}

	virtual ~UObject();
	virtual void Destroy();

	void Initialize();

	// StaticClass() in Unreal Engine
	static FClassInfo ClassInfo;
	static const FClassInfo* GetClass();

	// GetClass() in Unreal Engine
	inline const FClassInfo* GetRuntimeClass() const { return mClassInfo; }

	// TODO?: Replace json type with a more generic type, such as a variant or a map
	virtual void SerializeClass(json::JSON& outJson) const;
	virtual void DeserializeClass(const json::JSON& inJson);

	virtual void PostDeserialize() {}

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	bool IsA() const;

	bool IsA(const FClassInfo* classInfo) const;

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	TObject* Cast();

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	const TObject* Cast() const;
	
	static UObject* GetObjectByUUID(int32 uuid);
	static UObject* GetObjectByInternalIndex(uint32 internalIndex);

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* GetObjectByUUID(int32 uuid);

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* GetObjectByInternalIndex(uint32 internalIndex);

	static TSparseArray<UObject*>& GetGObjectArray() { return GUObjectArray; }
	inline static uint64 GetGObjectRevision() { return GUObjectRevision; }

	static std::span<const FPropertyInfo> GetDeclaredProperties();


	/* Property Reflection */
	template<typename T>
	T* GetPropertyValueOrNull(std::string_view propertyKey) const
	{
		const FPropertyInfo* propertyInfo = GetRuntimeClass()->FindProperty(propertyKey);
		if (!propertyInfo)
		{
			return nullptr;
		}
		FPropertyValue value = propertyInfo->GetValue(this);
		if (std::holds_alternative<T>(value))
		{
			return &std::get<T>(value);
		}
		return nullptr;
	}

	FPropertyValue* GetPropertyValueOrNull(std::string_view propertyKey) const
	{
		const FPropertyInfo* propertyInfo = GetRuntimeClass()->FindProperty(propertyKey);
		if (!propertyInfo)
		{
			return nullptr;
		}
		FPropertyValue value = propertyInfo->GetValue(this);
		return new FPropertyValue(value);
	}

	void SetPropertyValue(std::string_view propertyKey, const FPropertyValue& value)
	{
		const FPropertyInfo* propertyInfo = GetRuntimeClass()->FindProperty(propertyKey);
		if (!propertyInfo)
		{
			throw std::runtime_error("Property not found: " + std::string(propertyKey));
		}
		propertyInfo->SetValue(*propertyInfo, this, value);
	}

	template<typename TVisitor>
	void ForEachProperty(TVisitor&& Visitor) const
	{
		GetRuntimeClass()->ForEachProperty(std::forward<TVisitor>(Visitor));
	}

public:
	static TSparseArray<UObject*> GUObjectArray;

protected:
	UObject();
	inline static uint64 GUObjectRevision = 0;

private:
	friend struct FObjectFactory;

	FName mName;
	const FClassInfo* mClassInfo = nullptr;
};


#include  "Object.inl"
