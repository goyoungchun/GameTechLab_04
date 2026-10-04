#pragma once

#include <Windows.h>
#include "FrameTimer.h"
#include "FEditorViewportClient.h"
#include "Camera.h"
#include "SceneManager.h"
#include "FileManager.h"
#include "Renderer.h"
#include "World.h"
#include "FAssetManager.h"
#include "FFontManager.h"
#include "FComponentVisualizer.h"
#include "FContentBrowser.h"
#include "GraphicsManager.h"
#include <d3d11.h>

class FEngineLoop
{
public:
	FEngineLoop() = default;
	~FEngineLoop() = default;

	void Init(HINSTANCE hInstance, WNDPROC WndProc);
	void Tick(bool bPumpMessages);
	void End();

	inline FFrameTimer* GetFrameTimer() const { return FrameTimer; }

private:
	bool GInTick = false;

	FFrameTimer* FrameTimer;
};

inline FEngineLoop GEngineLoop;
