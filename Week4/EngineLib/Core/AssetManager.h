// EngineLib/Core/AssetManager.h

#pragma once

#include <memory>

#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Core/Name.h"

#include "Rendering/Mesh/StaticMesh.h"


// In charge of managing assets and **resources**
// TODO: separate resource manager from asset manager
// Assets: UStaticMesh, etc
// Resources: FStaticMesh, etc
class FAssetManager
{
public:
	FAssetManager();

	/* Assets */
	const UStaticMesh* FindStaticMeshAssetOrNull(const FName& assetName) const;
	const UStaticMesh& FindStaticMeshAssetOrAdd(const FName& assetName);
	const UMaterial* FindMaterialAssetOrNull(const FName& assetName) const;

	TArray<FName> GetAllStaticMeshAssetKeys() const;
	TArray<FName> GetAllMaterialAssetKeys() const;

	uint32 GetStaticMeshAssetCount() const { return mStaticMeshAssets.Num(); }
	uint32 GetMaterialAssetCount() const { return mMaterialAssets.Num(); }

private:
	/* Assets */
	TMap<FName, std::unique_ptr<UStaticMesh>> mStaticMeshAssets;
	TMap<FName, std::unique_ptr<UMaterial>> mMaterialAssets;

	void createBuiltinStaticMeshAssets();
	void createBuiltinMaterialAssets();
	bool createStaticMeshAsset(const FName& assetName);
};
