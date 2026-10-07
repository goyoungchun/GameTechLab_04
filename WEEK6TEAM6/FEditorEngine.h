#pragma once

#include "Core.h"
#include "FEngine.h"
#include "SceneData.h"
#include "TArray.h"
#include "RenderInfo.h"
#include "enum.h"
#include "FAssetManager.h"
#include "FObjViewer.h"
#include "FFrustum.h"
#include "FEditorViewportClient.h"
#include <string_view>
#include <filesystem>
#include <ImGui/imgui.h>

inline constexpr std::string_view kSceneDataDir = "SceneData\\";
inline constexpr std::string_view kSceneDataSuffix = ".Scene";

class FFileManager;
class FFrameTimer;
class FEditorViewportClient;
class FGraphicsManager;
class UWorld;
class URenderer;
struct FViewport;
struct FEditorLayout;
struct FEditorViewport;
class UStaticMesh;
class UActorComponent;
class FEditorUIManager;
class FComponentVisualizerManager;

struct FEditorLayout
{
	bool bIsSplitView = false;
	int32 MaximizedViewportIndex = 0;

	TSharedPtr<SWindow> RootWindow;
	TSharedPtr<SSplitterH> HSplitter;
	TSharedPtr<SSplitterV> VSplitter[2];
	TSharedPtr<SWindow> ViewportWindows[4];

	void Initialize(const FRect& InRect)
	{
		// Build viewport layout tree
		TSharedPtr<SSplitterH> HRoot = MakeShared<SSplitterH>();
		TSharedPtr<SSplitterV> VSplitter0 = MakeShared<SSplitterV>();
		TSharedPtr<SSplitterV> VSplitter1 = MakeShared<SSplitterV>();

		for (int32 i = 0; i < 4; ++i)
		{
			ViewportWindows[i] = MakeShared<SWindow>();
		}

		VSplitter0->SideLT = ViewportWindows[0];
		VSplitter0->SideRB = ViewportWindows[1];

		VSplitter1->SideLT = ViewportWindows[2];
		VSplitter1->SideRB = ViewportWindows[3];

		HRoot->SideLT = VSplitter0;
		HRoot->SideRB = VSplitter1;

		HRoot->SetRect(InRect);

		RootWindow = HRoot;
		HSplitter = HRoot;
		VSplitter[0] = VSplitter0;
		VSplitter[1] = VSplitter1;
	}

	void Resize(const FRect& InRect)
	{
		RootWindow->SetRect(InRect);
	}

	void SetSplitRatios(float HorizontalRatio, float VerticalRatio)
	{
		HSplitter->SplitterRatio = HorizontalRatio;
		VSplitter[0]->SplitterRatio = VerticalRatio;
		VSplitter[1]->SplitterRatio = VerticalRatio;
	}

	void GetSplitRatios(float& HorizontalRatio, float& VerticalRatio)
	{
		HorizontalRatio = HSplitter->SplitterRatio;
		VerticalRatio = VSplitter[0]->SplitterRatio;
	}
};

struct FEditorViewport
{
	TSharedPtr<SWindow> Window;
	TSharedPtr<FViewport> Viewport;
	TSharedPtr<FEditorViewportClient> Client;

	void Release()
	{
		Viewport.reset();
		Client.reset();
		Window.reset();
	}
};

class FEditorEngine : public FEngine
{
public:
	virtual void Initialize(HINSTANCE hInstance, WNDPROC WndProc) override;
	virtual void Cleanup() override;

	FWorldContext* GetEditorWorldContext();
	FWorldContext* GetPIEWorldContext();

	virtual void Tick(float DeltaTime) override;
	virtual void Render(float DeltaTime) override;

	UActorComponent* GetSelectedComponent() const { return mSelectedComponent; }
	bool IsComponentSelected() const { return mSelectedComponent != nullptr; }
	void SetSelectedComponent(UActorComponent* component);
	void ResetSelectedComponent() { mSelectedComponent = nullptr; }

	static constexpr int32 MaxViewportCount = 4;
	static constexpr int32 MainViewportIndex = 0;

	FEditorViewport& GetMainViewport() { return mViewports[MainViewportIndex]; }
	const FEditorViewport& GetMainViewport() const { return mViewports[MainViewportIndex]; }

	void SetMouseCursor(ImGuiMouseCursor InMouseCursor) { mMouseCursor = InMouseCursor; }

	void SaveEditorSettings();
	void LoadEditorSettings();

	void CreateNewMapForEditing();

	void StartPIE();
	void EndPIE();

private:
	void InitAssetManager();

	void OnCreateWorldContext(FWorldContext& NewContext) override;
	void OnDestroyWorldContext(FWorldContext& Context) override;

private:
	FEditorLayout mEditorLayout;
	FEditorViewport mViewports[4]; // 0 : MainView 1, 2, 3 : Other

	FEditorUIManager* mEditorUIManager;
	FComponentVisualizerManager* mComponentVisualizerManager;

	int32  mEditorWorldContextIndex = -1;
	UActorComponent* mSelectedComponent = nullptr;
	ImGuiMouseCursor mMouseCursor = ImGuiMouseCursor_Arrow;
	bool mbIsPlayingInEditor = false;
};

extern FEditorEngine GEditor;

void SaveMap(UWorld* World, FCamera* Camera, const std::filesystem::path& scenePath, const FFileManager& fileManager);
void LoadMap(UWorld* World, FCamera* Camera, const std::filesystem::path& scenePath, const FFileManager& fileManager);
