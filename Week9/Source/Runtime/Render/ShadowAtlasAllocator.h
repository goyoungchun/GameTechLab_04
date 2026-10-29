#pragma once

#include "Core/Types.h"

struct FShadowAtlasTile
{
	uint32 X = 0;
	uint32 Y = 0;
	uint32 Size = 0;
	uint32 Index = 0;
};

class FShadowAtlasAllocator
{
public:
	void Reset(uint32 InAtlasSize, uint32 InMinTileSize);
	FShadowAtlasTile Allocate(uint32 Size);
	void Free(const FShadowAtlasTile& Tile);
	bool TryAllocate(uint32 Size, uint32 Count, FShadowAtlasTile* OutTiles);

private:
	uint32 LevelOf(uint32 Size) const;
	uint32 CellsAt(uint32 Level) const;
	uint32 PopLowest(uint32 Level);
	int32 FindFree(uint32 Level, uint32 Index) const;
	uint32 CompactEvenBits(uint32 Value) const;
	FShadowAtlasTile MakeTile(uint32 Index, uint32 Size) const;

private:
	static constexpr uint32 MaxLevels = 8;
	TArray<uint32> FreeLists[MaxLevels];
	uint32 AtlasSize = 0;
	uint32 MinTileSize = 0;
	uint32 TotalCells = 0;
};