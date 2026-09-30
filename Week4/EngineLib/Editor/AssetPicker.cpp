#include "AssetPicker.h"

#include <cstring>

#include "Core/AssetManager.h"
#include "ThirdParty/ImGui/imgui.h"

bool FAssetPicker::DrawStaticMeshPicker(const FAssetManager& assetManager, FName& outSelectedAssetKey)
{
	const FString previewName = outSelectedAssetKey.ToString();
	bool bChanged = false;

	if (ImGui::BeginCombo("Static Mesh", previewName.CStr()))
	{
		bChanged = drawStaticMeshList(
			assetManager,
			outSelectedAssetKey);

		ImGui::EndCombo();
	}

	// Drag and Drop
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload =
			ImGui::AcceptDragDropPayload("ASSET_STATIC_MESH"))
		{
			if (payload->DataSize == sizeof(FName))
			{
				FName droppedKey;
				std::memcpy(
					&droppedKey,
					payload->Data,
					sizeof(droppedKey));

				if (assetManager.FindStaticMeshAssetOrNull(droppedKey)
					&& !(droppedKey == outSelectedAssetKey))
				{
					outSelectedAssetKey = droppedKey;
					bChanged = true;
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	return bChanged;
}

bool FAssetPicker::DrawMaterialPicker(const FAssetManager& assetManager, FName& outSelectedAssetKey, uint32 materialSlotNumber)
{
	bool bChanged = false;

	const FString previewName = outSelectedAssetKey.ToString();
	const FString materialSlotLabel =
		FString(std::format("Material Slot {}", materialSlotNumber));

	if (ImGui::BeginCombo(
		materialSlotLabel.CStr(),
		previewName.CStr()))
	{
		bChanged = drawMaterialList(
			assetManager,
			outSelectedAssetKey);

		ImGui::EndCombo();
	}

	// Drag and Drop
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload =
			ImGui::AcceptDragDropPayload("ASSET_MATERIAL"))
		{
			if (payload->DataSize == sizeof(FName))
			{
				FName droppedKey;
				std::memcpy(
					&droppedKey,
					payload->Data,
					sizeof(droppedKey));

				if (assetManager.FindMaterialAssetOrNull(droppedKey)
					&& !(droppedKey == outSelectedAssetKey))
				{
					outSelectedAssetKey = droppedKey;
					bChanged = true;
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	return bChanged;
}

bool FAssetPicker::drawStaticMeshList(const FAssetManager& assetManager, FName& outSelectedAssetKey)
{
	bool bChanged = drawNoneOption(outSelectedAssetKey);

	const TArray<FName> staticMeshAssetKeys = assetManager.GetAllStaticMeshAssetKeys();

	for (const FName& assetKey : staticMeshAssetKeys)
	{
		const FString assetName = assetKey.ToString();
		const bool bSelected = assetKey == outSelectedAssetKey;

		if (ImGui::Selectable(assetName.CStr(), bSelected))
		{
			outSelectedAssetKey = assetKey;
			bChanged = true;
		}

		if (bSelected) ImGui::SetItemDefaultFocus();
	}

	return bChanged;
}

bool FAssetPicker::drawMaterialList(const FAssetManager& assetManager, FName& outSelectedAssetKey)
{
	bool bChanged = drawNoneOption(outSelectedAssetKey);
	const TArray<FName> materialAssetKeys = assetManager.GetAllMaterialAssetKeys();
	for (const FName& assetKey : materialAssetKeys)
	{
		const FString assetName = assetKey.ToString();
		const bool bSelected = assetKey == outSelectedAssetKey;
		if (ImGui::Selectable(assetName.CStr(), bSelected))
		{
			outSelectedAssetKey = assetKey;
			bChanged = true;
		}
		if (bSelected) ImGui::SetItemDefaultFocus();
	}
	return bChanged;
}

bool FAssetPicker::drawNoneOption(FName& outSelectedAssetKey)
{
	const bool bSelected = outSelectedAssetKey == FName();

	if (!ImGui::Selectable("None", bSelected)) return false;

	outSelectedAssetKey = FName();
	return true;
}
