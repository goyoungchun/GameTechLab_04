#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"

#include "Engine/World.h"

struct FTransform;
class ULightComponent;

struct FShadowMapPreview
{
	FString Label = "";
	ImTextureID Texture = ImTextureID_Invalid;
	ImVec2 UV0 = { 0.0f, 0.0f };
	ImVec2 UV1 = { 1.0f, 1.0f };
};

class FDetailsPanel : public IEditorPanel
{
public:
	FDetailsPanel() = default;
	~FDetailsPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Details"; }

	void SetTarget(USceneComponent* InTargetOrNull) { 
		Target = InTargetOrNull; 
		SelectedComponent = nullptr;
	}

	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetShadowMapProvider(std::function<TArray<FShadowMapPreview>(const ULightComponent*)> InProvider) { ShadowMapProvider = InProvider; }

private:
	void DrawComponentTree(USceneComponent* SceneComp);
	void DrawAddComponentPopup(AActor* Owner);
	UActorComponent* SelectedComponent = nullptr;

private:
	UWorld* World = nullptr;
	USceneComponent* Target = nullptr;
	std::function<TArray<FShadowMapPreview>(const ULightComponent*)> ShadowMapProvider;
};

