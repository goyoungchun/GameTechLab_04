#pragma once

#include <memory>
#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "Rendering/Mesh/Material.h"

struct FStaticMesh;

// 바이너리 또는 Obj에서 읽어 온 공통 결과를 담는 구조체
struct FStaticMeshCookedData
{
	std::unique_ptr<FStaticMesh> meshData;
	TArray<FMaterialSlot> materialSlots;
};

namespace StaticMeshLoader
{
	bool Load(const FString& sourcePath, FStaticMeshCookedData& outResult);
}
