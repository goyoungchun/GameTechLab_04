#pragma once

#include <Windows.h>
#include <memory>

#include "Core/FrameTimer.h"
#include "Core/AssetManager.h"
#include "Core/IO/FileManager.h"
#include "Editor/FEditorViewportClient.h"
#include "Editor/EditorUIManager.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include "Engine/Stats/StatManager.h"
#include "Rendering/Camera.h"
#include "Rendering/Renderer.h"
#include "Rendering/FontResource.h"
#include "Rendering/GpuResourceManager.h"

#include <d3d11.h>

class Sphere;
class FGraphicsManager;
class FEditorViewportManager;

class FEngineLoop
{
public:
	FEngineLoop()
	{
	}
	~FEngineLoop() {};

	void Init(HINSTANCE hInstance, WNDPROC WndProc);
	void Tick(bool bPumpMessages);
	void End();
private:
	// Todo: Make as pointer
	FFrameTimer* FrameTimer;
	bool GInTick = false;

	/* Managers */
	FGraphicsManager* mGraphicsManager;
	FSceneManager* mSceneManager;
	FFileManager* mFileManager;
	FEditorUIManager* mEditorUIManager;
	std::unique_ptr<FAssetManager> mAssetManager;
	FEditorViewportManager* mEditorViewportManager;
	FEditorViewportClient* viewportClient = nullptr;
	FStatManager mStatManager;

	std::unique_ptr<FFontResource> mDefaultFontResource;
	std::unique_ptr<FGpuResourceManager> mGpuResourceManager;

	/* Editor Command */
	void processEditorCommands(const FEditorCommands& commands);
	void processEditorCommand(const FNewSceneCommand& command);
	void processEditorCommand(const FSaveSceneCommand& command);
	void processEditorCommand(const FSaveSceneAsCommand& command);
	void processEditorCommand(const FLoadSceneCommand& command);
	void processEditorCommand(const FLoadObjCommand& command);

	void processEditorCommand(const FSpawnActorCommand& command);
	void processEditorCommand(const FSpawnStaticMeshActorCommand& command);
	void processEditorCommand(const FDeleteActorCommand& command);
	void processEditorCommand(const FSpawnParticleCommand& command);

	void processEditorCommand(const FSetActorLocationCommand& command);
	void processEditorCommand(const FSetActorRotationCommand& command);
	void processEditorCommand(const FSetActorScaleCommand& command);
	void processEditorCommand(const FSetActorNameCommand& command);
	void processEditorCommand(const FSetSelectedActorCommand& command);

	void processEditorCommand(const FSetComponentUseTextureCommand& command);
	void processEditorCommand(const FSetComponentColorCommand& command);
	void processEditorCommand(const FSetStaticMeshComponentStaticMeshCommand& command);
	void processEditorCommand(const FSetStaticMeshComponentMaterialCommand& command);
	void processEditorCommand(const FSetStaticMeshComponentSubUVCommand& command);
	void processEditorCommand(const FSetSphereComponentSpinCommand& command);
	void processEditorCommand(const FSetSphereComponentSpinSpeedCommand& command);
	void processEditorCommand(const FSetParticleSubUVComponentLoopingCommand& command);
	void processEditorCommand(const FSetParticleSubUVComponentPlayRateCommand& command);
	void processEditorCommand(const FSetParticleSubUVComponentBlendStateTypeCommand& command);
	void processEditorCommand(const FSetPropertyCommand& command);

	void processEditorCommand(const FSetViewModeCommand& command);
	void processEditorCommand(const FSetShowFlagCommand& command);
	void processEditorCommand(const FSetCameraSensitivityCommand& command);
	void processEditorCommand(const FSetCameraFovCommand& command);
	void processEditorCommand(const FSetCameraLocationCommand& command);
	void processEditorCommand(const FSetCameraRotationCommand& command);
	void processEditorCommand(const FSetGizmoModeCommand& command);
	void processEditorCommand(const FCycleGizmoModeCommand& command);

	void processEditorCommand(const FSetViewportTypeCommand& command);
	void processEditorCommand(const FSetViewportViewModeCommand& command);
	void processEditorCommand(const FSetViewportShowFlagCommand& command);
	void processEditorCommand(const FSetSharedCameraSpeedCommand& command);
	void processEditorCommand(const FSetSharedSnapPresetCommand& command);
	void processEditorCommand(const FSetViewportFovCommand& command);

	void processEditorCommand(const FSetGridWidthCommand& command);
	void processEditorCommand(const FStartProjectionTransitionCommand& command);

	void processEditorCommand(const FToggleStatCommand& command);
	void processEditorCommand(const FDisableAllStatsCommand& command);
};

inline FEngineLoop GEngineLoop;
