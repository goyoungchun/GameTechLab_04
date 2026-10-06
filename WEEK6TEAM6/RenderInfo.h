#pragma once

#include "Transform.h"
#include "Object.h"
#include "FName.h"
#include "Assets.h"
#include "TArray.h"
#include "FFrustum.h"
#include "FBVH.h"
#include "TRangePool.h"
#include <algorithm>

class FCamera;
class UPrimitiveComponent;
class FRenderPipeline;

inline uint64 MakeRenderSortKey(uint16 pipelineID, uint32 materialID, uint32 meshID)
{
	return (static_cast<uint64>(pipelineID) << 48) |
		((static_cast<uint64>(materialID) & 0x00FFFFFF) << 24) |
		(static_cast<uint64>(meshID) & 0x00FFFFFF);
}

enum class ERenderBlendMode
{
	Opaque,
	Masked,
	Transparent,
	Additive,
	Count
};

struct FRenderInfo
{
	uint64 SortKey = 0;
	FRenderPipeline* Pipeline = nullptr;
	// 버퍼 소유권은 메시 에셋 또는 GraphicsManager에 있습니다.
	// 수집부터 Draw 제출까지 버퍼를 교체/해제하지 않고, 다음 프레임에는 다시 수집합니다.
	ID3D11Buffer* VertexBuffer = nullptr;
	uint32 VertexCount = 0;
	ID3D11Buffer* IndexBuffer = nullptr;
	uint32 StartIndex = 0;
	uint32 IndexCount = 0;
	FTexture2DAsset* Texture = nullptr;
	FVector2 UVOffset = { 0.f, 0.f };
	FMatrix Model;
	uint32 ObjectInternalIndex;
	FVector4 Color = { 1.f, 1.f, 1.f, 1.f };
	bool UseVertexColor = true;

	bool operator<(const FRenderInfo& Other) const
	{
		return SortKey < Other.SortKey;
	}
};

struct FRenderQuadInfo
{
	FMatrix Model;
	FVector4 Color = { 1.f, 1.f, 1.f, 1.f };
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
	DXGI_FORMAT TextureFormat = DXGI_FORMAT_UNKNOWN;
	FVector4 SubUV = { 0.f, 0.f, 1.f, 1.f };
	ERenderBlendMode BlendMode = ERenderBlendMode::Opaque;
	bool EnableDepthTest = true;
	bool EnableDepthWrite = true;
};

struct FRenderQuad2DInfo
{
	FVector2 Position;
	FVector2 Size;
	FVector4 Color = { 1.f, 1.f, 1.f, 1.f };
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
	DXGI_FORMAT TextureFormat = DXGI_FORMAT_UNKNOWN;
	FVector4 SubUV = { 0.f, 0.f, 1.f, 1.f };
	float Rotation = 0.f;
	ERenderBlendMode BlendMode = ERenderBlendMode::Opaque;
};

struct FRenderLineInfo
{
	FVector4 Color;
	FVector3 Start;
	float Thickness;
	FVector3 End;
	float Padding;
};

// 이번 프레임에 그릴 것들을 한데 모은다. 소유자는 FGraphicsManager.
struct FRenderCollector
{
public:
	enum { DEFAULT_RESERVE_MEM = 1024U };

	bool bRenderInfosSorted = true;
	bool bHasPreviousSortKey = false;
	uint64 PreviousSortKey = 0;

	FCamera* Camera = nullptr;
	FFrustum Frustum;
	bool bNeedPickTargets = false;

	TArray<FRenderLineInfo> LineInfos; // 라인 패스
	FBVH<UPrimitiveComponent*>* BVH = nullptr;

	inline void AddQuad2DInfo(const FRenderQuad2DInfo& Quad2DInfo)
	{
		Quad2DInfos.Add(Quad2DInfo);
	}

	inline void Clear()
	{
		BVH = nullptr;
		bNeedPickTargets = false;
		VisibleRenderOverlayQuadInfoIndices.Reset(DEFAULT_RESERVE_MEM);
		VisibleRenderTransparentQuadInfoIndices.Reset(DEFAULT_RESERVE_MEM);
		VisibleRenderQuadInfoIndices.Reset(DEFAULT_RESERVE_MEM);
		VisibleRenderInfoIndices.Reset(DEFAULT_RESERVE_MEM);
		LineInfos.Reset(DEFAULT_RESERVE_MEM);
		Quad2DInfos.Reset(DEFAULT_RESERVE_MEM);
	}

	inline const TRangePool<FRenderInfo>& GetRenderInfoPool() const { return RenderInfoPool; }
	inline TArray<int32>& GetVisibleRenderInfoIndices() { return VisibleRenderInfoIndices; }

	inline const TRangePool<FRenderQuadInfo>& GetRenderQuadInfoPool() const { return RenderQuadInfoPool; }
	inline TArray<int32>& GetVisibleRenderQuadInfoIndices() { return VisibleRenderQuadInfoIndices; }

	inline const TRangePool<FRenderQuadInfo>& GetRenderTransparentQuadInfoPool() const { return RenderTransparentQuadInfoPool; }
	inline TArray<int32>& GetVisibleRenderTransparentQuadInfoIndices() { return VisibleRenderTransparentQuadInfoIndices; }

	inline const TRangePool<FRenderQuadInfo>& GetRenderOverlayQuadInfoPool() const { return RenderOverlayQuadInfoPool; }
	inline TArray<int32>& GetVisibleRenderOverlayQuadInfoIndices() { return VisibleRenderOverlayQuadInfoIndices; }

	inline const TArray<FRenderQuad2DInfo>& GetQuad2DInfos() const { return Quad2DInfos; }

private:
	friend class FRenderProxy;

	TRangePool<FRenderInfo> RenderInfoPool;
	TArray<int32> VisibleRenderInfoIndices;

	TRangePool<FRenderQuadInfo> RenderQuadInfoPool;
	TArray<int32> VisibleRenderQuadInfoIndices;

	TRangePool<FRenderQuadInfo> RenderTransparentQuadInfoPool;
	TArray<int32> VisibleRenderTransparentQuadInfoIndices;

	TRangePool<FRenderQuadInfo> RenderOverlayQuadInfoPool;
	TArray<int32> VisibleRenderOverlayQuadInfoIndices;

	TArray<FRenderQuad2DInfo> Quad2DInfos;
};

class FRenderProxy
{
public:
	FRenderProxy() = default;
	~FRenderProxy()
	{
		ReleaseRenderInfos();
		ReleaseRenderQuadInfos();
		ReleaseRenderTransparentQuadInfos();
		ReleaseRenderOverlayQuadInfos();
	}

	inline void SetCollector(FRenderCollector& InCollector)
	{
		if (Collector == &InCollector)
		{
			return;
		}

		ReleaseRenderInfos();
		ReleaseRenderQuadInfos();
		ReleaseRenderTransparentQuadInfos();
		ReleaseRenderOverlayQuadInfos();

		Collector = &InCollector;
	}

	inline void ReserveRenderInfos(int32 Count)
	{
		if (RenderInfoBlock.IsValid())
		{
			if (RenderInfoBlock.Size >= Count)
			{
				return;
			}

			Collector->RenderInfoPool.Release(RenderInfoBlock);
		}

		RenderInfoBlock = Collector->RenderInfoPool.Allocate(Count);
		ActiveRenderInfoNum = 0;
	}

	inline void ReserveRenderQuadInfos(int32 Count)
	{
		if (RenderQuadInfoBlock.IsValid())
		{
			if (RenderQuadInfoBlock.Size >= Count)
			{
				return;
			}
			Collector->RenderQuadInfoPool.Release(RenderQuadInfoBlock);
		}

		RenderQuadInfoBlock = Collector->RenderQuadInfoPool.Allocate(Count);
		ActiveRenderQuadInfoNum = 0;
	}

	inline void ReserveRenderTransparentQuadInfos(int32 Count)
	{
		if (RenderTransparentQuadInfoBlock.IsValid())
		{
			if (RenderTransparentQuadInfoBlock.Size >= Count)
			{
				return;
			}
			Collector->RenderTransparentQuadInfoPool.Release(RenderTransparentQuadInfoBlock);
		}

		RenderTransparentQuadInfoBlock = Collector->RenderTransparentQuadInfoPool.Allocate(Count);
		ActiveRenderTransparentQuadInfoNum = 0;
	}

	inline void ReserveRenderOverlayQuadInfos(int32 Count)
	{
		if (RenderOverlayQuadInfoBlock.IsValid())
		{
			if (RenderOverlayQuadInfoBlock.Size >= Count)
			{
				return;
			}
			Collector->RenderOverlayQuadInfoPool.Release(RenderOverlayQuadInfoBlock);
		}

		RenderOverlayQuadInfoBlock = Collector->RenderOverlayQuadInfoPool.Allocate(Count);
		ActiveRenderOverlayQuadInfoNum = 0;
	}

	inline void ReleaseRenderInfos()
	{
		if (!RenderInfoBlock.IsValid())
		{
			return;
		}

		Collector->RenderInfoPool.Release(RenderInfoBlock);
		RenderInfoBlock = {};
		ActiveRenderInfoNum = 0;
	}

	inline void ReleaseRenderQuadInfos()
	{
		if (!RenderQuadInfoBlock.IsValid())
		{
			return;
		}

		Collector->RenderQuadInfoPool.Release(RenderQuadInfoBlock);
		RenderQuadInfoBlock = {};
		ActiveRenderQuadInfoNum = 0;
	}

	inline void ReleaseRenderTransparentQuadInfos()
	{
		if (!RenderTransparentQuadInfoBlock.IsValid())
		{
			return;
		}

		Collector->RenderTransparentQuadInfoPool.Release(RenderTransparentQuadInfoBlock);
		RenderTransparentQuadInfoBlock = {};
		ActiveRenderTransparentQuadInfoNum = 0;
	}

	inline void ReleaseRenderOverlayQuadInfos()
	{
		if (!RenderOverlayQuadInfoBlock.IsValid())
		{
			return;
		}

		Collector->RenderOverlayQuadInfoPool.Release(RenderOverlayQuadInfoBlock);
		RenderOverlayQuadInfoBlock = {};
		ActiveRenderOverlayQuadInfoNum = 0;
	}

	inline void SetActiveRenderInfoNum(int32 Count)
	{
		ActiveRenderInfoNum = Count;
	}

	inline void SetActiveRenderQuadInfoNum(int32 Count)
	{
		ActiveRenderQuadInfoNum = Count;
	}

	inline void SetActiveRenderTransparentQuadInfoNum(int32 Count)
	{
		ActiveRenderTransparentQuadInfoNum = Count;
	}

	inline void SetActiveRenderOverlayQuadInfoNum(int32 Count)
	{
		ActiveRenderOverlayQuadInfoNum = Count;
	}

	inline FRenderInfo& GetRenderInfo(uint32 Index)
	{
		return Collector->RenderInfoPool.Get(RenderInfoBlock, Index);
	}

	inline FRenderQuadInfo& GetRenderQuadInfo(uint32 Index)
	{
		return Collector->RenderQuadInfoPool.Get(RenderQuadInfoBlock, Index);
	}

	inline FRenderQuadInfo& GetRenderTransparentQuadInfo(uint32 Index)
	{
		return Collector->RenderTransparentQuadInfoPool.Get(RenderTransparentQuadInfoBlock, Index);
	}

	inline FRenderQuadInfo& GetRenderOverlayQuadInfo(uint32 Index)
	{
		return Collector->RenderOverlayQuadInfoPool.Get(RenderOverlayQuadInfoBlock, Index);
	}

	inline void Submit()
	{
		if (RenderInfoBlock.IsValid())
		{
			for (uint32 i = 0; i < ActiveRenderInfoNum; ++i)
			{
				Collector->VisibleRenderInfoIndices.Emplace(RenderInfoBlock.Index + i);
			}
		}

		if (RenderQuadInfoBlock.IsValid())
		{
			for (uint32 i = 0; i < ActiveRenderQuadInfoNum; ++i)
			{
				Collector->VisibleRenderQuadInfoIndices.Emplace(RenderQuadInfoBlock.Index + i);
			}
		}
		
		if (RenderTransparentQuadInfoBlock.IsValid())
		{
			for (uint32 i = 0; i < ActiveRenderTransparentQuadInfoNum; ++i)
			{
				Collector->VisibleRenderTransparentQuadInfoIndices.Emplace(RenderTransparentQuadInfoBlock.Index + i);
			}
		}

		if (RenderOverlayQuadInfoBlock.IsValid())
		{
			for (uint32 i = 0; i < ActiveRenderOverlayQuadInfoNum; ++i)
			{
				Collector->VisibleRenderOverlayQuadInfoIndices.Emplace(RenderOverlayQuadInfoBlock.Index + i);
			}
		}
	}

private:
	FRenderCollector* Collector = nullptr;

	FRangePoolBlock RenderInfoBlock;
	uint32 ActiveRenderInfoNum = 0;

	FRangePoolBlock RenderQuadInfoBlock;
	uint32 ActiveRenderQuadInfoNum = 0;

	FRangePoolBlock RenderTransparentQuadInfoBlock;
	uint32 ActiveRenderTransparentQuadInfoNum = 0;

	FRangePoolBlock RenderOverlayQuadInfoBlock;
	uint32 ActiveRenderOverlayQuadInfoNum = 0;
};
