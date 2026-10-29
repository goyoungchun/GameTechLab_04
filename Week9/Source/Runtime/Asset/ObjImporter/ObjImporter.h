#pragma once

#include "ObjInfo.h"
#include "Container/Array.h"
#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Render/StaticMeshData.h"

struct FObjMeshPart
{
	FString Name;
	FStaticMeshData Data;
};

class FObjImporter
{
public:
	static TUniquePtr<FStaticMeshData> LoadStaticMeshData(const FString& Path);
	// 원본 OBJ의 o 경계를 보존한다. 전체 메시용 .bin 캐시는 사용하지 않는다.
	static bool LoadStaticMeshObjects(const FString& Path, TArray<FObjMeshPart>& OutParts);

	// 디버그용 출력
	static void PrintObjInfo(const FObjInfo& ObjInfo);
	static void PrintSMD(const FStaticMeshData& SMD);

private:
	// .obj 파일 경로 -> FObjInfo 변환
	static bool ParseObj(const FString& Path, FObjInfo& Out);
	// FObjInfo -> FStaticMeshData 변환
	static bool Cook(const FObjInfo& Raw, FStaticMeshData& Out);
};
