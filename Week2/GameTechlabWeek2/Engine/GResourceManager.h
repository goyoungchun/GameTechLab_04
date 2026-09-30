#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma once
#include <unordered_map>
#include <map>
#include <string>
#include "GDevice.h"
#include "Core.h"
#include "Container/FString.h"
#include "Container/Tarray.h"
#include "Engine/Renderer/FVertexSimple.h"
#include "Engine/Primitive/FMeshResource.h"

//struct FMeshResource
//{
//	ID3D11Buffer* VertexBuffer = nullptr;
//	ID3D11Buffer* IndexBuffer = nullptr;
//	UINT VertexCount = 0;
//	UINT IndexCount = 0;
//	UINT Stride = 0;
//
//	TArray<FVertexSimple> vertexs;
//	TArray<uint32> indexes;
//};

struct FShaderResource
{
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
};

//enum class EPrimitiveType
//{
//	Sphere, Cube, Cylinder, Cone, Plane
//};

class GResourceManager
{
public:
	static GResourceManager* GetInstance();

	void Initialize(GDevice* InDevice);
	void Shutdown();
	FMeshResource* CreateMesh(const FString& MeshName,const TArray<FVertexSimple>& Vertices, const TArray<uint32>& Indices);

	template <size_t VCount, size_t ICount>
	FMeshResource* CreateMesh(
		const std::string& MeshName,
		const FVertexSimple(&Vertices)[VCount],
		const uint32_t(&Indices)[ICount])
	{
		if (PrimitiveCache.find(MeshName) != PrimitiveCache.end())
		{
			return PrimitiveCache[MeshName];
		}

		FMeshResource* MeshResource = new FMeshResource;
		UINT VertexCount = VCount;
		UINT IndexCount = ICount;

		for (UINT i = 0; i < VertexCount; ++i)
		{
			MeshResource->vertexs.Add(Vertices[i]);
		}
		for (UINT i = 0; i < IndexCount; ++i)
		{
			MeshResource->indexes.Add(Indices[i]);
		}

		MeshResource->VertexBuffer = Device->CreateVertexBuffer(&MeshResource->vertexs[0], sizeof(FVertexSimple) * VertexCount);

		MeshResource->IndexBuffer = Device->CreateIndexBuffer(&MeshResource->indexes[0], sizeof(uint32_t) * IndexCount);

		MeshResource->VertexCount = VertexCount;
		MeshResource->IndexCount = IndexCount;
		MeshResource->Stride = sizeof(FVertexSimple);
		MeshResource->bHasBounds = false;
		if (MeshResource->vertexs.Num() > 0)
		{
			const auto& First = MeshResource->vertexs[0];

			MeshResource->BoundsMin = FVector(First.x, First.y, First.z);
			MeshResource->BoundsMax = MeshResource->BoundsMin;

			for (const auto& Vertex : MeshResource->vertexs)
			{
				MeshResource->BoundsMin.X = (std::min)(MeshResource->BoundsMin.X, Vertex.x);
				MeshResource->BoundsMin.Y = (std::min)(MeshResource->BoundsMin.Y, Vertex.y);
				MeshResource->BoundsMin.Z = (std::min)(MeshResource->BoundsMin.Z, Vertex.z);

				MeshResource->BoundsMax.X = (std::max)(MeshResource->BoundsMax.X, Vertex.x);
				MeshResource->BoundsMax.Y = (std::max)(MeshResource->BoundsMax.Y, Vertex.y);
				MeshResource->BoundsMax.Z = (std::max)(MeshResource->BoundsMax.Z, Vertex.z);
			}

			MeshResource->bHasBounds = true;
		}
		PrimitiveCache[MeshName] = MeshResource;

		return MeshResource;
	};

	FMeshResource* GetPrimitive(const FString& Type);

	FShaderResource* GetShader(
		const std::wstring& FilePath,
		const std::string& VSEntry,
		const std::string& PSEntry,
		const D3D11_INPUT_ELEMENT_DESC* Layout,
		UINT LayoutCount
	);

private:
	GResourceManager() = default;
	~GResourceManager() = default;
	GResourceManager(const GResourceManager&) = delete;
	GResourceManager& operator=(const GResourceManager&) = delete;

	GDevice* Device = nullptr;

	std::unordered_map<std::string, FMeshResource*> PrimitiveCache;
	//std::unordered_map<std::string, FShaderResource*> ShaderCache;	// 일단 Renderer에서 - 셰이더 무조건 하나만 쓰니까..
	//std::map<std::pair<D3D11_FILL_MODE, D3D11_CULL_MODE>, ID3D11RasterizerState*> RasterizerStateCache;
};

