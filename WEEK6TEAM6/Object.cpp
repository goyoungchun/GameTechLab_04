#include "Object.h"
#include "EngineStatics.h"
#include "Json/json.hpp"
#include "FDuplicatedDataRW.h"
#include "Serializers.h"
#include "FArchive.h"
#include "JsonUtil.h"

TSparseArray<UObject*> UObject::GUObjectArray;
TMap<const FClassInfo*, TArray<uint32>> UObject::GUObjectMap;

UObject* FClassInfo::CreateInstance() const
{
	if (Constructor)
	{
		return Constructor();
	}
	return nullptr;
}

bool FClassInfo::IsChildOf(const FClassInfo* other) const
{
	const FClassInfo* currentClass = this;
	while (currentClass)
	{
		if (currentClass == other)
		{
			return true;
		}
		currentClass = currentClass->SuperClass;
	}
	return false;
}

UObject::UObject()
	: UUID(0)
	, InternalIndex(0)
	, ObjectMapIndex(0)
{
	GUObjectRevision++;
}

UObject::~UObject()
{
	GUObjectRevision++;
}

void UObject::Initialize()
{
}

const FClassInfo* UObject::GetStaticClass()
{
	static FClassInfo classInstance = FClassInfo(
		"UObject",
		nullptr,
		[]() -> UObject* { return new UObject(); }
	);
	return &classInstance;
}

void UObject::Serialize(FArchive& Ar)
{
	Ar << Name;
}

void UObject::Deserialize(FArchive& Ar)
{
	Ar << Name;
}

void UObject::SerializeClass(json::JSON& outJson) const
{
	outJson["ClassName"] = GetClass()->Name;

	json::JSON PropertiesJson = json::JSON::Make(json::JSON::Class::Object);
	PropertiesJson["UUID"] = UUID;
	PropertiesJson["Name"] = JsonUtils::ToJson(Name);
	PropertiesJson["Guid"] = JsonUtils::ToJson(Guid);
	outJson["Properties"] = PropertiesJson;
}

void UObject::DeserializeClass(const json::JSON& inJson)
{
	if (!inJson.hasKey("Properties") || inJson.at("Properties").JSONType() != json::JSON::Class::Object)
	{
		throw std::runtime_error("Invalid JSON format for Properties");
	}

	const json::JSON& PropertiesJson = inJson.at("Properties");
	if (!PropertiesJson.hasKey("UUID") || PropertiesJson.at("UUID").JSONType() != json::JSON::Class::Integral)
	{
		throw std::runtime_error("Invalid JSON format for UUID");
	}

	UUID = PropertiesJson.at("UUID").ToInt();
	Name = JsonUtils::FromJson<FName>(PropertiesJson.at("Name"));
	Guid = JsonUtils::FromJson<FGuid>(PropertiesJson.at("Guid"));
}

bool UObject::IsA(const FClassInfo* classInfo) const
{
	return GetClass()->IsChildOf(classInfo);
}

UObject* UObject::GetObjectByUUID(int32 uuid)
{
	for (const auto& object : GUObjectArray)
	{
		if (object && object->UUID == uuid)
		{
			return object;
		}
	}
	return nullptr;
}

UObject* UObject::GetObjectByInternalIndex(uint32 internalIndex)
{
	if (GUObjectArray.IsValidIndex(internalIndex))
	{
		return GUObjectArray[internalIndex];
	}
	return nullptr;
}
