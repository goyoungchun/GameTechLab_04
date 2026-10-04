#pragma once

#include "Core.h"
#include "TArray.h"
#include "ImGui/imgui.h"

struct FGuiReference;
class FEditorEngine;
class UObject;
class AActor;
class USceneComponent;
class UActorComponent;

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

	UActorComponent* SelectedComponent;
	bool SelectedActorDeleted;
	bool HasDragDropRequest;
	FDragDropRequst DragDropRequest;
};
