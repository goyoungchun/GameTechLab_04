#pragma once

#include "Core.h"
#include "ImGui/imgui.h"

struct FGuiReference;
class AActor;
class UText3DComponent;
class USpotLightComponent;
class UAtlasAnimationComponent;
class UStaticMeshComponent;
class FAssetManager;
class USceneComponent;
class UActorComponent;
class UHeightFogComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UProjectileMovementComponent;
class URotationMovementComponent;

class FPropertyWindow
{
public:
	void Render(const FGuiReference& GuiReference);

private:
	void RenderAddComponentPopup();

	void RenderSceneComponentHierarchy(USceneComponent* SceneComponent);

	void RenderTransformProperties(USceneComponent* SceneComponent);
	void RenderText3DComponent(UText3DComponent* text3DComponent);
	void RenderSpotLightComponent(USpotLightComponent* spotLightComponent);
	void RenderAtlasAnimationComponent(UAtlasAnimationComponent* atlasAnimationComponent);
	void RenderStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent);
	void RenderHeightFogComponent(UHeightFogComponent* HeightFogComponent);
	void RenderPointLightComponent(UPointLightComponent* PointLightComponent);
	void RenderTextRenderComponent(UTextRenderComponent* TextRenderComponent);
	void RenderProjectileMovementComponent(UProjectileMovementComponent* ProjectileMovementComponent);
	void RenderRotationMovementComponent(URotationMovementComponent* RotationMovementComponent);

private:
	FAssetManager* mAssetManager;

	AActor* mSelectedActor = nullptr;
	UActorComponent* mSelectedComponent = nullptr;
};