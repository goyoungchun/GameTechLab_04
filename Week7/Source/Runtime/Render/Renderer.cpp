#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include "Engine/PrimitiveSceneProxy.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"

#include "Camera/CameraComponent.h"

#include "Asset/AssetManager.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <bit>

DECLARE_CYCLE_STAT("Draw Render Packets", STAT_DrawRenderPackets);
DECLARE_CYCLE_STAT("Render Queue Sorting", STAT_RenderQueueSorting);
DECLARE_CYCLE_STAT("Upload Per-Object CB", STAT_UploadPerObjectCB);

namespace
{
	// UberLit.hlsl b6. 반투명/컬링을 생략한 뷰는 전체 라이트를 사용한다.
	struct alignas(16) FTileLightInfo
	{
		uint32 TileCountX;
		uint32 bUseTileCulling;
		uint32 Padding[2]{};
	};
	struct alignas(16) FCullingViewConstants
	{
		uint32 TileCountX;
		uint32 TileSize;
		uint32 MaxLightsPerTile;
		uint32 bValid;
		float ViewportOrigin[2];
		float Padding[2]{};
	};
	static_assert(sizeof(FCullingViewConstants) == 32);
	// VSSetConstantBuffers1의 오프셋은 16개 상수(256바이트) 단위여야 하므로 오브젝트마다 256바이트 칸을 쓴다.
	constexpr uint32 PerObjectSlotConstants = 16;
	static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

	constexpr uint32 MinPerObjectSlots = 1024;

	const FMatrix& GetPacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	uint64 MakeSortKey(const FRenderPacket& Packet)
	{
		if (Packet.Material->BlendState != EBlendState::Opaque)
		{
			const uint32 DistanceBits = std::bit_cast<uint32>(Packet.CameraToParticleDistance);
			return (1ull << 63) | static_cast<uint64>(~DistanceBits);
		}

		return (static_cast<uint64>(Packet.Material->SortID) << 47)
			| (static_cast<uint64>(Packet.Mesh->SortID) << 31)
			| (static_cast<uint64>(Packet.LODIndex & 0x3) << 29);
	}

	// 불투명 패킷과 같은 규칙의 키 (묶음은 전부 불투명)
	uint64 MakeGroupKey(const FStaticDrawGroup& Group)
	{
		return (static_cast<uint64>(Group.Material->SortID) << 47)
			| (static_cast<uint64>(Group.Mesh->SortID) << 31)
			| (static_cast<uint64>(Group.LODIndex & 0x3) << 29);
	}
}

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	DirectionalLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDirectionalLightConstants));

	// 라이트가 0개여도 t1/t2에 유효한 SRV가 묶이도록 처음부터 만들어 둔다
	LightCB = RenderCommand::CreateConstantBuffer(sizeof(FLightConstants));
	CameraCB = RenderCommand::CreateConstantBuffer(sizeof(FCameraConstants));
	TileCullingCB = RenderCommand::CreateConstantBuffer(sizeof(FTileCullingConstants));
	TileLightInfoCB = RenderCommand::CreateConstantBuffer(sizeof(FTileLightInfo));
	CullingViewCB = RenderCommand::CreateConstantBuffer(sizeof(FCullingViewConstants));

	EnsureLightBufferCapacity(PointLightBuffer, 1, sizeof(FPointLightInfo));
	EnsureLightBufferCapacity(SpotLightBuffer, 1, sizeof(FSpotLightInfo));
	GPUOcclusion.Init();   // 실패해도 오클루전만 못 쓸 뿐 렌더링은 된다

	return true;
}

// 필요한 칸 수가 용량을 넘을 때만 두 배씩 키워 재할당을 드물게 한다.
void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity)
		return;

	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount)
		NewCapacity *= 2;

	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer ({} slots) creation failed. Falling back to per-draw updates.", NewCapacity);
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

// 원래 패킷 순서로 World 행렬을 올린다. 정렬 목록은 원래 패킷 번호의 칸을 바인딩한다.
void FRenderer::UploadPerObjectConstants()
{
	SCOPE_CYCLE_COUNTER(STAT_UploadPerObjectCB);

	const uint32 Count = static_cast<uint32>(RenderPackets.size());
	if (!bUsePerObjectSlots || Count == 0)
		return;

	EnsurePerObjectSlotCapacity(Count);
	if (!PerObjectSlotCB)
	{
		bUsePerObjectSlots = false;
		return;
	}

	uint8* Dest = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Dest)
	{
		bUsePerObjectSlots = false;
		return;
	}

	// 매핑된 메모리는 write-combined라 순차 쓰기만 하고 읽지 않는다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		FRenderPacket& P = RenderPackets[Index];
		const FMatrix& Model = GetPacketWorld(P);
		std::memcpy(Dest + static_cast<size_t>(Index) * ObjectSlotBytes, &Model, sizeof(FMatrix));
		P.Slot = Index;
	}

	RenderCommand::Unmap(PerObjectSlotCB.get());
}

// 카메라의 ViewProjection을 공통 렌더 경로로 전달한다.
void FRenderer::RenderAll(FRenderQueue& InQueue, UCameraComponent* CameraComponent)
{
	RenderAll(InQueue, CameraComponent->GetViewProjectionMatrix());
}

// 불투명 우선·반투명 거리순으로 정렬해 View 행렬과 Section 범위로 그린다.
void FRenderer::RenderAll(FRenderQueue& InQueue, const FMatrix& ViewProjection)
{
	RenderQueueSorting(InQueue, ViewProjection);
	RenderOpaque(ViewProjection);
	RenderTranslucent(ViewProjection);
}

void FRenderer::RenderOpaque(const FMatrix& ViewProjection)
{
	//Light Bind
	BindLights(true);

	//

	//이건 사실상 안쓰는 중인듯
	DrawStaticGroups();


	DrawPackets(0, FirstTranslucentIndex, ViewProjection);
}

// RenderOpaque가 남긴 반투명 패킷을 먼 것부터 그린다.
void FRenderer::RenderTranslucent(const FMatrix& ViewProjection)
{
	BindLights();

	DrawPackets(FirstTranslucentIndex, SortEntries.Num(), ViewProjection);
	RenderPackets.Reset();
	FirstTranslucentIndex = 0;
	StaticGroups.clear();   // 묶음 메모리는 World 것이므로 이번 프레임이 끝나면 놓는다
}

// 스태틱 메시 묶음을 정렬 키 순서로 그린다. 바인딩은 묶음마다 한 번, 항목마다는 칸 바인딩과 드로우만 한다.
void FRenderer::DrawStaticGroups(bool bDepthOnly)
{
	if (StaticGroups.empty())
		return;

	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);

	UMaterial* BoundMaterial = nullptr;
	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		RenderCommand::BindMesh(Group->Mesh, Group->LODIndex);
		if (Group->Material != BoundMaterial)
		{
			BoundMaterial = Group->Material;
			BindMaterial(BoundMaterial, bDepthOnly);
			FRenderPacket MaterialOnly;           // 머티리얼 파라미터 갱신은 패킷을 받으므로 머티리얼만 채워 넘긴다
			MaterialOnly.Material = BoundMaterial;
			UpdateMaterialParams(MaterialOnly);
		}

		for (const FStaticDrawItem& Item : Group->Items)
		{
			if (bUsePerObjectSlots && Item.Slot != InvalidObjectSlot)
			{
				RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), Item.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
			}
			else
			{
				RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
				UpdatePerObjectConstants(Item.Proxy->GetLocalToWorld());
			}
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}

	// 뒤따르는 DrawPackets가 처음부터 다시 바인딩하도록 기록을 비운다.
	LastMesh = nullptr;
	LastMaterial = nullptr;
}

void FRenderer::RenderQueueSorting(FRenderQueue& InQueue, const FMatrix& ViewProjection)
{
	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewProjection);
	{
		SCOPE_CYCLE_COUNTER(STAT_RenderQueueSorting);

		std::swap(RenderPackets, InQueue);
		InQueue.Reset();

		// ① 패킷을 한 번 훑으며 키를 만든다. 그릴 수 없는 패킷은 목록에 넣지 않는다.
		SortEntries.Reset();
		SortEntries.Reserve(RenderPackets.Num());
		for (uint32 i = 0; i < RenderPackets.Num(); ++i)
		{
			const FRenderPacket& P = RenderPackets[i];
			if (!P.Mesh || !P.Material) continue;
			SortEntries.Add({ MakeSortKey(P), i });
		}

		// ② 16B 항목만 정렬
		std::sort(SortEntries.begin(), SortEntries.end(),
			[](const FSortEntry& A, const FSortEntry& B) { return A.Key < B.Key; });

		// ③ 반투명 시작 위치 = 최상위 비트가 처음 1인 곳
		FirstTranslucentIndex = 0;
		while (FirstTranslucentIndex < SortEntries.Num() && !(SortEntries[FirstTranslucentIndex].Key >> 63))
			++FirstTranslucentIndex;

		// ④ 스태틱 메시 묶음: 빈 묶음을 빼고 키 순으로 정렬한다. 조각마다 같은 키 묶음이 있으므로 정렬하면 서로 붙는다.
		std::erase_if(StaticGroups, [](const FStaticDrawGroup* Group) { return Group->Items.empty(); });
		std::sort(StaticGroups.begin(), StaticGroups.end(),
			[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return MakeGroupKey(*A) < MakeGroupKey(*B); });
	}
	// Gather uploads group and packet slots together. Other queues need an upload here.
	if (!bObjectConstantsPrepared)
		UploadPerObjectConstants();
	bObjectConstantsPrepared = false;
}

// 정렬된 패킷 중 [Begin, End) 범위를 View 행렬과 Section 범위로 그린다.
void FRenderer::DrawPackets(uint32 Begin, uint32 End, const FMatrix& ViewProjection, bool bDepthOnly)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);

	LastMesh = nullptr;
	LastMaterial = nullptr;
	uint8 LastLODIndex = 0;

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);


	for (uint32 k = Begin; k < End; ++k)          // k = 정렬된 위치
	{
		const uint32 PacketIndex = SortEntries[k].PacketIndex;
		const FRenderPacket& RenderPacket = RenderPackets[PacketIndex];
		//if (RenderPacket.Mesh == nullptr || RenderPacket.Material == nullptr) continue;
		if (RenderPacket.Mesh != LastMesh || RenderPacket.LODIndex != LastLODIndex) {
			RenderCommand::BindMesh(RenderPacket.Mesh, RenderPacket.LODIndex);
		}
		if (RenderPacket.Material != LastMaterial) {
			BindMaterial(RenderPacket.Material, bDepthOnly);
		}
		if (RenderPacket.Material != LastMaterial || RenderPacket.MaterialParamData)
			UpdateMaterialParams(RenderPacket);
		if (bUsePerObjectSlots && RenderPacket.Slot != InvalidObjectSlot)
		{
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), RenderPacket.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		}
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
			UpdatePerObjectConstants(RenderPacket, ViewProjection);
		}

		RenderCommand::DrawIndexed(
			RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.Mesh->GetIndexBuffer(RenderPacket.LODIndex)->GetIndexCount(),
			RenderPacket.StartIndex
		);
		LastMaterial = RenderPacket.Material;
		LastMesh = RenderPacket.Mesh;
		LastLODIndex = RenderPacket.LODIndex;
	}
}

FOcclusionMeasureResult FRenderer::MeasureOpaqueOcclusion(const FMatrix& ViewProjection)
{
	FOcclusionMeasureResult Result;

	// 불투명 드로우를 두 경로(스태틱 묶음, 일반 패킷)에서 한 목록으로 모은다. 그린 순서와 같게 묶음 먼저.
	struct FMeasureDraw
	{
		UStaticMesh* Mesh;
		UMaterial* Material;
		uint8 LODIndex;
		uint32 Slot;
		uint32 StartIndex;
		uint32 IndexCount;
		const FMatrix* World;       // 칸이 없을 때만 쓴다
		const void* ObjectKey;      // 같은 물체의 섹션을 한 물체로 센다
		bool bOccludedByGpu;
		const FRenderPacket* Packet;
	};
	std::vector<FMeasureDraw> Draws;
	for (const FStaticDrawGroup* Group : StaticGroups)
		for (const FStaticDrawItem& Item : Group->Items)
			Draws.push_back({ Group->Mesh, Group->Material, Group->LODIndex, Item.Slot, Item.StartIndex, Item.IndexCount,
				&Item.Proxy->GetLocalToWorld(), Item.Proxy, Item.bOccludedByGpu != 0, nullptr });
	for (uint32 k = 0; k < FirstTranslucentIndex; ++k)
	{
		const FRenderPacket& Packet = RenderPackets[SortEntries[k].PacketIndex];
		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Packet.Mesh->GetIndexBuffer(Packet.LODIndex)->GetIndexCount();
		// 프록시가 없는 패킷(빌보드 등)은 패킷 자체를 한 물체로 센다.
		const void* Key = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);
		Draws.push_back({ Packet.Mesh, Packet.Material, Packet.LODIndex, Packet.Slot, Packet.StartIndex, IndexCount,
			&GetPacketWorld(Packet), Key, Packet.bOccludedByGpu, &Packet });
	}

	const uint32 Count = static_cast<uint32>(Draws.size());
	if (Count == 0)
		return Result;

	const auto StartTime = std::chrono::high_resolution_clock::now();

	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	// 같은 셰이더·같은 행렬로 다시 그리면 깊이가 비트 단위로 같으므로,
	// LESS_EQUAL이면 최종 깊이 버퍼에서 이 물체가 이긴 픽셀만 통과한다.
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf())))
			return Result;
	}

	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC QueryDesc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&QueryDesc, Query.GetAddressOf())))
			return Result;
		OcclusionQueries.Add(std::move(Query));
	}

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	if (!bUsePerObjectSlots)
		RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	UStaticMesh* BoundMesh = nullptr;
	UMaterial* BoundMaterial = nullptr;
	uint8 BoundLOD = 0;

	// 1) 드로우마다 쿼리를 걸고 다시 그린다. 본 패스와 같은 바인딩(칸 또는 PerObjectCB)을 쓴다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		if (Draw.Mesh != BoundMesh || Draw.LODIndex != BoundLOD)
			RenderCommand::BindMesh(Draw.Mesh, Draw.LODIndex);
		if (Draw.Material != BoundMaterial)
		{
			BindMaterial(Draw.Material);
			// BindMaterial이 바꾼 상태를 측정용으로 덮어쓴다.
			RenderCommand::SetBlendState(EBlendState::NoColorWrite);
			Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		}
		if (Draw.Packet && (Draw.Material != BoundMaterial || Draw.Packet->MaterialParamData))
			UpdateMaterialParams(*Draw.Packet);

		if (bUsePerObjectSlots && Draw.Slot != InvalidObjectSlot)
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(),
				Draw.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
			UpdatePerObjectConstants(*Draw.World);
		}

		ID3D11Query* Query = OcclusionQueries[Index].Get();
		Context->Begin(Query);
		RenderCommand::DrawIndexed(Draw.IndexCount, Draw.StartIndex);
		Context->End(Query);

		BoundMesh = Draw.Mesh;
		BoundMaterial = Draw.Material;
		BoundLOD = Draw.LODIndex;
	}

	// 2) 결과를 기다려 모은다. 한 물체가 Section 여러 개로 나뉘면 하나라도 보이면 보이는 것으로 친다.
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		UINT64 Samples = 0;
		while (Context->GetData(OcclusionQueries[Index].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}

		const uint64 Triangles = Draw.IndexCount / 3;
		const bool bVisible = Samples > 0;

		// Cull을 끄고 판정만 한 드로우: 가렸다고 했는데 최종 화면에 픽셀이 남았으면 잘못 가린 것이다.
		if (Draw.bOccludedByGpu)
		{
			++Result.GPUOccludedDraws;
			if (bVisible)
				++Result.FalseCulls;
		}

		++Result.TotalDraws;
		Result.TotalTriangles += Triangles;
		if (bVisible)
		{
			++Result.VisibleDraws;
			Result.VisibleTriangles += Triangles;
		}

		bool& bObjectVisible = ObjectVisible[Draw.ObjectKey];
		bObjectVisible = bObjectVisible || bVisible;
	}

	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;

	// 뒤따르는 Grid·반투명 패스를 위해 상태를 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

	Result.ElapsedMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	bObjectConstantsPrepared = false;
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	return static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB)
	{
		RenderCommand::Unmap(PerObjectSlotCB.get());
		bObjectConstantsPrepared = true;
	}
}

// Material마다 Shader/Texture/Sampler/State 꽂기
void FRenderer::BindMaterial(UMaterial* material,bool bDepthOnly)
{
	// 머티리얼 에셋은 건드리지 않고, 이번 바인딩에서만 뷰 모드에 맞는 셰이더로 바꿔 끼운다.
	FShaderProgram* Shader = material->Shader;
	if (ViewMode != ERenderViewMode::Unlit)
	{
		
		ResolveViewModeShaders();

		// 스태틱 메시 머티리얼만 바꾼다. 파티클·텍스트 등 다른 셰이더는 그대로 둔다.
		FShaderProgram* ViewModeShader = ViewModeShaders[static_cast<uint32>(ViewMode)];
		if (Shader == StaticMeshShader && ViewModeShader)
			Shader = ViewModeShader;
	}

	
	RenderCommand::BindShaderProgram(Shader);
	if (bDepthOnly) RenderCommand::GetContext()->PSSetShader(nullptr, nullptr, 0); //LightCulling용
	RenderCommand::SetBlendState(material->BlendState);
	// 반투명은 뒤에 그려지는 Grid·다른 반투명을 가리지 않도록 깊이를 쓰지 않는다.
	const bool bTranslucent = material->BlendState != EBlendState::Opaque;
	RenderCommand::SetDepthStencilState(bTranslucent && material->DepthStencilState == EDepthStencilState::Default
		? EDepthStencilState::ReadOnly : material->DepthStencilState);

	const bool bStaticMesh = material->ParamLayout == EMaterialParamLayout::StaticMesh;
	if (bStaticMesh)
	{
		RenderCommand::BindShaderResource(0, material->GetTexture(UMaterial::ColorTextureSlot), EShaderBindFlagBits::Pixel);
		// t1/t2는 라이트 버퍼다. 노멀맵은 t3에 연결하고, 없으면 이전 바인딩을 비운다.
		RenderCommand::BindShaderResource(3, material->GetTexture(UMaterial::NormalTextureSlot), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	}
	else
	{
		for (int i = 0; i < material->Textures.size(); i++)
			RenderCommand::BindShaderResource(i, material->Textures[i], EShaderBindFlagBits::Pixel);
	}
	RenderCommand::BindSamplerState(0, material->SamplerState, bStaticMesh
		? EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel : EShaderBindFlagBits::Pixel);
}

// b1 내용 채우고 꽂기
void FRenderer::UpdateMaterialParams(const FRenderPacket& RenderPacket)
{
	switch (RenderPacket.Material->ParamLayout)
	{
	case EMaterialParamLayout::StaticMesh:
	{
		//break; // 라이팅 적용 시 제거

		const float TotalTime = EngineTimer::GetTotalTime();

		FStaticMeshMaterialParams Params{};
		Params.BaseColor = RenderPacket.Material->BaseColor;
		Params.UVOffset = RenderPacket.Material->UVScrollSpeed * TotalTime;
		Params.bOpaque = RenderPacket.Material->BlendState == EBlendState::Opaque ? 1.0f : 0.0f;

		RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
		RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		break;
	}
	case EMaterialParamLayout::ParticleSubUV:
	{
		if (RenderPacket.Material->ParamBuffer && RenderPacket.MaterialParamData != nullptr)
		{
			RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
			RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		}
		break;
	}
	case EMaterialParamLayout::None:
	{
		break;
	}
	}
}

// b0 MVP 채우고 꽂기
void FRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection)
{
	// rp.Transform 과 Camera VP 행렬 곱
	// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요

	UpdatePerObjectConstants(GetPacketWorld(RenderPacket));
}

void FRenderer::UpdatePerObjectConstants(const FMatrix& World)
{
	FPerObjectConstants Constants;

	Constants.World = World;

	RenderCommand::UpdateBufferData(PerObjectCB.get(), &Constants);
}

void FRenderer::SetLights(const void* Source, uint32 Version, const FLightConstants& Constants, const TArray<FPointLightInfo>& PointLights, const TArray<FSpotLightInfo>& SpotLights)
{
	if (Source == LastLightSource && Version == LastLightVersion)
		return;   // 버퍼에 이미 같은 내용이 올라가 있다
	LastLightSource = Source;
	LastLightVersion = Version;

	UploadedPointLightCount = static_cast<uint32>(PointLights.Num()); 
	UploadedSpotLightCount = static_cast<uint32>(SpotLights.Num());   

	RenderCommand::UpdateBufferData(LightCB.get(), &Constants, sizeof(FLightConstants));
	UploadLightBuffer(PointLightBuffer, PointLights.GetData(), static_cast<uint32>(PointLights.Num()), sizeof(FPointLightInfo));
	UploadLightBuffer(SpotLightBuffer, SpotLights.GetData(), static_cast<uint32>(SpotLights.Num()), sizeof(FSpotLightInfo));
}

bool FRenderer::EnsureLightBufferCapacity(FLightStructuredBuffer& Target, uint32 Count, uint32 Stride)
{
	if (Count <= Target.Capacity && Target.Buffer && Target.SRV)
		return true;

	uint32 NewCapacity = std::max(Target.Capacity * 2, 16u);
	while (NewCapacity < Count)
		NewCapacity *= 2;

	Target.Buffer.Reset();
	Target.SRV.Reset();
	Target.Capacity = 0;

	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = NewCapacity * Stride;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;

	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
	SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SrvDesc.Buffer.NumElements = NewCapacity;

	ID3D11Device* Device = RenderCommand::GetDevice();
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, Target.Buffer.GetAddressOf())) ||
		FAILED(Device->CreateShaderResourceView(Target.Buffer.Get(), &SrvDesc, Target.SRV.GetAddressOf())))
	{
		HTR_LOG(Warning, "Light structured buffer ({} x {} bytes) creation failed.", NewCapacity, Stride);
		Target.Buffer.Reset();
		Target.SRV.Reset();
		return false;
	}

	Target.Capacity = NewCapacity;
	return true;
}

void FRenderer::UploadLightBuffer(FLightStructuredBuffer& Target, const void* Data, uint32 Count, uint32 Stride)
{
	// 셰이더는 Count만큼만 읽으므로 0개면 올릴 것이 없다
	if (Count == 0 || !EnsureLightBufferCapacity(Target, Count, Stride))
		return;

	D3D11_MAPPED_SUBRESOURCE Mapped{};
	ID3D11DeviceContext* Context = RenderCommand::GetContext();
	if (FAILED(Context->Map(Target.Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		return;

	std::memcpy(Mapped.pData, Data, static_cast<size_t>(Count) * Stride);
	Context->Unmap(Target.Buffer.Get(), 0);
}

void FRenderer::ResolveViewModeShaders()
{
	if (bViewModeShadersResolved)
		return;
	bViewModeShadersResolved = true;

	StaticMeshShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/Unlit.hlsl");

	
	struct FViewModeVariant
	{
		ERenderViewMode Mode;
		const char* Define;
	};
	static const FViewModeVariant Variants[] =
	{
		{ ERenderViewMode::Lit_Gouraud, "LIGHTING_MODEL_GOURAUD" },
		{ ERenderViewMode::Lit_Lambert, "LIGHTING_MODEL_LAMBERT" },
		{ ERenderViewMode::Lit_Phong,   "LIGHTING_MODEL_PHONG" },
	};

	// 처음 한 번 모든 변형을 받아 둔다. 캐시(.cso)가 없으면 여기서 컴파일하므로 첫 프레임에 잠깐 멈출 수 있다.
	// 실패한 변형은 nullptr로 남아 그 모드에서는 StaticMeshShader로 그린다.
	for (const FViewModeVariant& Variant : Variants)
	{
		ViewModeShaders[static_cast<uint32>(Variant.Mode)] =
			FRenderResourceManager::GetShaderProgram("Resources/Shader/UberLit.hlsl", { FShaderDefine{ Variant.Define, "1" } });
	}

	ViewModeShaders[static_cast<uint32>(ERenderViewMode::World_Normal)] = FRenderResourceManager::GetShaderProgram("Resources/Shader/WorldNormal.hlsl", {});
}

void FRenderer::SetCameraPosition(const FVector& InCameraPosition)
{
	FCameraConstants Constants{};
	Constants.CameraPosition = InCameraPosition;
	RenderCommand::UpdateBufferData(CameraCB.get(), &Constants, sizeof(FCameraConstants));
}

void FRenderer::BindLights(bool bUseTileCulling)
{
	const bool bUseTiles = bUseTileCulling && bTileCullingValid;
	FTileLightInfo TileInfo{CurrentTileCountX, bUseTiles ? 1u : 0u};
	RenderCommand::UpdateBufferData(TileLightInfoCB.get(), &TileInfo, sizeof(TileInfo));
	RenderCommand::BindConstantBuffer(6, TileLightInfoCB.get(), EShaderBindFlagBits::Pixel);
	ID3D11ShaderResourceView* TileSRV = bUseTiles ? TileLightSRV.Get() : nullptr;
	RenderCommand::GetContext()->PSSetShaderResources(4, 1, &TileSRV);
	// Gouraud는 VS에서 라이트를 계산하므로 VS에도 묶는다
	RenderCommand::BindConstantBuffer(4, LightCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::BindConstantBuffer(5, CameraCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	ID3D11ShaderResourceView* SRVs[2] = { PointLightBuffer.SRV.Get(), SpotLightBuffer.SRV.Get() };
	RenderCommand::GetContext()->VSSetShaderResources(1, 2, SRVs);
	RenderCommand::GetContext()->PSSetShaderResources(1, 2, SRVs);
}

void FRenderer::SetDirectionalLight(const FDirectionalLightConstants& LightData)
{
	DirectionalLightData = LightData;
	RenderCommand::UpdateBufferData(DirectionalLightCB.get(), &DirectionalLightData, sizeof(FDirectionalLightConstants));
}


void FRenderer::RenderDepthOnly(const FMatrix& ViewProjection)
{
	RenderCommand::UpdateBufferData(ViewCB.get(), &ViewProjection);
	DrawStaticGroups(true);
	DrawPackets(0, FirstTranslucentIndex, ViewProjection,true);
}
void FRenderer::LightCulling(const FMatrix& View, const FMatrix& Projection, uint32 Width, uint32 Height, FTexture2D* DepthTexture)
{
	bTileCullingValid = false;
	// 0. 셰이더
	if (!TileCullingShader)
		TileCullingShader = FRenderResourceManager::GetComputeShader("Resources/Shader/Compute/TileLightCulling.hlsl");
	if (!TileCullingShader)
		return;
	if (!DepthTexture || !DepthTexture->GetSRV() || !TileCullingCB)
		return;
	// ① 화면 크기에 맞는 출력 버퍼 용량 확보
	// 타일 개수 변경 시 재생성 또는 확장

	if (!EnsureTileLightBufferCapacity(Width, Height))
		return;

	// ② 이번 뷰의 컬링 상수 업로드
	FTileCullingConstants Constants{};
	Constants.View = View;
	Constants.ProjXScale = Projection.M[1][0];
	Constants.ProjYScale = Projection.M[2][1];
	Constants.ProjZScale = Projection.M[0][2];
	Constants.ProjZOffset = Projection.M[3][2];
	Constants.ScreenWidth = Width;
	Constants.ScreenHeight = Height;
	Constants.TileCountX = (Width + TileSize - 1) / TileSize;
	Constants.TileCountY = (Height + TileSize - 1) / TileSize;
	Constants.PointLightCount = UploadedPointLightCount;
	Constants.SpotLightCount = UploadedSpotLightCount;
	RenderCommand::UpdateBufferData(TileCullingCB.get(), &Constants, sizeof(FTileCullingConstants));

	// ③~⑧ 이후 단계
	// 
	// ③ 이전 Opaque 패스가 읽던 '컬링 결과 SRV' 해제
	// 동일한 버퍼를 이번에는 UAV로 쓰기 때문

	// t3는 노멀맵, 타일 목록은 UberLit의 t4다.
	RenderCommand::UnbindShaderResource(4, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	// ⑤ 입력 설정
	//    CSSetConstantBuffers(...)    // 컬링 파라미터, 라이트 개수
	//    CSSetShaderResources(...)    // Depth, PointLights, SpotLights
	RenderCommand::CSSetConstantBuffer(0, TileCullingCB.get());
	ID3D11ShaderResourceView* InputSRVs[] =
	{
		DepthTexture->GetSRV(),       // t0
		PointLightBuffer.SRV.Get(),   // t1
		SpotLightBuffer.SRV.Get()     // t2
	};
	RenderCommand::CSSetShaderResources(0, 3, InputSRVs); // t0부터,연속된 슬룻 3개에, 배열 순서대로 연결

	// ⑥ 출력 설정
	// 타일 개수, Point/Spot 인덱스
	RenderCommand::CSSetUnorderedAccessView(0, TileLightUAV.Get());

	// ⑦ 실행
	DisPatch(Constants.TileCountX, Constants.TileCountY);
	CurrentTileCountX = Constants.TileCountX;
	bTileCullingValid = true;

	// ⑧ CS의 출력 UAV, 입력 SRV, Shader 바인딩 해제
	RenderCommand::CSSetUnorderedAccessView(0, nullptr);
	ID3D11ShaderResourceView* NullSRVs[3] = {};
	RenderCommand::CSSetShaderResources(0, 3, NullSRVs);
	RenderCommand::CSSetConstantBuffer(0, nullptr);

}
void FRenderer::DisPatch(uint32 TileCountX, uint32 TileCountY)
{
	if (!TileCullingShader || TileCountX == 0 || TileCountY == 0)
		return;

	RenderCommand::CSSetShader(TileCullingShader);
	RenderCommand::Dispatch(TileCountX, TileCountY, 1);
	RenderCommand::CSSetShader(nullptr);
}

void FRenderer::RenderCullingView(const FRenderingInfo& RenderingInfo)
{
	if (!CullingViewShader)
		CullingViewShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/CullingView.hlsl");
	if (!CullingViewShader || !CullingViewCB || !CullingViewCB->GetBuffer())
		return;
	// 첫 뷰가 직교 뷰인 경우에도 유효한 SRV를 제공한다. bValid=0이면 내용은 읽지 않는다.
	if (!TileLightSRV && !EnsureTileLightBufferCapacity(1, 1))
		return;

	FCullingViewConstants Constants{CurrentTileCountX, TileSize, MaxLightsPerTile,
		bTileCullingValid ? 1u : 0u,
		{static_cast<float>(RenderingInfo.ViewportSetting.StartX), static_cast<float>(RenderingInfo.ViewportSetting.StartY)}};
	RenderCommand::UpdateBufferData(CullingViewCB.get(), &Constants, sizeof(Constants));

	FRenderingInfo PassInfo = RenderingInfo;
	for (FRenderingDesc& Color : PassInfo.ColorRenderTargets)
		Color.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.DepthStencil.Texture = nullptr;
	RenderCommand::BeginRenderPass(PassInfo);
	RenderCommand::BindShaderProgram(CullingViewShader);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidNone);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Disabled);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::BindConstantBuffer(0, CullingViewCB.get(), EShaderBindFlagBits::Pixel);
	ID3D11ShaderResourceView* SRV = TileLightSRV.Get();
	RenderCommand::GetContext()->PSSetShaderResources(0, 1, &SRV);
	RenderCommand::Draw(3);
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
	RenderCommand::EndRenderPass(PassInfo);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
}

bool FRenderer::EnsureTileLightBufferCapacity(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
		return false;

	// ① 필요한 uint 칸 수 계산
	// 올림나눗셈 
	const uint32 TileCountX = (Width + TileSize - 1) / TileSize;
	const uint32 TileCountY = (Height + TileSize - 1) / TileSize;

	const uint32 RequiredCount =
		TileCountX * TileCountY * (1 + MaxLightsPerTile);

	// ② 이미 충분한 용량이 있으면 재사용
	if (RequiredCount <= TileLightCapacity)
		return true;

	ID3D11Device* Device = RenderCommand::GetDevice();

	// 모두 생성된 뒤 기존 멤버를 교체한다.
	ComPtr<ID3D11Buffer> NewBuffer;
	ComPtr<ID3D11UnorderedAccessView> NewUAV;
	ComPtr<ID3D11ShaderResourceView> NewSRV;

	// ③ 실제 GPU 버퍼 생성
	D3D11_BUFFER_DESC BufferDesc{};
	BufferDesc.ByteWidth = RequiredCount * sizeof(uint32);
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.BindFlags =
		D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(uint32);

	if (FAILED(Device->CreateBuffer(
		&BufferDesc, nullptr, NewBuffer.GetAddressOf())))
		return false;

	// ④ CS가 쓸 UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.NumElements = RequiredCount;

	if (FAILED(Device->CreateUnorderedAccessView(
		NewBuffer.Get(), &UAVDesc, NewUAV.GetAddressOf())))
		return false;

	// ⑤ 이후 셰이더가 읽을 SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.NumElements = RequiredCount;

	if (FAILED(Device->CreateShaderResourceView(
		NewBuffer.Get(), &SRVDesc, NewSRV.GetAddressOf())))
		return false;

	// ⑥ 생성에 성공했으므로 멤버 교체
	TileLightBuffer = std::move(NewBuffer);
	TileLightUAV = std::move(NewUAV);
	TileLightSRV = std::move(NewSRV);
	TileLightCapacity = RequiredCount;

	return true;
}

