#pragma once

#include "Object.h"
#include "ActorComponent.h"
#include "TSet.h"
#include "enum.h"

class UWorld;
class ULevel;
struct FRenderInfo;
struct FTransform;
class USceneComponent;
class FRenderCollector;

class AActor : public UObject
{
	REFLECT_CLASS(AActor, UObject)

public:
	AActor() = default;
	virtual ~AActor() = default;

	void Initialize();

	void BeginDestroy() override;

	virtual void Serialize(FArchive& Ar) override;
	virtual void Deserialize(FArchive& Ar) override;
	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	void AddOwnedComponent(UActorComponent* actorComponent);
	void SetRootComponent(USceneComponent* sceneComponent);

	void AttachToComponent(USceneComponent* ParentComponent);

	inline bool HasComponent(UActorComponent* Target) const { return mComponents.Contains(Target); }

	USceneComponent* GetRootComponent() const;
	bool RemoveComponent(UActorComponent* Target);
	inline const TSet<UActorComponent*>& GetComponents() const { return mComponents; }

	virtual void CreateEditorComponents();

	const FTransform& GetTransform() const;

    // 컴포넌트 Tick은 World의 활성 목록에서 직접 실행합니다.
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason EndPlayReason);
	virtual void Tick(float deltaTime);
	virtual void Render(FRenderCollector& RenderCollector);

	void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const;
	bool GetFirstRenderInfo(FRenderInfo& outRenderInfo) const;

	void SetLocation(FVector location);
	void SetRotation(FRotator rotation);
	void SetScale(FVector scale);

	ULevel* GetLevel() const;
	UWorld* GetWorld() const;

private:
	friend class ULevel;
	friend class UWorld;

	ULevel* mLevel = nullptr;

	USceneComponent* mRootComponent = nullptr;
	TSet<UActorComponent*> mComponents;
};

inline const FVector Up = FVector(0, 0, 1);
inline const FVector Right = FVector(0, 1, 0);
inline const FVector Front = FVector(1, 0, 0);
