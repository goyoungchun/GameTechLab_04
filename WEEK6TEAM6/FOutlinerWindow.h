#pragma once

#include "Core.h"
#include "TArray.h"
#include "ImGui/imgui.h"

struct FGuiReference;
class FSceneManager;
class UObject;
class AActor;
class USceneComponent;

class FOutlinerWindow
{
public:
	void Render(const FGuiReference& GuiReference);

private:
	void RenderActorHierarchy(AActor* Actor, USceneComponent* SceneComponent);

private:
	struct FDragDropRequst
	{
		USceneComponent* Parent;
		USceneComponent* Child;
	};

	AActor* SelectedActor;
	bool SelectedActorDeleted;
	bool HasDragDropRequest;
	FDragDropRequst DragDropRequest;
};
