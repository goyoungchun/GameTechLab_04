#include "Actor.h"
#include "JsonUtil.h"
#include "RenderInfo.h"
#include "SceneComponent.h"
#include "UTextComponent.h"
#include "ObjectFactory.h"
#include "World.h"
#include <format>

void AActor::Initialize()
{
	UObject::Initialize();

	mbPressed = false;
	mbStarted = false;
}

void AActor::BeginDestroy()
{
	if (mWorld)
	{
		mWorld->RemoveActor(UUID);
	}

	while (!mComponents.IsEmpty())
	{
		FObjectFactory::DestroyObject(mComponents.Last());
	}

	Super::BeginDestroy();
}

void AActor::SerializeClass(json::JSON& outJson) const
{
	UObject::SerializeClass(outJson);
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

	outJson["Properties"]["mComponents"] = componentsJson;
	outJson["Properties"]["mRootComponentUUID"] = mRootComponent ? mRootComponent->UUID : -1;
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

	for (const auto& componentJson : componentsJson.ArrayRange())
	{
		if (!componentJson.hasKey("ClassName") || componentJson.at("ClassName").JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error(std::format("{}: ClassName requires a string", GetClass()->Name));
		}
		FString className(componentJson.at("ClassName").ToString());

		const FClassInfo* classInfo = FObjectFactory::GetClassInfoByName(className);
		if (!classInfo)
		{
			throw std::runtime_error(std::format("{}: Unknown class name: {}", GetClass()->Name, className));
		}
		UActorComponent* component = static_cast<UActorComponent*>(FObjectFactory::LoadObject(classInfo, componentJson));
		AddOwnedComponent(component);
	}

	if (!propertiesJson.hasKey("mRootComponentUUID") || propertiesJson.at("mRootComponentUUID").JSONType() != json::JSON::Class::Integral)
	{
		throw std::runtime_error(std::format("{}: mRootComponentUUID requires an integral", GetClass()->Name));
	}
	int32 RootComponentUUID = propertiesJson.at("mRootComponentUUID").ToInt();
	if (RootComponentUUID == -1)
	{
		mRootComponent = nullptr;
	}
	else
	{
		for (UActorComponent* Component : mComponents)
		{
			if (Component->UUID == RootComponentUUID)
			{
				mRootComponent = static_cast<USceneComponent*>(Component);
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

	if (mWorld)
	{
		mWorld->RegisterComponent(actorComponent);
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

	if (mWorld)
	{
		mWorld->UnregisterComponent(Target);
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
	Text3DComponent->SetBillboard(true);
	Text3DComponent->SetText(Utf2Wide(std::format("UUID: {}", UUID)));
	Text3DComponent->SetFontAtlasAsset(FAssetManager::Get().GetAssetAs<FFontAtlasAsset>(FName("TestFontAtlas")));
	Text3DComponent->SetDepthState(false, false);
	Text3DComponent->SetEditorOnly(true);
	Text3DComponent->SetDoNotSerialize(true);

	if (mRootComponent)
	{
		Text3DComponent->SetupAttachment(mRootComponent);
	}
	
	AddOwnedComponent(Text3DComponent);
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

