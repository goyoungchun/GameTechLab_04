#include "EnginePCH.h"
#include "ShadowAtlasAllocator.h"

static inline bool IsPowerOfTwo(uint32 x)
{
	return x != 0 && (x & (x - 1)) == 0;
}

void FShadowAtlasAllocator::Reset(uint32 InAtlasSize, uint32 InMinTileSize)
{
	// 2의 제곱 크기만 일단 가정 (256, 512, 1024, 2048, 4096)
	assert(IsPowerOfTwo(InAtlasSize) && IsPowerOfTwo(InMinTileSize));
	assert(InMinTileSize <= InAtlasSize);

	AtlasSize = InAtlasSize;
	MinTileSize = InMinTileSize;
	const uint32 Count = AtlasSize / MinTileSize;
	TotalCells = Count * Count;

	for (auto& FreeList : FreeLists)
		FreeList.Reset();
	
	FreeLists[0].Add(0);
}

FShadowAtlasTile FShadowAtlasAllocator::Allocate(uint32 Size)
{
	uint32 Want = LevelOf(Size);
	int32 Found = Want;

	while (Found >= 0 && FreeLists[Found].IsEmpty())
		--Found;

	if (Found < 0)
		return {};

	uint32 Index = PopLowest(Found);
	for (uint32 Level = Found; Level < Want; ++Level)
	{
		uint32 Child = CellsAt(Level + 1);
		for (uint32 i = 1; i <= 3; ++i)
			FreeLists[Level + 1].Add(Index + i * Child);
	}

	return MakeTile(Index, Size);
}

void FShadowAtlasAllocator::Free(const FShadowAtlasTile& Tile)
{
	if (Tile.Size == 0)
		return;

	uint32 Level = LevelOf(Tile.Size);
	uint32 Index = Tile.Index;

	assert(Index % CellsAt(Level) == 0);
	
	while (Level > 0)
	{
		const uint32 BlockCells = CellsAt(Level);
		const uint32 Parent = Index & ~(CellsAt(Level - 1) - 1);
		bool bAllChildrenFree = true;
		for (uint32 c = 0; c < 4; ++c)
		{
			const uint32 Child = Parent + c * BlockCells;

			if (Child == Index)
				continue;

			if (FindFree(Level, Child) == INDEX_NONE)
			{
				bAllChildrenFree = false;
				break;
			}
		}

		if (!bAllChildrenFree)
			break;

		for (uint32 c = 0; c < 4; ++c)
		{
			const uint32 Child = Parent + c * BlockCells;

			if (Child == Index)
				continue;

			const int32 Idx = FindFree(Level, Child);
			assert(Idx != INDEX_NONE);
			FreeLists[Level].RemoveAtSwap(Idx);
		}

		Index = Parent;
		--Level;
	}

	FreeLists[Level].Add(Index);
}

int32 FShadowAtlasAllocator::FindFree(uint32 Level, uint32 Index) const
{
	int32 Idx = FreeLists[Level].Find(Index);
	return Idx < FreeLists[Level].Num() ? Idx : INDEX_NONE;
}

uint32 FShadowAtlasAllocator::LevelOf(uint32 Size) const
{
	assert(IsPowerOfTwo(Size));
	assert(MinTileSize <= Size && Size <= AtlasSize);
	return log2(AtlasSize / Size);
}

uint32 FShadowAtlasAllocator::CellsAt(uint32 Level) const
{
	return TotalCells >> (2 * Level);
}

uint32 FShadowAtlasAllocator::PopLowest(uint32 Level)
{
	TArray<uint32>& List = FreeLists[Level];

	assert(!List.IsEmpty());

	uint32 Lowest = 0;
	uint32 MinIdx = 0;
	for (uint32 i = 0; i < List.Num(); ++i)
	{
		if (List[i] <= List[MinIdx])
		{
			Lowest = List[i];
			MinIdx = i;
		}
	}
	List.RemoveAtSwap(MinIdx);
	return Lowest;
}

FShadowAtlasTile FShadowAtlasAllocator::MakeTile(uint32 Index, uint32 Size) const
{
	const uint32 CellX = CompactEvenBits(Index);
	const uint32 CellY = CompactEvenBits(Index >> 1);
	const uint32 PixelX = CellX * MinTileSize;
	const uint32 PixelY = CellY * MinTileSize;

	return { PixelX, PixelY, Size, Index };
}

bool FShadowAtlasAllocator::TryAllocate(uint32 Size, uint32 Count, FShadowAtlasTile* OutTiles)
{
	for (uint32 i = 0; i < Count; ++i)
	{
		OutTiles[i] = Allocate(Size);

		if (OutTiles[i].Size == 0)
		{
			for (uint32 j = 0; j < i; ++j)
				Free(OutTiles[j]);

			for (uint32 j = 0; j < Count; ++j)
				OutTiles[j] = {};

			return false;
		}
	}
	return true;
}

uint32 FShadowAtlasAllocator::CompactEvenBits(uint32 Value) const
{
	Value &= 0x55555555u;
	Value = (Value | (Value >> 1)) & 0x33333333u;
	Value = (Value | (Value >> 2)) & 0x0F0F0F0Fu;
	Value = (Value | (Value >> 4)) & 0x00FF00FFu;	
	Value = (Value | (Value >> 8)) & 0x0000FFFFu;
	return Value;
}