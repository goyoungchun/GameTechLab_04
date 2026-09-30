#pragma once

#include "Core/Core.h"
#include "Core/Math/Vector.h"
#include "Core/Container/TArray.h"
#include <filesystem>
#include "Rendering/Mesh/StaticMesh.h"
#include "Material.h"

struct FNormalVertex;
struct FStaticMesh;

struct FObjImportResult
{
	std::unique_ptr<FStaticMesh> meshData;

	TArray<FMaterialSlot> materialSlots;

	// OBJ의 mtllib에 실제로 기록된 경로
	// 얘는 메시 직렬화 대상 아님
	TArray<FString> materialLibraryPaths;
};

// f v1/vt1/vn1
// FObjVertexIndex는 하나의 정점에 대한 위치, UV, 노멀들의 인덱스를 저장함.
struct FObjVertexIndex
{
	int32 PositionIndex = -1;
	int32 UVIndex = -1;
	int32 NormalIndex = -1;

	bool operator==(const FObjVertexIndex&) const = default;
};

/// FObjFace는 하나의 면을 나타냄 (n각형 : 정점 n개)
struct FObjFace
{
	TArray<FObjVertexIndex> VertexIndices; // Face의 각 Vertex에 대한 Position, UV, Normal Index

	int32 smoothingGroup = 0;
};

struct FObjFaceGroup
{
	int32 MaterialSlotIndex = -1;
	int32 GroupIndex = -1;

	uint32 FirstFaceIndex = 0;
	uint32 FaceCount = 0;
};

struct FObjInfo
{
	TArray<FVector> Positions;
	TArray<FVector2> UVs;
	TArray<FVector> Normals;

	TArray<FObjFace> Faces;
	TArray<FMaterialSlot> MaterialSlots;
	TArray<FString> GroupNames;
	TArray<FObjFaceGroup> FaceGroups;

	TArray<FString> MaterialLibraryPaths;
};


class FObjImporter
{
public:

	//obj 파일을 읽어서 FStaticMesh로 변환하는 함수
	static bool ParseAndConvert(const FString& fileName, FObjImportResult& outResult);

private:

	// Obj File을 파싱해서 FObjInfo
	static bool parseObjFile(const FString& fileName, FObjInfo& outObjInfo);

	// Parsing Mtl File 에서 FObjMaterialInfo
	static bool parseMtlFile(const std::filesystem::path& filePath, TArray<FMaterialSlot>& outMaterials);

	//FObjInfo 에서 FStaticMesh로 변환
	static void convertObjToStaticMesh(const FObjInfo& objInfo, FStaticMesh& outStaticMesh);

	static FVector positionToUEBasis(const FVector& inVector)
	{
		return FVector(inVector.x, -inVector.y, inVector.z);
	}

	static FVector2 UVToUEBasis(const FVector2& inVector)
	{
		return FVector2(inVector.x, 1.0f - inVector.y);
	}
private:
	FObjInfo mObjInfo;
};
