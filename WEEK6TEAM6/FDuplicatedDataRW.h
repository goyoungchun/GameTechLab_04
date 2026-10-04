#pragma once

#include "Core.h"
#include "FArchive.h"
#include "TMap.h"
#include "TArray.h"
#include "Object.h"
#include "Serializers.h"
#include "ObjectFactory.h"

class FDuplicatedDataWriter : public FMemoryWriter
{
public:
	FDuplicatedDataWriter(TMap<UObject*, UObject*>& InDuplicatedObjects)
		: DuplicatedObjects(InDuplicatedObjects)
	{
	}

	void SerializeObject(UObject*& Object) override
	{
		if (Object && !DuplicatedObjects.Contains(Object))
		{
			UObject* DuplicatedObject = FObjectFactory::ConstructUnInitializedObject(Object->GetClass());
			DuplicatedObjects.Add(Object, DuplicatedObject);
			SerializedObjects.Add(Object);
		}

		Serialize(&Object, sizeof(UObject*));
	}

	void Commit()
	{
		for (int32 i = 0; i < SerializedObjects.Num(); ++i)
		{
			UObject* Original = SerializedObjects[i];
			Original->Serialize(*this);
		}
	}

	inline const TArray<UObject*>& GetSerializedObjects() const { return SerializedObjects; }

private:
	TMap<UObject*, UObject*>& DuplicatedObjects; // Key : Original  Value : Duplicated
	TArray<UObject*> SerializedObjects; // Duplicated and serialized
};

class FDuplicatedDataReader : public FMemoryReader
{
public:
	FDuplicatedDataReader(const TMap<UObject*, UObject*>& InDuplicatedObjects, const TArray<UObject*>& InSerializedObjects, const TArray<int8>& InData)
		: FMemoryReader(InData)
		, DuplicatedObjects(InDuplicatedObjects)
		, SerializedObjects(InSerializedObjects)
	{
	}

	void SerializeObject(UObject*& Object) override
	{
		UObject* Original = nullptr;
		if (!Serialize(&Original, sizeof(UObject*)))
		{
			return;
		}

		if (Original && DuplicatedObjects.Contains(Original))
		{
			Object = DuplicatedObjects[Original];
		}
		else
		{
			Object = nullptr;
		}
	}

	void Commit()
	{
		for (int32 i = 0; i < SerializedObjects.Num(); ++i)
		{
			UObject* Serialized = SerializedObjects[i];
			UObject* DuplicatedObject = DuplicatedObjects[Serialized];
			DuplicatedObject->Deserialize(*this);
		}
	}

private:
	const TMap<UObject*, UObject*>& DuplicatedObjects; // Key : Original  Value : Duplicated
	const TArray<UObject*>& SerializedObjects; // Duplicated and serialized
};