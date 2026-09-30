// EngineLib/Rendering/Mesh/StaticMesh.h

#pragma once

#include <memory>

#include "Core/Object/Object.h"
#include "Core/Core.h"
#include "Core/Name.h"
#include "Rendering/VertexType.h"
#include "Material.h"

class UMaterial;

struct FStaticMeshSection
{
	FString Name;

	// UStaticMesh::GetDefaultMaterials()의 인덱스
   // -1이면 연결된 Material 슬롯이 없음
	int32 MaterialSlotIndex = -1; 

	int32 StartIndex = 0; // 해당 머티리얼이 적용되는 인덱스 버퍼 시작 위치
	int32 IndexCount = 0; // 해당 머티리얼이 적용되는 인덱스 버퍼 개수
	int32 GroupIndex = -1; // 해당 머티리얼이 속한 그룹 인덱스 (Obj 파일에서의 그룹)
};

// Coocked Data
struct FStaticMesh
{
	FName PathFileName;

	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	// 매시가 제공하는 기본 Material 슬롯
	//TArray<FMaterialSlot> MaterialSlots;

	TArray<FStaticMeshSection> Sections;
	TArray<FString> GroupNames;
};

class UStaticMesh : public UObject
{
	DECLARE_OBJECT(UStaticMesh, UObject)
	DECLARE_SERIALIZATION()
public:
	void Initialize() {};
	void Initialize(FStaticMesh* inStaticMesh);
	void Initialize(std::unique_ptr<FStaticMesh> inStaticMesh);
	void Initialize(std::unique_ptr<FStaticMesh> inStaticMesh, TArray<const UMaterial*>&& inDefaultMaterialRefs);
	const FName& GetAssetPathFileName() const;

	void SetStaticMeshAsset(FStaticMesh* inStaticMesh);

	void SetStaticMeshAsset(std::unique_ptr<FStaticMesh> inStaticMesh);

	const FStaticMesh* GetStaticMeshAsset() const;

	const UMaterial* GetDefaultMaterialOrNull(int32 slotIndex) const;

	const TArray<const UMaterial*>& GetDefaultMaterials() const;

private:
	std::unique_ptr<FStaticMesh> mStaticMeshAsset;

	TArray<const UMaterial*> mDefaultMaterialRefs;
};

