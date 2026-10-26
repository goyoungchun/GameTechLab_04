#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "Component/PrimitiveComponent.h"
#include "Component/TextRenderComponent.h"
#include "Math/Transform.h"
#include "Render/Renderer.h"
#include "PathTracker.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Engine/Scene.h"
#include "Camera/CameraActor.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "UI/GameUIBuffer.h"

//class ACameraActor;
class ULevel;
class UBillboardComponent;
class FPhysSceneChaos;

struct FViewContext;

enum EWorldType
{
	None,
	Editor,
	PIE,
};

class UWorld;

enum class EWorldTick : uint8
{
	TimeOnly,
	ViewportsOnly,
	PauseTick,
	All,
};






class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

public:
	UWorld();
	virtual ~UWorld();

	bool Init();
	/*UPrimitiveComponent* SpawnPrimitive(FClass* Class);*/
	AActor* SpawnActor(UClass* Class, FName InName = NAME_None, const FTransform* Transform = nullptr);

	template <class T>
	T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), InName, Transform));
	}

	void Tick(EWorldTick TickType, float DeltaTime);

	void ClearWorld();


	void CreateMainCamera();

	// 카메라 Get/Set
	void SetMainCamera(ACameraActor* Camera);
	ACameraActor* GetMainCamera() const { return MainCamera; }

	static constexpr int32 MaxViewCameraOverrides = 2;
	void SetViewCameraOverride(int32 PlayerIndex, const FVector& Location, const FRotator& Rotation);
	void ClearViewCameraOverride(int32 PlayerIndex);
	UCameraComponent* GetViewCameraOverride(int32 PlayerIndex) const;

	// Level
	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	TArray<ULevel*> GetLevel()  { return Levels; }
	void AddLevel(ULevel* InLevel) { Levels.Add(InLevel); }
	void SetPersistentLevel(ULevel* InLevel) { PersistentLevel = InLevel; }

	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }

	FPathTracker& GetPathTracker() { return PathTracker; }

	int32 GetActorNum();

	void AddDestroyList(AActor* InActor);
	void FlushDestroyList();

	bool DestroyActor(AActor* Actor);

	// View별 Billboard 행렬 공급자는 이 동기 호출 동안만 사용하며 저장하지 않는다.
	using FBillboardTraceTransform = FMatrix(*)(const UBillboardComponent&, const void*);
	// 현재 World의 Component에 Ray를 전달하고 가장 가까운 유효 교차를 반환한다.
	bool LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
		FBillboardTraceTransform ResolveBillboard = nullptr, const void* ViewContext = nullptr);
	// 가장 가까운 Block만 OutHit에 반환한다. Overlap은 Block까지의 실제 이동 구간에서 따로 수집한다.
	bool SweepSingle(const FRay& WorldRay, float MaxDistance, float Radius, FHitResult& OutHit,
		AActor* IgnoreActor = nullptr, const UPrimitiveComponent* MovingComponent = nullptr,
		TArray<FHitResult>* OutOverlaps = nullptr);

	void BeginPlay();
	bool HasBegunPlay() const { return bBegunPlay; }
	void EndPlay();

	FScene& GetScene() { return Scene; }
	FPhysSceneChaos& GetPhysicsScene() { return *PhysicsScene; }
	const FPhysSceneChaos& GetPhysicsScene() const { return *PhysicsScene; }
	FTickTaskManager& GetTickTaskManager() { return TickTaskManager; }


	EWorldType& GetWorldType() { return WorldType; }
	
	FGameUIBuffer& GetGameUIBuffer() { return GameUIBuffer; }

	bool& GetbIsPause() { return bIsPaused; }

	// Duplicate into an uninitialized world; restore internal references before proxy registration.
	bool DuplicateWorld(UWorld* SrcWorld);

	// 컨트롤러에 Pawn 등록
	int32 RegisterPlayer(APawn* InPawnPtr);

	uint32 GetPlayerCount() const { return static_cast<uint32>(PlayerControllers.Num()); }
	APlayerController* GetPlayerController(uint32 Index = 0) const;
	APawn* GetPlayerPawn(uint32 Index = 0) const;

	// 화면 분할
	void SetSplitScreenEnable(bool bEnable) { bSplitScreenEnabled = bEnable; }
	bool IsSplitScreenEnabled() const { return bSplitScreenEnabled; }

private:
	// 등록된 Tick 함수만 실행한다. Actor보다 먼저 사라져도 남은 함수와의 연결을 스스로 끊는다.
	FTickTaskManager TickTaskManager;

	TQueue<AActor*> BeginPlayList;

	TArray<AActor*> DestroyList;

	//메인 카메라 
	ACameraActor* MainCamera = nullptr;

	// 레벨에 속하지 않는 카메라. Tick을 등록하지 않아 입력으로 움직이지 않는다.
	ACameraActor* ViewCameraOverrides[MaxViewCameraOverrides] = {};
	bool bViewCameraOverrideActive[MaxViewCameraOverrides] = {};

	// 플레이어 폰 및 컨트롤러
	//APawn* PlayerPawn = nullptr;
	//APlayerController* PlayerController = nullptr;

	// 1월드 다중 플레이어 위함
	TArray<APlayerController*> PlayerControllers;
	// 원래 언리얼에서는 PlayerController 만 갖고있음.
	// Controller 를 통해서 pawn에 접근하는 방식.

	FPathTracker PathTracker;

	ULevel* PersistentLevel = nullptr;
	ULevel* CurrentLevel = nullptr;
	TArray<ULevel*> Levels;
	EWorldType WorldType = EWorldType::None;
	EWorldTick CurrentTickType = EWorldTick::All;
	bool bBegunPlay = false;
	bool bIsPaused = false;

	FScene Scene;
	TUniquePtr<FPhysSceneChaos> PhysicsScene;

	FGameUIBuffer GameUIBuffer;

	// 뷰포트 분할 여부
	bool bSplitScreenEnabled = false;
};
