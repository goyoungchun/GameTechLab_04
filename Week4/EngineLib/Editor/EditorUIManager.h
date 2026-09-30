#pragma once

#include <variant>

#include "Core/Core.h"
#include "Core/Object/Object.h"
#include "Core/enum.h"

#include "EditorSetting.h"
#include "EditorCommands.h"

/* Foward Declarations */
class ImGuiIO;
class URenderer;
class FFrameTimer;
class FGraphicsManager;
class FEditorViewportClient;
class FFileManager;
class FSceneManager;
class FAssetManager;
class FEditorViewportManager;
class FStatManager;

struct ID3D11ShaderResourceView;



struct FGuiReference
{
	const FFrameTimer& FrameTimer;
	const FSceneManager& SceneManager;
	const FEditorViewportClient& ViewportClient;
	const FGraphicsManager& GraphicsManager;
	const FFileManager& FileManager;
	const FAssetManager& AssetManager;
	FEditorViewportManager& ViewportManager;
	const FStatManager& StatManager;
};

struct FGuiInputField
{
	/* Spawn Actor */
	EPrimitive PrimitiveType = EPrimitive::EP_Cube;
	int32 SelectedMeshIndex = 0;
	int32 SpawnCount = 1;

	/* Scene Control */
	char SceneName[512] = "Default";

	/* Object Lists */
	TArray<UObject*> SortedObjectLists;
	uint64 LastGUObjectRevision = -1;

	/* Static Mesh */
	FName SelectedStaticMeshKey;
};

class ImGuiIO;
class FEditorUIManager
{
public:
	FEditorUIManager(const ImGuiIO& io);

	static constexpr float BOTTOM_BAR_HEIGHT = 32.0f;

	void LoadSettings(FEditorViewportManager& viewportManager, FEditorCommands& outCommands);
	void saveSettings(const FEditorViewportManager& viewportManager);
	void UpdateGui(const FGuiReference& guiReference,FViewportSharedSettings& sharedsettings, FEditorCommands& outCommands);
	float GetPanelWidth() const { return mPanelWidth; }
	/*void RenderLoadingScreen(FGraphicsManager& graphicsManager);*/

private:
	// Internal state for ImGui input fields and other GUI elements
	FGuiInputField mGuiInputField;
	FEditorSetting mEditorSetting;

	const ImGuiIO& mImGuiIO;
	/*ID3D11ShaderResourceView* mLoadingScreenSRV = nullptr;*/

	float mPanelWidth = 300.0f; // Default width for the property and object list panels

	static constexpr float MIN_WIDTH_RATIO = 0.2f;
	static constexpr float MAX_WIDTH_RATIO = 0.6f;

	static constexpr float CONTROL_PANEL_HEIGHT_RATIO = 0.25f;
	static constexpr float WINDOW_PROPERTY_HEIGHT_RATIO = 0.5f;
	static constexpr float CONSOLE_POPUP_HEIGHT_RATIO = 0.35f;

	void drawMainMenuBar(FEditorCommands& outCommands);
	void updateBottomBarGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void drawAssetBrowserContents(const FAssetManager& assetManager);
	void updateControlPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updatePropertyWindowGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updateObjectListPanelGUI(const FGuiReference& guiReference, FEditorCommands& outCommands);
	void updateViewportLayoutPanelGUI(FEditorViewportManager& viewportManager, FViewportSharedSettings& sharedsettings, const FStatManager& statManager, FEditorCommands& outCommands);
};
