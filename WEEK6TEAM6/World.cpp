#include "World.h"
#include "RenderInfo.h"
#include "JsonUtil.h"
#include "Console.h"
#include "ObjectFactory.h"
#include "PrimitiveComponent.h"
#include "FBVH.h"
#include "FInstrumentor.h"
#include "UTextComponent.h"
#include "ShowFlags.h"
#include "FHiZOcclusionManager.h"
#include "FDuplicatedDataRW.h"
#include <format>

ULevel::~ULevel()
{
	for (AActor* Actor : mActors)
	{
		Actor->mLevel = nullptr;
		FObjectFactory::DestroyObject(Actor);
	}
}

void ULevel::AddActor(AActor* actor)
{
	assert(mWorld != nullptr);
	assert(GetActorIndex(actor->UUID) == -1);

	actor->mLevel = this;
	for (UActorComponent* component : actor->GetComponents())
	{
		mWorld->RegisterComponent(component);
	}

	mActors.Add(actor);

	if (mWorld->GetWorldType() == EWorldType::Editor)
	{
		actor->CreateEditorComponents();
	}
}

bool ULevel::RemoveActor(uint32 uuid)
{
	assert(mWorld != nullptr);

	int32 ActorIndex = GetActorIndex(uuid);
	if (ActorIndex == -1)
	{
		return false;
	}

	AActor* ActorToRemove = mActors[ActorIndex];
	for (UActorComponent* component : ActorToRemove->GetComponents())
	{
		mWorld->UnregisterComponent(component);
	}
	ActorToRemove->mLevel = nullptr;

	mActors.RemoveAtSwap(ActorIndex);

	return true;
}

int32 ULevel::GetActorIndex(uint32 actorUUID) const
{
	for (uint32 i = 0; i < mActors.Num(); ++i)
	{
		if (mActors[i]->UUID == actorUUID)
		{
			return i;
		}
	}

	return -1;
}

UWorld::UWorld()
	: mWorldType(EWorldType::Game)
{
	mLevel = CreateDefaultSubobject<ULevel>(FName("PersistentLevel"));
	mLevel->mWorld = this;
}

UWorld::~UWorld()
{
	FObjectFactory::DestroyObject(mLevel);
}

void UWorld::SerializeClass(json::JSON& outJson) const
{
	UObject::SerializeClass(outJson);
	json::JSON actorsJson = json::JSON::Make(json::JSON::Class::Array);

	for (const AActor* actor : mLevel->GetActors())
	{
		json::JSON actorJson;
		actor->SerializeClass(actorJson);
		actorsJson.append(std::move(actorJson));
	}

	outJson["Properties"]["mActors"] = actorsJson;
}

void UWorld::DeserializeClass(const json::JSON& inJson)
{
	UObject::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("mActors") || propertiesJson.at("mActors").JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error(std::format("{}: mActors requires an array", GetClass()->Name));
	}

	const json::JSON& actorsJson = propertiesJson.at("mActors");

	for (const auto& actorJson : actorsJson.ArrayRange())
	{
		if (!actorJson.hasKey("ClassName") || actorJson.at("ClassName").JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error(std::format("{}: ClassName requires a string", GetClass()->Name));
		}
		FString className(actorJson.at("ClassName").ToString());

		const FClassInfo* classInfo = FObjectFactory::GetClassInfoByName(className);
		if (!classInfo)
		{
			throw std::runtime_error(std::format("{}: Unknown class name: {}", GetClass()->Name, className));
		}
		AActor* actor = static_cast<AActor*>(FObjectFactory::LoadObject(classInfo, actorJson));
		mLevel->AddActor(actor);
	}
}

void UWorld::RegisterComponent(UActorComponent* Component)
{
	if (ComponentRegistrations.Contains(Component))
	{
		return;
	}

	UPrimitiveComponent* PrimitiveComponent = Component->Cast<UPrimitiveComponent>();
    ComponentRegistrations.Add(Component, { PrimitiveComponent, Component->IsRenderable() });
    RefreshComponentTick(Component);

	if (PrimitiveComponent)
	{
		mPrimitiveComponents.Add(PrimitiveComponent);
		mShouldRenderComponents.Add(Component);
		mbBVHDirty = true;
	}
	else if (Component->IsRenderable())
	{
        mNonPrimitiveRenderableComponents.Add(Component);
	}
}

void UWorld::UnregisterComponent(UActorComponent* Component)
{
    const auto* Found = ComponentRegistrations.Find(Component);
	if (!Found)
	{
		return;
	}

    const FComponentRegistration Registration = *Found;

    // 소멸 중 가상 타입에 의존하지 않고 등록 당시의 목록에서 제거합니다.
    mTickableComponents.Remove(Component);
    ComponentRegistrations.Remove(Component);

	UPrimitiveComponent* PrimitiveComponent = Registration.Primitive;
	if (PrimitiveComponent)
	{
		int32 index = mPrimitiveComponents.Find(PrimitiveComponent);
		if (index != -1)
		{
			mPrimitiveComponents.RemoveAtSwap(index);
			mbBVHDirty = true;
		}

		index = mShouldRenderComponents.Find(Component);
		if (index != -1)
		{
			mShouldRenderComponents.RemoveAtSwap(index);
		}
	}
	else if (Registration.bRenderable)
	{
		int32 index = mNonPrimitiveRenderableComponents.Find(Component);
		if (index != -1)
		{
			mNonPrimitiveRenderableComponents.RemoveAtSwap(index);
		}

		index = mShouldRenderComponents.Find(Component);
		if (index != -1)
		{
			mShouldRenderComponents.RemoveAtSwap(index);
		}
	}
}

void UWorld::RefreshComponentTick(UActorComponent* Component)
{
    // Owner만 연결되고 아직 월드에 등록되지 않은 컴포넌트는 실행하지 않습니다.
    const FComponentRegistration* Registration = ComponentRegistrations.Find(Component);
	if (!Registration)
	{
		return;
	}
    
	if (Component->IsTickable())
	{
		mTickableComponents.Add(Component);
	}
	else
	{
		mTickableComponents.Remove(Component);
	}
}

void UWorld::MarkBoundsDirty(UActorComponent* Component)
{
	UPrimitiveComponent* PrimitiveComponent = Component->Cast<UPrimitiveComponent>();
	if (PrimitiveComponent)
	{
		mBVH.Refit(PrimitiveComponent, PrimitiveComponent->GetBoundingBox());
		mBVH.GetAllBoundingBoxes(mCachedEntryAABBs);
		mbAABBsDirty = true;
	}
}

void UWorld::RequestRenderUpdate(UActorComponent* Component)
{
	mShouldRenderComponents.Add(Component);
}

void UWorld::Tick(ELevelTick LevelTick, float DeltaTime)
{
    {
        PROFILE_SCOPE("World/ActiveTick");
        mTickableComponents.Tick([LevelTick, DeltaTime](UActorComponent* Component)
        {
			if (LevelTick == ELevelTick::ViewportsOnly && !Component->bTickInEditor)
            {
                return;
            }

            Component->Tick(DeltaTime);
        });
    }

	if (mbBVHDirty)
	{
        PROFILE_SCOPE("World/BVHBuild");
		mBVH.Release();
		for (UPrimitiveComponent* primitiveComponent : mPrimitiveComponents)
		{
			mBVH.AddItem(primitiveComponent, primitiveComponent->GetBoundingBox());
		}
		mBVH.Build();
		mBVH.GetAllBoundingBoxes(mCachedEntryAABBs);
		mbBVHDirty = false;
		mbAABBsDirty = true;
	}
}

void UWorld::Render(float deltaTime, FRenderCollector& outCollector)
{
	{
		PROFILE_SCOPE("World/CollectNonPrimitive");
		for (UActorComponent* Component : mNonPrimitiveRenderableComponents)
		{
			Component->Render(outCollector);

			if (FRenderProxy* Proxy = Component->GetRenderProxy())
			{
				Proxy->Submit();
			}
		}
	}

	for (UActorComponent* Component : mShouldRenderComponents)
	{
		Component->Render(outCollector);
	}
	mShouldRenderComponents.Empty();

	if (FShowFlags::Get().IsEnabled(EShowFlag::Primitive))
	{
		if (mBVH.IsValid())
		{
			PROFILE_SCOPE("World/BVHQuery");
			const bool bOcclusionEnabled = FShowFlags::Get().IsEnabled(EShowFlag::OcclusionCulling);

			QueryStack.Empty();
			QueryStack.Add(mBVH.GetRootNode());

			while (!QueryStack.IsEmpty())
			{
				FBVHNode* CurrentNode = QueryStack.Last();
				QueryStack.Pop();
				if (!CurrentNode) continue;

				int32 CollisionResult = outCollector.Frustum.Intersects(CurrentNode->BoundingBox);
				if (CollisionResult == -1)
				{
					continue;
				}

				if (CollisionResult == 1 || CurrentNode->IsLeaf())
				{
					for (int32 i = 0; i < CurrentNode->ItemRange.Count; ++i)
					{
						int32 EntryIndex = CurrentNode->ItemRange.Offset + i;

#if ENABLE_OCCULSION_CULLING
						if (bOcclusionEnabled && FHiZOcclusionManager::Get().IsOccluded(EntryIndex))
						{
							FHiZOcclusionManager::Get().IncrementCulledCount();
							continue;
						}
#endif

						UPrimitiveComponent* Object = mBVH.GetPayload(EntryIndex);
						if (Object && Object->GetRenderProxy())
						{
							Object->GetRenderProxy()->Submit();
						}
					}
				}
				else
				{
					QueryStack.Add(CurrentNode->Left);
					QueryStack.Add(CurrentNode->Right);
				}
			}
		}
	}

	outCollector.BVH = &mBVH;
}

UWorld* UWorld::CreateWorld(EWorldType WorldType)
{
	UWorld* NewWorld = FObjectFactory::ConstructUnInitializedObject<UWorld>();
	NewWorld->mWorldType = WorldType;
	return NewWorld;
}

UWorld* UWorld::DuplicateWorldForPIE(UWorld* SourceWorld)
{
	TMap<UObject*, UObject*> ObjectMap;
	for (AActor* Actor : SourceWorld->mLevel->GetActors())
	{
		if (ObjectMap.Contains(Actor))
		{
			continue;
		}

		UObject* Duplicated = FObjectFactory::ConstructUnInitializedObject(Actor->GetClass());
		ObjectMap.Add(Actor, Duplicated);

		FDuplicatedDataWriter DuplicatedDataWriter(ObjectMap);
		Actor->Serialize(DuplicatedDataWriter);
		DuplicatedDataWriter.Commit();

		FDuplicatedDataReader DuplicatedDataReader(ObjectMap, DuplicatedDataWriter.GetSerializedObjects(), DuplicatedDataWriter.GetData());
		Duplicated->Deserialize(DuplicatedDataReader);
		DuplicatedDataReader.Commit();
	}

	UWorld* DuplicatedWorld = FObjectFactory::ConstructUnInitializedObject<UWorld>();
	DuplicatedWorld->mWorldType = EWorldType::PIE;

	for (const auto& [Original, Duplicated] : ObjectMap)
	{
		AActor* DuplicatedActor = Duplicated->Cast<AActor>();
		if (DuplicatedActor)
		{
			DuplicatedWorld->mLevel->AddActor(DuplicatedActor);
		}
	}

	return DuplicatedWorld;
}
