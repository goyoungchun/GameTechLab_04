#pragma once

#include "../../Engine/Stats/StatTypes.h"

class FFrameTimer;
class FSceneManager;
class FAssetManager;
class FGpuResourceManager;

struct FStatCollectionSources
{
	const FSceneManager& SceneManager;
	const FAssetManager& AssetManager;
	const FGpuResourceManager& GpuResourceManager;
};

class FStatManager
{
public:
	bool Toggle(EStatGroup group);
	void DisableAll();

	bool IsEnabled(EStatGroup group) const;
	bool HasAnyEnabledStat() const;

	void UpdateFrame(const FFrameTimer& frameTimer);

	void UpdateMemory(float deltaTime, const FStatCollectionSources& sources);

	const FStatSnapshot& GetSnapshot() const { return mSnapshot;  }

private:
	void CollectMemory(const FStatCollectionSources& sources);

	uint8 mEnabledMask = 0;

	bool mbFrameInitialized = false;
	bool mbMemoryDirty = true;

	float mMemorySampleElapsed = 0.0f;

	FStatSnapshot mSnapshot;
};
