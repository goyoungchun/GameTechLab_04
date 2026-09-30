#include "StaticMesh.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UStaticMesh, UObject);
IMPLEMENT_SERIALIZATION(UStaticMesh, UObject,
	{
		// TODO: Get handle from assest manager?
		mStaticMeshAsset = nullptr;
	}
)

void UStaticMesh::Initialize(FStaticMesh* inStaticMesh)
{
	SetStaticMeshAsset(inStaticMesh);
}

void UStaticMesh::Initialize(std::unique_ptr<FStaticMesh> inStaticMesh)
{
	SetStaticMeshAsset(std::move(inStaticMesh));
}

void UStaticMesh::Initialize(std::unique_ptr<FStaticMesh> inStaticMesh, TArray<const UMaterial*>&& inDefaultMaterialRefs)
{
	SetStaticMeshAsset(std::move(inStaticMesh));
	mDefaultMaterialRefs = std::move(inDefaultMaterialRefs);
}

const FName& UStaticMesh::GetAssetPathFileName() const
{
	return mStaticMeshAsset->PathFileName;
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* inStaticMesh)
{
	mStaticMeshAsset.reset(inStaticMesh);
}

void UStaticMesh::SetStaticMeshAsset(std::unique_ptr<FStaticMesh> inStaticMesh)
{
	mStaticMeshAsset = std::move(inStaticMesh);
}

const FStaticMesh* UStaticMesh::GetStaticMeshAsset() const
{
	return mStaticMeshAsset.get();
}

const UMaterial* UStaticMesh::GetDefaultMaterialOrNull(int32 slotIndex) const
{
	if (slotIndex < 0 || slotIndex >= mDefaultMaterialRefs.Num())
	{
		return nullptr;
	}
	const UMaterial* materialAsset = mDefaultMaterialRefs[slotIndex];
	if (!materialAsset)
	{
		return nullptr;
	}
	return materialAsset;
}

const TArray<const UMaterial*>& UStaticMesh::GetDefaultMaterials() const
{
	return mDefaultMaterialRefs;
}
