#pragma once

#include "MeshComponent.h"

#include "Rendering/Mesh/StaticMesh.h"
#include "Rendering/SubUVMesh.h"

struct FMaterialOverride
{
	bool bIsSet = false;
	const UMaterial* OverridedMaterialRef = nullptr;
};

class UStaticMeshComponent : public UMeshComponent
{
	DECLARE_OBJECT(UStaticMeshComponent, UMeshComponent)
	DECLARE_SERIALIZATION()

	void Initialize(
		FVector location,
		FRotator rotation,
		FVector scale3D,
		const UStaticMesh* staticMeshOrNull,
		bool bUseTexture = false
	);

	static std::span<const FPropertyInfo> GetDeclaredProperties();

	void SetStaticMesh(const UStaticMesh& staticMeshRef);
	const FName& GetStaticMeshAssetKey() const { return mStaticMeshAssetKey; }
	const FName& GetMaterialAssetKey(int32 slotIndex) const;
	const TArray<FName>& GetMaterialAssetKeys() const { return mMaterialAssetKeys; }
	uint32 GetMaterialSlotCount() const { return mStaticMeshRef ? mStaticMeshRef->GetDefaultMaterials().Num() : 0; }

	const UStaticMesh* GetStaticMeshAsset() const;

	const UMaterial* GetMaterialAsset(int32 slotIndex) const;
	bool SetMaterial(int32 slotIndex, const UMaterial& materialAsset);
	bool ClearMaterialOverride(int32 slotIndex);

	/* SubUV */
	const FSubUVMesh& GetSubUVMesh() const { return mSubUVMesh; }
	void SetSubUVMesh(const FSubUVMesh& subUVMesh) { mSubUVMesh = subUVMesh; }
	void SetSubUVMesh(FVector2 uvOffset, FVector2 uvScale) { mSubUVMesh.UVOffset = uvOffset; mSubUVMesh.UVScale = uvScale; }


protected:
	virtual FRenderInfo makeRenderInfo() const override;
	FSubUVMesh mSubUVMesh = {};

private:
	void resetMaterialOverrides();

private:
	const UStaticMesh* mStaticMeshRef = nullptr;
	FName mStaticMeshAssetKey; // Temporary, for serialization and editor purposes

	// 인덱스는 FStaticMesh::MaterialSlots 인덱스와 동일하다.
	TArray<FMaterialOverride> mMaterialOverrides;
	TArray<FName> mMaterialAssetKeys; // Temporary, for serialization and editor purposes

};
