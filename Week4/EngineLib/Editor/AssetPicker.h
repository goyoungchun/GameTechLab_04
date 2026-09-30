#pragma once
#include "Core/Core.h"
#include "Core/Name.h"

class FAssetManager;

class FAssetPicker
{
public:
	// StaticMesh DropList를 그리고, 선택값이 변경되면 true 반환
	static bool DrawStaticMeshPicker(const FAssetManager& assetManager, FName& outSelectedAssetKey);
	static bool DrawMaterialPicker(const FAssetManager& assetManager, FName& outSelectedAssetKey, uint32 materialSlotNumber);

private:
	// StaticMesh 목록 출력 및 선택 처리
	static bool drawStaticMeshList(const FAssetManager& assetManager, FName& outSelectedAssetKey);
	static bool drawMaterialList(const FAssetManager& assetManager, FName& outSelectedAssetKey);

	// Mesh 할당 해제용 None 항목
	static bool drawNoneOption(FName& outSelectedAssetKey);
};
