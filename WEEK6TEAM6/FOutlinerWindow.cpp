#include "FOutlinerWindow.h"
#include "ObjectFactory.h"
#include "Actor.h"
#include "SceneManager.h"
#include "Object.h"
#include "World.h"
#include "FEditorUIManager.h"
#include "FInstrumentor.h"
#include "SceneComponent.h"
#include <algorithm>

void FOutlinerWindow::Render(const FGuiReference& GuiReference)
{
	PROFILE_FUNCTION();

	ImGuiIO& io = ImGui::GetIO();
	UWorld* CurrentWorld = GuiReference.SceneManager->GetCurrentWorld();
	AActor* PrevSelectedActor = GuiReference.SceneManager->GetSelectedActor();

	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Object List Panel", nullptr, Flags);

	/* Object Lists */
	
	ImGui::SeparatorText("Object Lists");

	SelectedActor = PrevSelectedActor;
	SelectedActorDeleted = false;
	HasDragDropRequest = false;

	for (AActor* Actor : CurrentWorld->GetActors())
	{
		USceneComponent* RootComponent = Actor->GetRootComponent();
		if (!RootComponent || RootComponent->HasParent())
		{
			continue;
		}

		RenderActorHierarchy(Actor, RootComponent);
	}

	if (SelectedActorDeleted)
	{
		GuiReference.SceneManager->ResetSelectedActor();
		assert(CurrentWorld != nullptr);
		CurrentWorld->RemoveActor(SelectedActor->UUID);
		// TODO: DestroyActor
	}
	else
	{
		if (SelectedActor != PrevSelectedActor)
		{
			GuiReference.SceneManager->SetSelectedActor(SelectedActor);
		}

		if (HasDragDropRequest)
		{
			// Reparent the dragged actor to the current actor
			DragDropRequest.Child->SetupAttachment(DragDropRequest.Parent);
			HasDragDropRequest = false;
		}
	}

	ImGui::End();
}

void FOutlinerWindow::RenderActorHierarchy(AActor* Actor, USceneComponent* SceneComponent)
{
	TArray<USceneComponent*> OtherActorsRootComponents;
	for (USceneComponent* Child : SceneComponent->GetChildComponents())
	{
		if (Child->GetOwner() != Actor)
		{
			OtherActorsRootComponents.Add(Child);
		}
	}

	bool bSelected = Actor == SelectedActor;

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	NodeFlags |= bSelected ? ImGuiTreeNodeFlags_Selected : 0;
	NodeFlags |= OtherActorsRootComponents.Num() == 0 ? ImGuiTreeNodeFlags_Leaf : 0;

	ImGui::PushID(Actor->UUID); // Ensure unique ID for each child

	bool Open = ImGui::TreeNodeEx(std::format("UUID: {}", Actor->UUID).c_str(), NodeFlags);
	if (ImGui::IsItemClicked())
	{
		SelectedActor = Actor;
	}

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("ACTOR_PTR", &Actor, sizeof(AActor));
		ImGui::Text("Dragging Actor UUID: %d", Actor->UUID);
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ACTOR_PTR"))
		{
			AActor* DraggedActor = *static_cast<AActor* const*>(Payload->Data);
			if (DraggedActor)
			{
				HasDragDropRequest = true;
				DragDropRequest.Parent = SceneComponent;
				DragDropRequest.Child = DraggedActor->GetRootComponent();
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (Open)
	{
		if (bSelected)
		{
			ImGui::SameLine();

			if (ImGui::Button("Delete"))
			{
				SelectedActorDeleted = true;
			}
		}

		if (!SelectedActorDeleted)
		{
			for (USceneComponent* Child : OtherActorsRootComponents)
			{
				RenderActorHierarchy(Child->GetOwner(), Child);
			}
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}
