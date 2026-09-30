#include "ObjectFactory.h"

#include "ThirdParty/Json/json.hpp"

#include "Core/AssetManager.h"
#include "Engine/Actor.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Engine/Components/NameComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Engine/Components/CubeComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/Components/StaticMeshComponent.h"
#include "Engine/Components/SceneComponent.h"

#include "Rendering/FontResource.h"

#include "Object.h"

const FFontResource* FObjectFactory::mDefaultFontResource = nullptr;
const FAssetManager* FObjectFactory::mAssetManagerRef = nullptr;


void FObjectFactory::SetDefaultFont(const FFontResource& fontResource)
{
	mDefaultFontResource = &fontResource;
}

void FObjectFactory::SetAssetManager(const FAssetManager& assetManager)
{
	mAssetManagerRef = &assetManager;
}

const FFontResource* FObjectFactory::GetDefaultFontResource()
{
	return mDefaultFontResource;
}

UObject* FObjectFactory::ConstructUnInitializedObject(const FClassInfo* classInfo)
{
	if (!classInfo || !classInfo->Constructor)
	{
		return nullptr;
	}

	UObject* instance = classInfo->CreateInstance();

	if (instance)
	{
		instance->mClassInfo = classInfo;
		instance->mName = FName(classInfo->Name);
	}
	return instance;
}

UObject* FObjectFactory::LoadObject(const FClassInfo* classInfo, const json::JSON& inJson)
{
	UObject* instance = ConstructUnInitializedObject(classInfo);

	if (instance)
	{
		instance->DeserializeClass(inJson);
	}
	return instance;
}

//AActor* FObjectFactory::SpawnPrimitiveActor(
//	EPrimitive primitiveType,
//	FVector3 Location, FRotator Rotation, FVector3 Scale)
//{
//	FName PrimitiveName(PrimitiveToString(primitiveType));
//
//	AActor* actor = ConstructObjectWithName<AActor>(PrimitiveName);
//
//	UPrimitiveComponent* component = nullptr;
//
//	if (primitiveType == EPrimitive::EP_Cube)
//	{
//		component = ConstructObject<UCubeComponent>(Location, Rotation, Scale);
//	}
//	else if (primitiveType == EPrimitive::EP_Sphere)
//	{
//		component = ConstructObject<USphereComponent>(Location, Rotation, Scale);
//	}
//	else
//	{
//		component = ConstructObject<UPrimitiveComponent>(
//			primitiveType, Location, Rotation, Scale);
//	}
//
//	actor->AddRootSceneComponent(component);
//
//	/* DEBUG */
//	assert(mDefaultFontResource && "FObjectFactory::Initialize must be called before SpawnPrimitiveActor.");
//	UNameComponent& billboardComponent = actor->CreateAndAddComponent<UNameComponent>(
//		actor->GetName().ToString(), FVector3{0, 0, 1}, *mDefaultFontResource);
//	billboardComponent.AttachTo(*component);
//	return actor;
//}

AActor* FObjectFactory::SpawnStaticMeshActor(
	FVector3 location, FRotator rotation, FVector3 scale,
	const UStaticMesh& staticMeshAsset)
{
	FName StaticMeshName("StaticMesh");

	return SpawnActorWithRootComponent<UStaticMeshComponent>(
		StaticMeshName, location, rotation, scale, &staticMeshAsset);
}

AActor* FObjectFactory::SpawnStaticMeshActor(
	FVector3 location, FRotator rotation, FVector3 scale,
	FName staticMeshAssetName)
{
	assert(mAssetManagerRef && "FObjectFactory::Initialize must be called before SpawnStaticMeshActor.");

	const UStaticMesh* staticMeshAsset = mAssetManagerRef->FindStaticMeshAssetOrNull(staticMeshAssetName);
	if (!staticMeshAsset)
	{
		return nullptr;
	}
	return SpawnStaticMeshActor(location, rotation, scale, *staticMeshAsset);
}

AActor* FObjectFactory::SpawnParticleActor(FVector3 Location, FRotator Rotation, FVector3 Scale)
{
	FName ParticleName("Particle");
	AActor* actor = ConstructObjectWithName<AActor>(ParticleName);

	UParticleSubUVComponent* component = ConstructObject<UParticleSubUVComponent>(
		Location, Rotation, Scale, 6, 6, true, 1.0f, 0.1f);
	actor->AddRootSceneComponent(component);

	/* DEBUG */
	//assert(mDefaultFontResource && "FObjectFactory::Initialize must be called before SpawnParticleActor.");
	//UNameComponent& billboardComponent = actor->CreateAndAddComponent<UNameComponent>(
	//	actor->GetName().ToString(), FVector3{ 0, 0, 1 }, *mDefaultFontResource);
	//billboardComponent.AttachTo(*component);
	return actor;
}

const FClassInfo* FObjectFactory::GetClassInfoByName(const FString& className)
{
	const FName classKey(className);

	if (!mClassInfoMap.Contains(classKey))
	{
		return nullptr;
	}

	return mClassInfoMap[classKey]();
}

bool FObjectFactory::RegisterClassInfo(FString className, const FClassInfo* classInfo)
{
	const FName classKey(className);

	if (mClassInfoMap.Contains(classKey))
	{
		return false;
	}
	mClassInfoMap.Add(classKey, [classInfo]() -> const FClassInfo* { return classInfo; });
	return true;
}

AActor* FObjectFactory::createActorWithRootComponent(const FName& Name, USceneComponent* rootComponent)
{
	AActor* actor = ConstructObjectWithName<AActor>(Name);
	if (!actor)
	{
		return nullptr;
	}
	if (rootComponent)
	{
		actor->AddRootSceneComponent(rootComponent);
	}

	// Add name component
	assert(mDefaultFontResource && "FObjectFactory::Initialize must be called before SpawnStaticMeshActor.");
	UNameComponent& billboardComponent = actor->CreateAndAddComponent<UNameComponent>(
		actor->GetName().ToString(), FVector3{ 0, 0, 0.2 }, *mDefaultFontResource);
	billboardComponent.AttachTo(*rootComponent);

	return actor;
}



#include "Engine/Components/SceneComponent.h"
#include "Engine/Components/CubeComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/Components/BillboardComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"

TMap<FName, std::function<const FClassInfo* ()>> FObjectFactory::mClassInfoMap = {
	{"UObject", &UObject::GetClass },
	{"AActor", &AActor::GetClass },
	{"UActorComponent", &UActorComponent::GetClass },
	{"USceneComponent", &USceneComponent::GetClass },
	{"UPrimitiveComponent", &UPrimitiveComponent::GetClass },
	{"UCubeComponent", &UCubeComponent::GetClass },
	{"USphereComponent", &USphereComponent::GetClass },
	{"UBillboardComponent", &UBillboardComponent::GetClass },
	{"UWorld", &UWorld::GetClass },
	{"UNameComponent",& UNameComponent::GetClass },
	{"UParticleSubUVComponent",&UParticleSubUVComponent::GetClass },
	{ "UMeshComponent", &UMeshComponent::GetClass },
	{ "UStaticMeshComponent", &UStaticMeshComponent::GetClass },
};
