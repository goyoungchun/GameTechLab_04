#pragma once

#include "Core.h"
#include "TArray.h"
#include "enum.h"
#include <functional>

class UWorld;
class FGraphicsManager;
class FFileManager;
class FAssetManager;
class FFontManager;

struct FWorldContext
{
public:
	void SetCurrentWorld(UWorld* InWorld);

	inline UWorld* World() const { return mWorld; }

private:
	friend class FEngine;
	friend class FEditorEngine;

	UWorld* mWorld = nullptr;
	EWorldType mWorldType = EWorldType::Game;
};

class FEngine
{
public:
	using FPendingTask = std::function<void()>;

	virtual void Initialize(HINSTANCE hInstance, WNDPROC WndProc);
	virtual void Cleanup();

	virtual void Tick(float DeltaTime) = 0;
	virtual void Render(float DeltaTime) = 0;

	FWorldContext& CreateNewWorldContext(EWorldType WorldType);
	void DestroyWorldContext(FWorldContext* Context);
	void DestroyWorldContext(UWorld* InWorld);

	void EnqueuePendingTask(const FPendingTask& Task)
	{
		mPendingTasks.Emplace(Task);
	}

	FGraphicsManager& GetGraphicsManager() { return *mGraphicsManager; }
	FFileManager& GetFileManager() { return *mFileManager; }
	FAssetManager& GetAssetManager() { return *mAssetManager; }
	FFontManager& GetFontManager() { return *mFontManager; }

protected:
	inline void ProcessPendingTasks()
	{
		for (FPendingTask& Task : mPendingTasks)
		{
			Task();
		}
		mPendingTasks.Empty();
	}

	virtual void OnCreateWorldContext(FWorldContext& NewContext) {}
	virtual void OnDestroyWorldContext(FWorldContext& Context) {}

protected:
	HWND mHWnd;

	FGraphicsManager* mGraphicsManager;
	FFileManager* mFileManager;
	FAssetManager* mAssetManager;
	FFontManager* mFontManager;

	TArray<FWorldContext> mWorldContexts;
	TArray<FPendingTask> mPendingTasks;
};

extern FEngine* GEngine;
