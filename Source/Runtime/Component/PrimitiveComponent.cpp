#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"


#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "Collision/FPhysSceneChaos.h"

#include <cmath>
#include <algorithm>

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";
			break;
		case EPrimitiveType::Cube:
			return "Cube";
			break;
		case EPrimitiveType::Cone:
			return "Cone";
			break;
		case EPrimitiveType::Plane:
			return "Plane";
			break;
		default:
			return "";
			break;
		}
	}
}

UPrimitiveComponent::UPrimitiveComponent()
{
	//SetMaterial(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	UnregisterComponent();
}

void UPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
	GetWorld()->GetPhysicsScene().RegisterPrimitive(this);
}

void UPrimitiveComponent::OnUnregister()
{
	GetWorld()->GetPhysicsScene().UnRegisterPrimitive(this);
	Super::OnUnregister();
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UPrimitiveComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{

}

bool UPrimitiveComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	const FStaticMeshData* Mesh = GetMeshData();
	return Mesh && TraceMesh(WorldRay, *Mesh, GetWorldMatrix(), OutHit);
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkRenderStateDirty(SceneProxy);
}

bool UPrimitiveComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	const FStaticMeshData* Mesh = GetMeshData();
	return Mesh && TraceMeshLocal(LocalRay, *Mesh, OutT);
}

void UPrimitiveComponent::OnTransformDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkDirty(SceneProxy);
}

bool UPrimitiveComponent::TraceMesh(const FRay& WorldRay, const FStaticMeshData& Mesh, const FMatrix& WorldMatrix, FHitResult& OutResult)
{
	FBox MeshBox = Mesh.AABB.GetWorldAABB(WorldMatrix);
	float BoxT;
	if (!RayIntersectsAABB(WorldRay, MeshBox.Min, MeshBox.Max, BoxT))
	{
		return false;
	}

	FRay LocalRay = ToLocalRay(WorldRay, WorldMatrix);
	float T = FLT_MAX;

	if (!RayIntersectsMesh(LocalRay, Mesh, T)) return false;

	OutResult.HitComponent = this;
	OutResult.Distance = T;
	OutResult.ImpactPoint = WorldRay.Origin + WorldRay.Direction * T;

	return true;
}



bool UPrimitiveComponent::TraceMeshLocal(const FRay& LocalRay, const FStaticMeshData& Mesh, float& OutT)
{
	return RayIntersectsMesh(LocalRay, Mesh, OutT);
}

void UPrimitiveComponent::OnPropertyChanged(const FString& PropertyName)
{
	// Details 편집, 직렬화 로드, SetMass 모두 동일한 검증 경로를 사용한다.
	if (PropertyName == "Mass" && (!std::isfinite(Mass) || Mass <= 0.0f))
		Mass = 1.0f;
	if (PropertyName == "COF" && (!std::isfinite(COF) || COF < 0.0f))
		COF = 0.2f;
	if (PropertyName == "Restitution")
		Restitution = std::isfinite(Restitution) ? std::clamp(Restitution, 0.0f, 1.0f) : 1.0f;
	Super::OnPropertyChanged(PropertyName);
	if (PropertyName == "Transform") return;
	// Visibility, geometry, text and material changes may affect cached rendering and bounds.
	MarkRenderStateDirty();
	MarkTransformDirty();
}

void UPrimitiveComponent::CreateRenderState()
{
    GetWorld()->GetScene().AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
    GetWorld()->GetScene().RemovePrimitive(this);
}

bool UPrimitiveComponent::GetCastShadow() const
{
	return bCastShadow;
}
void UPrimitiveComponent::SetCastShadow(bool CastShadow)
{
	bCastShadow = CastShadow;
	OnPropertyChanged("CastShadow");
}

bool UPrimitiveComponent::GetReceiveShadow() const
{
	return bReceiveShadow;
}
void UPrimitiveComponent::SetReceiveShadow(bool ReceiverShadow)
{
	bReceiveShadow = ReceiverShadow;
	OnPropertyChanged("ReceiveShadow");
}

bool UPrimitiveComponent::GetBackfaceDepth() const
{
	return bBackfaceDepth;
}
