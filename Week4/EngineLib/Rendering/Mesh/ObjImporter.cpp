#include "ObjImporter.h"
#include "Core/IO/FileManager.h"
#include "Editor/Console.h"
#include "StaticMesh.h"
#include <filesystem>
#include <fstream>
#include <istream>

namespace
{
	struct FObjVertexIndexHash
	{
		size_t operator()(const FObjVertexIndex& Key) const
		{
			size_t Hash = std::hash<int32>{}(Key.PositionIndex);

			Hash ^= std::hash<int32>{}(Key.UVIndex) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);

			Hash ^= std::hash<int32>{}(Key.NormalIndex) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);

			return Hash;
		}
	};

	bool ReadLine(std::ifstream& File, FString& OutLine)
	{
		OutLine.Reset();
		char Ch;

		while (File.get(Ch))
		{
			if (Ch == '\n')
			{
				return true;
			}

			if (Ch != '\r')
			{
				OutLine.AppendChar(Ch);
			}
		}

		// 마지막 줄에 개행이 없는 경우
		return OutLine.Len() > 0;
	}

	bool ReadToken(const char*& Cursor, FString& OutToken)
	{
		OutToken.Reset();

		while (*Cursor == ' ' || *Cursor == '\t')
		{
			++Cursor;
		}

		if (*Cursor == '\0' || *Cursor == '#')
		{
			return false;
		}

		while (*Cursor != '\0' &&
			*Cursor != ' ' &&
			*Cursor != '\t')
		{
			OutToken.AppendChar(*Cursor++);
		}

		return true;
	}


	bool IsPointInTriangle(
		const FVector& P,
		const FVector& A,
		const FVector& B,
		const FVector& C,
		const FVector& Normal)
	{
		constexpr float Epsilon = 1e-6f;

		const float AB = FVector::dot(FVector::cross(B - A, P - A), Normal);
		const float BC = FVector::dot(FVector::cross(C - B, P - B), Normal);
		const float CA = FVector::dot(FVector::cross(A - C, P - C), Normal);

		return AB >= -Epsilon &&
			BC >= -Epsilon &&
			CA >= -Epsilon;
	}

	bool TriangulateFace( const FObjFace& Face, const TArray<FVector>& Positions,
	TArray<FObjVertexIndex>& OutTriangles)
	{
		OutTriangles.Reset(0);

		if (Face.VertexIndices.Num() < 3)
		{
			return false;
		}

		// 정점 순서를 따르는 폴리곤 법선
		FVector PolygonNormal(0.0f);

		for (int32 i = 0; i < Face.VertexIndices.Num(); ++i)
		{
			const int32 Next = (i + 1) % Face.VertexIndices.Num();

			const FVector& A =
				Positions[Face.VertexIndices[i].PositionIndex];
			const FVector& B =
				Positions[Face.VertexIndices[Next].PositionIndex];

			PolygonNormal += FVector::cross(A, B);
		}

		const float NormalLength = PolygonNormal.Length();

		if (NormalLength <= SMALL_NUMBER)
		{
			return false;
		}

		PolygonNormal = PolygonNormal * (1.0f / NormalLength);

		PolygonNormal.Normalize();

		// Face.VertexIndices에서 아직 남아 있는 정점 번호
		TArray<int32> Remaining;
		Remaining.Reserve(Face.VertexIndices.Num());

		for (int32 i = 0; i < Face.VertexIndices.Num(); ++i)
		{
			Remaining.Add(i);
		}

		constexpr float Epsilon = SMALL_NUMBER;

		while (Remaining.Num() > 3)
		{
			bool FoundEar = false;

			for (int32 i = 0; i < Remaining.Num(); ++i)
			{
				const int32 PrevIndex = Remaining[(i + Remaining.Num() - 1) % Remaining.Num()];
				const int32 CurrIndex = Remaining[i];
				const int32 NextIndex = Remaining[(i + 1) % Remaining.Num()];

				const FVector& A = Positions[Face.VertexIndices[PrevIndex].PositionIndex];
				const FVector& B = Positions[Face.VertexIndices[CurrIndex].PositionIndex];
				const FVector& C = Positions[Face.VertexIndices[NextIndex].PositionIndex];

				// 오목 정점 또는 퇴화 삼각형이면 ear가 아님
				const float Orientation = FVector::dot( FVector::cross(B - A, C - A), PolygonNormal);

				if (Orientation <= Epsilon)
				{
					continue;
				}

				bool ContainsVertex = false;

				for (int32 TestIndex : Remaining)
				{
					if (TestIndex == PrevIndex ||
						TestIndex == CurrIndex ||
						TestIndex == NextIndex)
					{
						continue;
					}

					const FVector& P = Positions[Face.VertexIndices[TestIndex].PositionIndex];

					if (IsPointInTriangle(P, A, B, C, PolygonNormal))
					{
						ContainsVertex = true;
						break;
					}
				}

				if (ContainsVertex)
				{
					continue;
				}

				OutTriangles.Add(Face.VertexIndices[PrevIndex]);
				OutTriangles.Add(Face.VertexIndices[CurrIndex]);
				OutTriangles.Add(Face.VertexIndices[NextIndex]);

				Remaining.RemoveAt(i, 1);
				FoundEar = true;
				break;
			}

			// 자기 교차, 중복점 등 잘못된 폴리곤
			if (!FoundEar)
			{
				OutTriangles.Reset(0);
				return false;
			}
		}

		OutTriangles.Add(Face.VertexIndices[Remaining[0]]);
		OutTriangles.Add(Face.VertexIndices[Remaining[1]]);
		OutTriangles.Add(Face.VertexIndices[Remaining[2]]);

		return true;
	}
}

bool FObjImporter::ParseAndConvert(const FString& fileName, FObjImportResult& outResult)
{
	outResult.meshData.reset(0);
	outResult.materialSlots.Reset(0);
	outResult.materialLibraryPaths.Reset(0);

    FObjInfo objInfo;

	const bool Success = parseObjFile(fileName, objInfo);

	UE_LOG(Log, Render, "OBJ parsing: %s", Success ? "success" : "failed");
	if (!Success) return false;

	auto staticMesh = std::make_unique<FStaticMesh>();

	convertObjToStaticMesh(objInfo, *staticMesh);
	if (staticMesh->Vertices.IsEmpty() ||
		staticMesh->Indices.IsEmpty())
	{
		return false;
	}

	staticMesh->PathFileName = FName(fileName);
	//staticMesh->MaterialSlots = std::move(objInfo.MaterialSlots);

	outResult.meshData = std::move(staticMesh);
	outResult.materialSlots = std::move(objInfo.MaterialSlots);
	outResult.materialLibraryPaths = std::move(objInfo.MaterialLibraryPaths);


	return true;
}

bool FObjImporter::parseObjFile(const FString& fileName, FObjInfo& outObjInfo)
{

	// OBJ 파일이 Assets 폴더에 있다면 kDefaultAssetsPath
	// 실행 폴더 바로 아래에 있다면 kDefaultRootPath
	std::filesystem::path objFilePath = std::filesystem::path{kDefaultRootPath} / fileName.CStr();
	std::ifstream fileIn(objFilePath.c_str());    //Open file

	if (!fileIn.is_open())
	{
		return false;
	}

	FString line;
	int32 currentGroupIndex = -1;
	int32 currentMaterialIndex = -1;

	// 그룹 또는 재질이 바뀌면, 다음 면부터 새 구간을 생성.
	bool startNewFaceGroup = true;

	uint32 faceCount = 0;

	while (ReadLine(fileIn, line))
	{
		const char* Cursor = line.CStr();
		FString Prefix;

		if(!ReadToken(Cursor, Prefix)){ continue; }

		if (Prefix == "v")
		{
			FString X, Y, Z;
			if(!ReadToken(Cursor, X) || !ReadToken(Cursor, Y) || !ReadToken(Cursor, Z))
			{
				continue;
			}
			//UE_LOG(Log, Render, "v %f %f %f", X.ToFloat(), Y.ToFloat(), Z.ToFloat());
			outObjInfo.Positions.Add((FVector(X.ToFloat(), Y.ToFloat(), Z.ToFloat())));
		}
		else if (Prefix == "vt")
		{
			FString U, V;
			if(!ReadToken(Cursor, U) || !ReadToken(Cursor, V))
			{
				continue;
			}
			//UE_LOG(Log, Render, "vt %f %f", U.ToFloat(), V.ToFloat());

			const FVector2 objUV(
				U.ToFloat(),
				V.ToFloat());

			outObjInfo.UVs.Add(
				UVToUEBasis(objUV));
			//outObjInfo.UVs.Add(FVector2(U.ToFloat(), V.ToFloat()));
		}
		else if (Prefix == "vn")
		{
			FString X, Y, Z;
			if(!ReadToken(Cursor, X) || !ReadToken(Cursor, Y) || !ReadToken(Cursor, Z))
			{
				continue;
			}
			//UE_LOG(Log, Render, "vn %f %f %f", X.ToFloat(), Y.ToFloat(), Z.ToFloat());
			outObjInfo.Normals.Add(FVector(X.ToFloat(), Y.ToFloat(), Z.ToFloat()));
		}
		else if (Prefix == "mtllib")
		{
			FString MtlFileName;

			while (ReadToken(Cursor, MtlFileName))
			{
				//UE_LOG(Log, Render, MtlFileName.CStr());

				const std::filesystem::path relativePath = std::filesystem::path(MtlFileName.CStr());
				const FString normalizedPath(relativePath.string().c_str());
				outObjInfo.MaterialLibraryPaths.Add(normalizedPath);


				// MTL 파일 경로를 OBJ 파일 경로와 동일한 디렉토리에 있다고 가정하고 Path 등록
				const auto mtlFilePath = objFilePath.parent_path() / MtlFileName.CStr();
				parseMtlFile(mtlFilePath, outObjInfo.MaterialSlots);
			}
		}
		else if (Prefix == "g" || Prefix == "o")
		{
			FString groupName;
			int32 groupIndex = -1;

			if (ReadToken(Cursor, groupName))
			{
				// 같은 이름이 다시 나오면 기존 인덱스를 재사용.
				for (int32 i = 0; i < outObjInfo.GroupNames.Num(); ++i)
				{
					if (outObjInfo.GroupNames[i] == groupName)
					{
						groupIndex = i;
						break;
					}
				}

				if (groupIndex == -1)
				{
					groupIndex =
						static_cast<int32>(outObjInfo.GroupNames.Add(groupName));
				}
			}

			if (currentGroupIndex != groupIndex)
			{
				currentGroupIndex = groupIndex;
				startNewFaceGroup = true;
			}
		}
		else if (Prefix == "usemtl")
		{
			FString CurrentMaterialNameStr;
			if(!ReadToken(Cursor, CurrentMaterialNameStr))
			{
				continue;
			}

			auto It = std::find_if(outObjInfo.MaterialSlots.begin(), outObjInfo.MaterialSlots.end(),
				[&](const FMaterialSlot& Slot) {
					return Slot.Name == CurrentMaterialNameStr;
				}
			);

			int32 MatIndex = -1;
			if (It != outObjInfo.MaterialSlots.end())
			{
				MatIndex = std::distance(outObjInfo.MaterialSlots.begin(), It);
			}

			UE_LOG(Log, Render, CurrentMaterialNameStr.CStr());

			//인덱스 찾음 
			if (currentMaterialIndex != MatIndex)
			{
				currentMaterialIndex = MatIndex;
				startNewFaceGroup = true;
			}
		}
		else if (Prefix == "f")
		{
			FObjFace newFace;
			FString faceData;
			while (ReadToken(Cursor, faceData))
			{
				const FString Slash("/");
				int32 Pos = faceData.Find(Slash);
				int32 Normal = (Pos == -1) ? -1 : faceData.Find(Slash, Pos + 1);

				FObjVertexIndex VertexIndex;

				const FString positionStr = (Pos == -1) ? faceData : faceData.Left(Pos);

				// 음수 인덱스 지원
				VertexIndex.PositionIndex = positionStr.ToInt() > 0 ? positionStr.ToInt() - 1
					: static_cast<int32>(outObjInfo.Positions.Num()) + positionStr.ToInt();

				// v/vt
				if (Pos != -1 && Normal == -1 )
				{
					VertexIndex.UVIndex = faceData.RightChop(Pos + 1).ToInt() > 0 ? faceData.RightChop(Pos + 1).ToInt() - 1
						: static_cast<int32>(outObjInfo.UVs.Num()) + faceData.RightChop(Pos + 1).ToInt();
				}
				else if (Normal != -1)
				{
					// v/vt/vn: 두 '/' 사이에 UV가 있는 경우
					// v//vn이면 건너뛰고 UVIndex는 -1 유지
					if (Normal > Pos + 1)
					{
						VertexIndex.UVIndex = faceData.Mid(Pos + 1, Normal - Pos - 1).ToInt() > 0 ? faceData.Mid(Pos + 1, Normal - Pos - 1).ToInt() - 1
							: static_cast<int32>(outObjInfo.UVs.Num()) + faceData.Mid(Pos + 1, Normal - Pos - 1).ToInt();
					}

					// v/vt/vn 또는 v//vn의 노멀
					VertexIndex.NormalIndex = faceData.RightChop(Normal + 1).ToInt() > 0 ? faceData.RightChop(Normal + 1).ToInt() - 1
						: static_cast<int32>(outObjInfo.Normals.Num()) + faceData.RightChop(Normal + 1).ToInt();
				}

				newFace.VertexIndices.Add(VertexIndex);
			}

			//테스트 코드 시작
			/*UE_LOG(Log, Render, "Face %u: %d vertices",faceCount,static_cast<int>(newFace.VertexIndices.Num()));

			for (const FObjVertexIndex& Vertex : newFace.VertexIndices)
			{
				UE_LOG(Log, Render, "Position=%d UV=%d Normal=%d",
					Vertex.PositionIndex,
					Vertex.UVIndex,
					Vertex.NormalIndex);
			}*/
			//테스트 코드 끝

			if (newFace.VertexIndices.Num() < 3)
			{
				continue;
			}

			// usemtl 없이 시작하는 면은 기본 재질 그룹에 저장
			if (outObjInfo.FaceGroups.Num() == 0 || startNewFaceGroup)
			{
				FObjFaceGroup newGroup;
				newGroup.GroupIndex = currentGroupIndex;
				newGroup.MaterialSlotIndex = currentMaterialIndex;
				newGroup.FirstFaceIndex = faceCount;

				outObjInfo.FaceGroups.Add(newGroup);
				startNewFaceGroup = false;
			}

			// 현재 그룹의 FaceCount 증가
			outObjInfo.FaceGroups[outObjInfo.FaceGroups.Num() - 1].FaceCount++;

			outObjInfo.Faces.Add(newFace);

			// 전체 면 수 증가
			faceCount++;
		}
	}

	return true;
}

bool FObjImporter::parseMtlFile(const std::filesystem::path& filePath, TArray<FMaterialSlot>& outMaterialSlots)
{
	std::ifstream fileIn(filePath.c_str());
	FString line;

	if (!fileIn.is_open())
	{
		return false;
	}

	while (ReadLine(fileIn, line))
	{
		const char* Cursor = line.CStr();
		FString Prefix;

		if (!ReadToken(Cursor, Prefix)) { continue; }

		if (Prefix == "newmtl")
		{
			FString MaterialName;
			if (!ReadToken(Cursor, MaterialName)) { continue; }

			FMaterialSlot slot;
			slot.Name = MaterialName;

			UE_LOG(Log, Render, slot.Name.CStr());

			outMaterialSlots.Add(slot);
		}

		if (outMaterialSlots.IsEmpty())
		{
			continue;
		}

		FMaterial& currentMaterial =
			outMaterialSlots[outMaterialSlots.Num() - 1].DefaultMaterial;

		if (Prefix == "Ka" || Prefix == "Kd" || Prefix == "Ks")
		{
			FString R, G, B;
			if (!ReadToken(Cursor, R) || !ReadToken(Cursor, G) || !ReadToken(Cursor, B)) { continue; }
			FVector Color(R.ToFloat(), G.ToFloat(), B.ToFloat());
			if (outMaterialSlots.Num() > 0)
			{
				if (Prefix == "Ka")
				{
					currentMaterial.AmbientColor = Color;
				}
				else if (Prefix == "Kd")
				{
					currentMaterial.DiffuseColor = Color;
				}
				else if (Prefix == "Ks")
				{
					currentMaterial.SpecularColor = Color;
				}
			}
			UE_LOG(Log, Render, "%s %f %f %f", Prefix.CStr(), Color.x, Color.y, Color.z);
		}
		else if (Prefix == "Ns")
		{
			FString SpecularExponentStr;
			if (!ReadToken(Cursor, SpecularExponentStr)) { continue; }
			currentMaterial.SpecularExponent = SpecularExponentStr.ToFloat();
			UE_LOG(Log, Render, "Ns %f", SpecularExponentStr.ToFloat());
		}
		else if (Prefix == "d" || Prefix == "Tr")
		{
			FString AlphaStr;
			if (!ReadToken(Cursor, AlphaStr)) { continue; }

			float AlphaValue = AlphaStr.ToFloat();
			if (Prefix == "Tr") // Tr은 투명도이므로 반전
			{
				AlphaValue = 1.0f - AlphaValue;
			}
			currentMaterial.Opacity = AlphaValue;
			

			UE_LOG(Log, Render, "d/Tr %f", AlphaStr.ToFloat());
		}
		else if (Prefix == "map_Kd")
		{
			FString DiffuseTexturePath;
			if (!ReadToken(Cursor, DiffuseTexturePath)) { continue; }

			//실제 경로 저장
			const std::filesystem::path texturePath = std::filesystem::absolute(filePath.parent_path() / DiffuseTexturePath.CStr()).lexically_normal();
			currentMaterial.DiffuseTexture =  FString(texturePath.string().c_str());

			UE_LOG(Log, Render, "map_Kd %s", DiffuseTexturePath.CStr());
		}
		else if (Prefix == "map_bump" || Prefix == "bump" || Prefix == "norm")
		{
			FString NormalTexturePath;
			if (!ReadToken(Cursor, NormalTexturePath)) { continue; }

			//실제 경로 저장
			const std::filesystem::path texturePath = std::filesystem::absolute(filePath.parent_path() / NormalTexturePath.CStr()).lexically_normal();
			currentMaterial.NormalTexture = FString(texturePath.string().c_str());
			
			UE_LOG(Log, Render, "map_bump %s", NormalTexturePath.CStr());
		}
		else if (Prefix == "map_Ks")
		{
			FString SpecularTexturePath;
			if (!ReadToken(Cursor, SpecularTexturePath)) { continue; }

			//실제 경로 저장
			const std::filesystem::path texturePath = std::filesystem::absolute(filePath.parent_path() / SpecularTexturePath.CStr()).lexically_normal();
			currentMaterial.SpecularTexture = FString(texturePath.string().c_str());
			
			UE_LOG(Log, Render, "map_Ks %s", SpecularTexturePath.CStr());
		}
	}

	return true;
}

void FObjImporter::convertObjToStaticMesh(const FObjInfo& objInfo, FStaticMesh& outStaticMesh)
{
	outStaticMesh.Vertices.Reset(0);
	outStaticMesh.Indices.Reset(0);
	outStaticMesh.Sections.Reset(0);

	// 일단은 std map 으로 박아놓기
	std::unordered_map<FObjVertexIndex, uint32, FObjVertexIndexHash> vertexMap;
	outStaticMesh.GroupNames = objInfo.GroupNames;

	for (const FObjFaceGroup& group : objInfo.FaceGroups)
	{
		FStaticMeshSection section;
		section.MaterialSlotIndex = group.MaterialSlotIndex;
		section.GroupIndex = group.GroupIndex;
		section.StartIndex = static_cast<uint32>(outStaticMesh.Indices.Num());
		section.IndexCount = 0;

		for (uint32 FaceOffset = 0; FaceOffset < group.FaceCount; ++FaceOffset)
		{
			const FObjFace& face = objInfo.Faces[group.FirstFaceIndex + FaceOffset];

			TArray<FObjVertexIndex> Triangles;

			if (!TriangulateFace(face, objInfo.Positions, Triangles))
			{
				UE_LOG(Warning, Render, "Failed to triangulate OBJ face");
				continue;
			}

			// Ear clipping 방식으로 삼각형을 생성하고, 각 삼각형의 정점 정보를 FStaticMesh에 추가
			for (int32 i = 0; i + 2 < Triangles.Num(); i += 3)
			{
				const FObjVertexIndex vertices[3] =
				{
					Triangles[i],
					Triangles[i+1],
					Triangles[i + 2]
				};

				// 원본 노말이 없을 경우 면의 노말을 계산하여 사용
				const FVector& A = objInfo.Positions[vertices[0].PositionIndex];
				const FVector& B = objInfo.Positions[vertices[1].PositionIndex];
				const FVector& C = objInfo.Positions[vertices[2].PositionIndex];
				FVector FaceNormal = FVector::cross(B - A, C - A);

				FaceNormal.Normalize();

				for (const FObjVertexIndex& vertex : vertices)
				{
					const bool HasNormal = vertex.NormalIndex >= 0;

					// 원본 노멀이 있으면 기존 정점을 재사용
					if (HasNormal)
					{
						auto It = vertexMap.find(vertex);

						if (It != vertexMap.end())
						{
							outStaticMesh.Indices.Add(It->second);
							continue;
						}
					}

					const uint32 NewIndex = static_cast<uint32>( outStaticMesh.Vertices.Num());

					outStaticMesh.Vertices.Add(FNormalVertex{
						objInfo.Positions[vertex.PositionIndex],

						HasNormal ? objInfo.Normals[vertex.NormalIndex]
							: FaceNormal,

						FLinearColor(1.f, 1.f, 1.f, 1.f),

						vertex.UVIndex >= 0
							? objInfo.UVs[vertex.UVIndex]
							: FVector2(0.f, 0.f)
						});

					outStaticMesh.Indices.Add(NewIndex);

					// 노멀 없는 점은 면별 노멀을 유지하도록 공유 안 함
					if (HasNormal)
					{
						vertexMap.emplace(vertex, NewIndex);
					}
				}
			}
		}

		// 섹션의 인덱스 개수 계산
		// 섹션의 인덱스 개수는 현재 인덱스 배열의 크기에서 섹션 시작 인덱스를 뺀 값
		section.IndexCount = static_cast<uint32>(outStaticMesh.Indices.Num()) - section.StartIndex;

		if (section.IndexCount > 0)
		{
			outStaticMesh.Sections.Add(section);
		}
	}
}


