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
	UWorld* CurrentWorld = GuiReference.WorldContext->World();
	UActorComponent* PrevSelectedComponent = GuiReference.SceneManager->GetSelectedComponent();

	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Object List Panel", nullptr, Flags);

	/* Object Lists */
	
	ImGui::SeparatorText("Object Lists");

	SelectedComponent = PrevSelectedComponent;
	SelectedActorDeleted = false;
	HasDragDropRequest = false;

	for (AActor* Actor : CurrentWorld->GetLevel()->GetActors())
	{
		USceneComponent* RootComponent = Actor->GetRootComponent();
		if (!RootComponent || RootComponent->HasParent())
		{
			continue;
		}

		RenderActorHierarchy(Actor, RootComponent);
	}

	ImGui::InvisibleButton("##outliner_invisible_button", ImVec2(0, 0));
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ACTOR_PTR"))
		{
			AActor* DraggedActor = *static_cast<AActor* const*>(Payload->Data);
			if (DraggedActor)
			{
				HasDragDropRequest = true;
				DragDropRequest.Parent = nullptr;
				DragDropRequest.Child = DraggedActor->GetRootComponent();
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (SelectedActorDeleted)
	{
		GuiReference.SceneManager->ResetSelectedComponent();
		assert(CurrentWorld != nullptr);
		CurrentWorld->GetLevel()->RemoveActor(SelectedComponent->GetOwner()->UUID);
		FObjectFactory::DestroyObject(SelectedComponent->GetOwner());
	}
	else
	{
		if (SelectedComponent != PrevSelectedComponent)
		{
			GuiReference.SceneManager->SetSelectedComponent(SelectedComponent);
		}

		if (HasDragDropRequest)
		{
			if (DragDropRequest.Parent == nullptr)
			{
				// If the parent is null, it means the actor is being dropped at the root level
				DragDropRequest.Child->DetachFromParent();
			}
			else
			{
				// Reparent the dragged actor to the current actor
				DragDropRequest.Child->SetupAttachment(DragDropRequest.Parent);
			}

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

	bool bSelected = SceneComponent == SelectedComponent;

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	NodeFlags |= bSelected ? ImGuiTreeNodeFlags_Selected : 0;
	NodeFlags |= OtherActorsRootComponents.Num() == 0 ? ImGuiTreeNodeFlags_Leaf : 0;

	ImGui::PushID(Actor->UUID); // Ensure unique ID for each child

	bool Open = ImGui::TreeNodeEx(std::format("{}", Actor->GetName().ToString().c_str()).c_str(), NodeFlags);
	if (ImGui::IsItemClicked())
	{
		SelectedComponent = SceneComponent;
	}

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("ACTOR_PTR", &Actor, sizeof(AActor*));
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
