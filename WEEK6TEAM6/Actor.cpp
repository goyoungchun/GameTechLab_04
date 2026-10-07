#include "Actor.h"
#include "JsonUtil.h"
#include "RenderInfo.h"
#include "SceneComponent.h"
#include "UTextComponent.h"
#include "ObjectFactory.h"
#include "World.h"
#include "Serializers.h"
#include <format>

void AActor::Initialize()
{
	UObject::Initialize();
}

void AActor::BeginDestroy()
{
	if (mLevel)
	{
		mLevel->RemoveActor(UUID);
	}

	while (!mComponents.IsEmpty())
	{
		FObjectFactory::DestroyObject(mComponents.Last());
	}

	Super::BeginDestroy();
}

void AActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	int32 ComponentsCount = 0;
	int32 RootComponentIndex = -1;

	TArray<UActorComponent*> ComponentsArray;
	for (UActorComponent* Component : mComponents)
	{
		if (!Component->ShouldSerialize())
		{
			continue;
		}

		ComponentsArray.Add(Component);
		
		if (mRootComponent == Component)
		{
			RootComponentIndex = ComponentsCount;
		}

		++ComponentsCount;
	}

	Ar << RootComponentIndex;
	Ar << ComponentsArray;
}

void AActor::Deserialize(FArchive& Ar)
{
	Super::Deserialize(Ar);

	while (!mComponents.IsEmpty())
	{
		FObjectFactory::DestroyObject(mComponents.Last());
	}

	int32 RootComponentIndex = -1;
	Ar << RootComponentIndex;

	TArray<UActorComponent*> ComponentsArray;
	Ar << ComponentsArray;

	for (int32 i = 0; i < ComponentsArray.Num(); ++i)
	{
		UActorComponent* Component = ComponentsArray[i];
		if (i == RootComponentIndex)
		{
			mRootComponent = Component->Cast<USceneComponent>();
		}
		mComponents.Add(Component);
	}
}

void AActor::SerializeClass(json::JSON& OutJson) const
{
	Super::SerializeClass(OutJson);

	json::JSON componentsJson = json::JSON::Make(json::JSON::Class::Array);

	for (const UActorComponent* component : mComponents)
	{
		if (!component->ShouldSerialize())
		{
			continue;
		}

		json::JSON componentJson;
		component->SerializeClass(componentJson);
		componentsJson.append(std::move(componentJson));
	}

	OutJson["Properties"]["mComponents"] = componentsJson;
	OutJson["Properties"]["mRootComponent"] = JsonUtils::ToJson(mRootComponent ? mRootComponent->GetUniqueID() : FGuid());
}

void AActor::DeserializeClass(const json::JSON& inJson)
{
	UObject::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("mComponents") || propertiesJson.at("mComponents").JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error(std::format("{}: mComponents requires an array", GetClass()->Name));
	}

	const json::JSON& componentsJson = propertiesJson.at("mComponents");

	for (const auto& ComponentJson : componentsJson.ArrayRange())
	{
		if (!ComponentJson.hasKey("ClassName") || ComponentJson.at("ClassName").JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error(std::format("{}: ClassName requires a string", GetClass()->Name));
		}

		FString className(ComponentJson.at("ClassName").ToString());
		const FClassInfo* classInfo = FObjectFactory::GetClassInfoByName(className);
		if (!classInfo)
		{
			throw std::runtime_error(std::format("{}: Unknown class name: {}", GetClass()->Name, className));
		}

		UActorComponent* Component = FObjectFactory::ConstructUnInitializedObject(classInfo)->Cast<UActorComponent>();
		Component->DeserializeClass(ComponentJson);

		AddOwnedComponent(Component);
	}

	FGuid RootComponentID = JsonUtils::FromJson<FGuid>(propertiesJson.at("mRootComponent"));
	if (RootComponentID == FGuid())
	{
		mRootComponent = nullptr;
	}
	else
	{
		for (UActorComponent* Component : mComponents)
		{
			if (Component->GetUniqueID() == RootComponentID)
			{
				mRootComponent = Component->Cast<USceneComponent>();
				break;
			}
		}
	}
}

void AActor::AddOwnedComponent(UActorComponent* actorComponent)
{
	assert(actorComponent);
	assert(!mComponents.Contains(actorComponent));

	mComponents.Add(actorComponent);

	actorComponent->SetOwner(this);

	if (mLevel)
	{
		mLevel->GetWorld()->RegisterComponent(actorComponent);
	}
}

void AActor::SetRootComponent(USceneComponent* sceneComponent)
{
	assert(sceneComponent);
	assert(!mComponents.Contains(sceneComponent));

	mRootComponent = sceneComponent;
	AddOwnedComponent(sceneComponent);
}

void AActor::AttachToComponent(USceneComponent* ParentComponent)
{
	mRootComponent->SetupAttachment(ParentComponent);
}

USceneComponent* AActor::GetRootComponent() const
{
	return mRootComponent;
}

bool AActor::RemoveComponent(UActorComponent* Target)
{
	if (!mComponents.Contains(Target))
	{
		return false;
	}

	if (mLevel)
	{
		mLevel->GetWorld()->UnregisterComponent(Target);
	}

	if (Target == mRootComponent)
	{
		mRootComponent = nullptr;
	}
    Target->mOwner = nullptr;

	mComponents.Remove(Target);

	return true;
}

void AActor::CreateEditorComponents()
{
	UText3DComponent* Text3DComponent = CreateDefaultSubobject<UText3DComponent>(FName("UUIDDisplayer"));
	Text3DComponent->SetRelativeScale3D(FVector(0.01f, 0.01f, 0.01f));
	Text3DComponent->SetBillboard(true);
	Text3DComponent->SetText(Utf2Wide(std::format("UUID: {}", UUID)));
	Text3DComponent->SetFontAtlasAsset(FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FName("TestFontAtlas")));
	Text3DComponent->SetDepthState(false, false);
	Text3DComponent->SetEditorOnly(true);
	Text3DComponent->SetDoNotSerialize(true);

	if (mRootComponent)
	{
		Text3DComponent->SetupAttachment(mRootComponent, false);
	}

	AddOwnedComponent(Text3DComponent);

	TArray<UPointLightComponent*> PointLightComponents;
	for (UActorComponent* Component : mComponents)
	{
		UPointLightComponent* PointLightComponent = Component->Cast<UPointLightComponent>();
		if (PointLightComponent)
		{
			PointLightComponents.Add(PointLightComponent);
		}
	}

	for (UPointLightComponent* PointLightComponent : PointLightComponents)
	{
		UBillboardComponent* BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(FName("PointLightIcon"));
		BillboardComponent->SetTexture(FAssetManager::Get().GetAssetAs<FTexture2DAsset>(BuiltInAssetID::PointLightIcon, true));
		BillboardComponent->SetBlendState(ERenderBlendMode::Transparent);
		BillboardComponent->SetDepthState(false, false);
		BillboardComponent->SetEditorOnly(true);
		BillboardComponent->SetDoNotSerialize(true);
		BillboardComponent->SetVisualizeProxy(true);
		BillboardComponent->SetupAttachment(PointLightComponent, false);

		AddOwnedComponent(BillboardComponent);
	}
}

const FTransform& AActor::GetTransform() const
{
	if (mRootComponent)
	{
		return mRootComponent->GetTransform();
	}
	else
	{
		throw std::runtime_error(std::format("{}: Actor has no root component", GetClass()->Name));
	}
}

void AActor::Tick(float deltaTime)
{
    // 현재 월드의 갱신 단위는 컴포넌트입니다. 여기서 다시 순회하면 중복 Tick이 발생합니다.
}

void AActor::Render(FRenderCollector& RenderCollector)
{
	for (UActorComponent* component : mComponents)
	{
		component->Render(RenderCollector);
	}
}

void AActor::GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const
{
	assert(outRenderInfos);

	for (const UActorComponent* component : mComponents)
	{
		component->GetRenderInfos(outRenderInfos);
	}
}

bool AActor::GetFirstRenderInfo(FRenderInfo &outRenderInfo) const
{
	TArray<FRenderInfo> renderInfos;
	GetRenderInfos(&renderInfos);

	if (renderInfos.Num() == 0)
	{
		return false;
	}

	outRenderInfo = renderInfos[0];

	return true;
}

void AActor::SetLocation(FVector location)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeLocation(location);
	}
}

void AActor::SetRotation(FRotator rotation)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeRotation(ToQuaternion(rotation));
	}
}

void AActor::SetScale(FVector scale)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeScale3D(scale);
	}
}

ULevel* AActor::GetLevel() const
{
	return mLevel;
}

UWorld* AActor::GetWorld() const
{
	return mLevel ? mLevel->GetWorld() : nullptr;
}	
