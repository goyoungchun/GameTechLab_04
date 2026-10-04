#include "LaunchEngineLoop.h"

// Visual Profiler
#define ENABLE_VISUAL_PROFILING 1
#include "FInstrumentor.h"
#include "FTextBuilder.h"

#include <windows.h>
#include "Renderer.h"
#include "WindowApplication.h"
#include "Console.h"
#include "GraphicsManager.h"
#include "CubeComponent.h"
#include "ObjectFactory.h"
#include "Object.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"
#include "Actor.h"
#include "World.h"
#include <FLogManager.h>
#include "Assets.h"
#include "FMeshDescription.h"
#include "FStaticMeshBuilder.h"
#include "FObjImporter.h"
#include "UStaticMeshComponent.h"
#include "Serializers.h"
#include "NativeFileDialog.h"
#include "FEditorUIManager.h"
#include "ShowFlags.h"
#include "FFrustum.h"
#include "FHiZOcclusionManager.h"
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

void FEngineLoop::Init(HINSTANCE hInstance, WNDPROC WndProc)
{
	timeBeginPeriod(1);
	FrameTimer = new FFrameTimer(120);

	GEngine = &GEditor;
	GEditor.Initialize(hInstance, WndProc);
}

void FEngineLoop::Tick(bool bPumpMessages)
{
	PROFILE_FUNCTION();

	if (GInTick) return;
	GInTick = true;

	FrameTimer->StartFrame();
	float deltaTime = FrameTimer->GetDeltaTime();

	{
		PROFILE_SCOPE("Frame/InputAndResize");
		WindowApplication.ProcessDeferredEvents();
	}

	{
		PROFILE_SCOPE("Frame/SceneTick");
		GEngine->Tick(deltaTime);
	}

	{
		PROFILE_SCOPE("Frame/Render");
		GEngine->Render(deltaTime);
	}

	{
		PROFILE_SCOPE("Frame/Limiter");
		FrameTimer->EndFrame();
	}

	GInTick = false;
}

void FEngineLoop::End()
{
	GEngine->Cleanup();

	timeEndPeriod(1);
	delete FrameTimer;

	PROFILE_END_SESSION();
}


