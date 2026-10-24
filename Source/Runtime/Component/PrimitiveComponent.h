#pragma once

#include "SceneComponent.h"
#include "../Render/Shader.h"
#include "../Render/Mesh.h"
#include "Render/RenderPacket.h"
#include "Render/GeometryGenerator.h"
#include "Collision/HitResult.h"
#include "Collision/PhysicsTypes.h"

class FPrimitiveSceneProxy;

// Unreal의 컴포넌트 충돌 이벤트와 동일한 인자 구성이다.
using FComponentBeginOverlapSignature = TMulticastDelegate<UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&>;
using FComponentEndOverlapSignature = TMulticastDelegate<UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32>;
using FComponentHitSignature = TMulticastDelegate<UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult&>;

enum class EPrimitiveType
{
	Sphere,
	Cube,
	Cone,
	Plane,

	Particle,
	ParticleQuad
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	// Material은 여기서 등록하지 않는다. 컴포넌트마다 머티리얼을 다루는 방식이 달라서
	// (빌보드는 단일 머티리얼, 스태틱 메시는 슬롯별 덮어쓰기) 각자 등록한다.
	REFLECT_START(ClassName)
		PROPERTY(bCastShadow)
		PROPERTY(bReceiveShadow)
		PROPERTY(bBackfaceDepth)
		PROPERTY_ENUM(MotionType,
			FEnumEntry(EPhysicsMotionType::Static, "Static"),
			FEnumEntry(EPhysicsMotionType::Kinematic, "Kinematic"),
			FEnumEntry(EPhysicsMotionType::Dynamic, "Dynamic"))
		PROPERTY_ENUM(ObjectChannel,
			FEnumEntry(ECollisionChannel::WorldStatic, "WorldStatic"),
			FEnumEntry(ECollisionChannel::WorldDynamic, "WorldDynamic"))
		PROPERTY(bEnableGravity)
		// 구조체 내부의 채널별 응답을 기존 Details/직렬화/복사 경로에 등록한다.
		InClass->AddProperty<ECollisionResponse>("WorldStatic",
			offsetof(ThisClass, CollisionResponses) + offsetof(FCollisionResponseContainer, WorldStatic),
			{ FEnumEntry(ECollisionResponse::Ignore, "Ignore"),
			  FEnumEntry(ECollisionResponse::Overlap, "Overlap"),
			  FEnumEntry(ECollisionResponse::Block, "Block") });
		InClass->AddProperty<ECollisionResponse>("WorldDynamic",
			offsetof(ThisClass, CollisionResponses) + offsetof(FCollisionResponseContainer, WorldDynamic),
			{ FEnumEntry(ECollisionResponse::Ignore, "Ignore"),
			  FEnumEntry(ECollisionResponse::Overlap, "Overlap"),
			  FEnumEntry(ECollisionResponse::Block, "Block") });
	REFLECT_END()
public:
	virtual void OnPropertyChanged(const FString& PropertyName) override;

	UPrimitiveComponent();
	virtual ~UPrimitiveComponent();
	void OnRegister() override;
	void OnUnregister() override;

	virtual void BeginPlay() override;

	FComponentBeginOverlapSignature OnComponentBeginOverlap;
	FComponentEndOverlapSignature OnComponentEndOverlap;
	// Block 접촉이 검출된 물리 Step마다 호출된다. BeginHit/EndHit 이벤트는 없다.
	FComponentHitSignature OnComponentHit;

	// Todo: subuv
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext);

	virtual const FStaticMeshData* GetMeshData() const { return nullptr; }

	// Outline처럼 컴포넌트를 통째로 한 번 더 그릴 때 쓰는 GPU 메시.
	// 메시가 없거나(텍스트) View에 따라 형상이 정해지는 컴포넌트(빌보드·파티클)는
	// 컴포넌트 월드 행렬만으로 같은 그림을 못 만들므로 nullptr을 돌려준다.
	virtual UStaticMesh* GetRenderMesh() const { return nullptr; }

	virtual int32 GetNumMaterials() const { return 0; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) {}
	//
	// FShader* GetShader() const { return Shader.get(); };

	void SetVisible(bool bInVisible) { SetVisibility(bInVisible); }

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit);
	virtual bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT);
	virtual FBox CalcLocalBounds() const override
	{
		const FStaticMeshData* Data = GetMeshData();
		return Data ? Data->AABB : Super::CalcLocalBounds();
	}

	FPrimitiveSceneProxy* GetSceneProxy() const { return SceneProxy; }

	void MarkRenderStateDirty() override;

	virtual void OnTransformDirty() override;

	bool GetCastShadow() const;
	void SetCastShadow(bool CastShadow);
	bool GetReceiveShadow() const;
	void SetReceiveShadow(bool ReceiveShadow);
	bool GetBackfaceDepth() const;

	EPhysicsMotionType GetMotionType() const { return MotionType; }
	void SetMotionType(EPhysicsMotionType InMotionType) { MotionType = InMotionType; OnPropertyChanged("MotionType"); }
	ECollisionChannel GetObjectChannel() const { return ObjectChannel; }
	void SetObjectChannel(ECollisionChannel InObjectChannel) { ObjectChannel = InObjectChannel; OnPropertyChanged("ObjectChannel"); }
	const FCollisionResponseContainer& GetCollisionResponses() const { return CollisionResponses; }
	void SetCollisionResponses(const FCollisionResponseContainer& InResponses) { CollisionResponses = InResponses; OnPropertyChanged("CollisionResponses"); }
	ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const
	{
		return CollisionResponses.GetResponse(Channel);
	}
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse InResponse)
	{
		switch (Channel)
		{
		case ECollisionChannel::WorldStatic:
			CollisionResponses.WorldStatic = InResponse;
			OnPropertyChanged("WorldStatic");
			break;
		case ECollisionChannel::WorldDynamic:
			CollisionResponses.WorldDynamic = InResponse;
			OnPropertyChanged("WorldDynamic");
			break;
		}
	}
	bool IsGravityEnabled() const { return bEnableGravity; }
	void SetEnableGravity(bool bInEnableGravity) { bEnableGravity = bInEnableGravity; OnPropertyChanged("bEnableGravity"); }

protected:
	bool ShouldCreateRenderState() const override { return true; }
	void CreateRenderState() override;
	void DestroyRenderState() override;
	bool TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult);
	bool TraceMeshLocal(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT);

private:
	friend class FScene;
	FPrimitiveSceneProxy* SceneProxy = nullptr;

	bool bCastShadow = true;
	bool bReceiveShadow = true;
	bool bBackfaceDepth = false;

	EPhysicsMotionType MotionType = EPhysicsMotionType::Static;
	ECollisionChannel ObjectChannel = ECollisionChannel::WorldStatic;
	FCollisionResponseContainer CollisionResponses;
	// 중력 적분은 Dynamic일 때만 이 설정을 사용한다.
	bool bEnableGravity = true;
};
