#pragma once
#include "Container/TArray.h"
#include "Engine/Core.h"
#include "Engine/Object/EObjectDomain.h"

// 전방 선언
class UObject;

class GObjectStatics
{
private:

	inline static TArray<uint32> NextUUID = TArray<uint32>(static_cast<size_t>(EObjectDomain::MAX_ITEMS));
	inline static TArray<UObject*> ObjectArray;

public:

	static uint32 GenerateUUID(EObjectDomain Domain) { return NextUUID[static_cast<size_t>(Domain)]++; }
	static uint32 GetNextUUID(EObjectDomain Domain) { return NextUUID[static_cast<size_t>(Domain)]; }
	
	static void SetNextUUID(EObjectDomain Domain, uint32 UUID);

	static void AddObject(UObject* Object);
	static void DestoryObject(uint32 InternalIndex);

	static uint32 GetNextIndex();

	static void Release();
};

