#include "FEditorEngine.h"

#include <algorithm>
#include <format>

#include "FileManager.h"
#include "NativeFileDialog.h"
#include "EngineStatics.h"
#include "JsonUtil.h"
#include "ObjectFactory.h"
#include "PrimitiveComponent.h"
#include "TArray.h"
#include "World.h"
#include "FEditorViewportClient.h"
#include "Camera.h"
#include "Console.h"
#include "FLogManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

#include "FrameTimer.h"
#include "CubeComponent.h"
#include "UAtlasAnimationComponent.h"
#include "ActorComponent.h"
#include "WindowApplication.h"

#include "Cube.h"
#include "Sphere.h"
#include "Circle.h"
#include "Triangle.h"
#include "Plane.h"
#include "GizmoArrow.h"
#include "Assets.h"
#include "UTextComponent.h"
#include "ShowFlags.h"
#include "UStaticMeshComponent.h"
#include "GraphicsManager.h"
#include "FFontManager.h"
#include "FComponentVisualizer.h"
#include "FAssetManager.h"
#include "FTextBuilder.h"
#include "FDuplicatedDataRW.h"
#include "FHiZOcclusionManager.h"
#include "FInstrumentor.h"
#include "FEditorUIManager.h"

#if IS_OBJ_VIEWER
#include "FObjViewer.h"
#endif

// NOTE: 현재 Occlusion이 제대로 동작하지 않을 확률이 높아서 임시로 비활성화. (어떻게 만든거야...)
#define ENABLE_OCCULSION_CULLING 0

FEditorEngine GEditor;

void FEditorEngine::Initialize(HINSTANCE hInstance, WNDPROC WndProc)
{
	FEngine::Initialize(hInstance, WndProc);

	URenderer* Renderer = mGraphicsManager->GetRenderer();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)mHWnd);
	ImGui_ImplDX11_Init(Renderer->GetDevice(), Renderer->GetDeviceContext());
	auto& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/malgun.ttf",
		18.0f,
		nullptr,
		IO.Fonts->GetGlyphRangesKorean()
	);

	mEditorLayout.Initialize(FRect(0, 0, (float)Renderer->GetWidth(), (float)Renderer->GetHeight()));

	static constexpr EViewportType DefaultLayoutTypes[FEditorEngine::MaxViewportCount] = {
		EViewportType::Perspective,
		EViewportType::Top,
		EViewportType::Front,
		EViewportType::Side
	};

	for (int32 i = 0; i < FEditorEngine::MaxViewportCount; ++i)
	{
		mViewports[i].Window = (i == MainViewportIndex) ? mEditorLayout.RootWindow : mEditorLayout.ViewportWindows[i];
		mViewports[i].Viewport = MakeShared<FViewport>();
		mViewports[i].Viewport->Resize(*mGraphicsManager->GetRenderer(), mEditorLayout.ViewportWindows[i]->Rect.Width, mEditorLayout.ViewportWindows[i]->Rect.Height);
		mViewports[i].Client = MakeShared<FEditorViewportClient>(*mGraphicsManager->GetRenderer());
		mViewports[i].Client->SetViewportType(DefaultLayoutTypes[i]);
	}

	mComponentVisualizerManager = new FComponentVisualizerManager();

	mEditorUIManager = new FEditorUIManager(*Renderer);

	InitAssetManager();

	LoadEditorSettings();

	/* Console Window */
	ConsoleWindow& console = ConsoleWindow::Get();
	console.Init(Renderer->GetWidth());

	CreateNewMapForEditing();

#ifdef IS_OBJ_VIEWER
	mObjViewer.Initialize(GEditor, *mGraphicsManager->GetRenderer(), *mFileManager);
#endif
}

void FEditorEngine::InitAssetManager()
{
	PROFILE_FUNCTION();

	URenderer* renderer = mGraphicsManager->GetRenderer();

	FAssetManager::Get().ScanDirectory("BuiltInAssets", *renderer);
	FAssetManager::Get().ScanDirectory("Assets", *renderer);

	// Register built-in asset types
	TSharedPtr<FStaticMeshAsset> cubeAsset = MakeShared<FStaticMeshAsset>(BuiltInAssetID::CubeMesh, FName("CubeMesh"), *renderer, Cube_vertices, sizeof(Cube_vertices) / sizeof(FVertex), Cube_indices, sizeof(Cube_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(cubeAsset);

	TSharedPtr<FStaticMeshAsset> sphereAsset = MakeShared<FStaticMeshAsset>(BuiltInAssetID::SphereMesh, FName("SphereMesh"), *renderer, Sphere_vertices, sizeof(Sphere_vertices) / sizeof(FVertex), Sphere_indices, sizeof(Sphere_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(sphereAsset);

	TSharedPtr<FStaticMeshAsset> circleAsset = MakeShared<FStaticMeshAsset>(BuiltInAssetID::CircleMesh, FName("CircleMesh"), *renderer, Circle_vertices, sizeof(Circle_vertices) / sizeof(FVertex), Circle_indices, sizeof(Circle_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(circleAsset);

	TSharedPtr<FStaticMeshAsset> triangleAsset = MakeShared<FStaticMeshAsset>(BuiltInAssetID::TriangleMesh, FName("TriangleMesh"), *renderer, Triangle_vertices, sizeof(Triangle_vertices) / sizeof(FVertex), Triangle_indices, sizeof(Triangle_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(triangleAsset);

	TSharedPtr<FStaticMeshAsset> gizmoArrowAsset = MakeShared<FStaticMeshAsset>(BuiltInAssetID::GizmoArrowMesh, FName("GizmoArrowMesh"), *renderer, GizmoArrow_vertices, sizeof(GizmoArrow_vertices) / sizeof(FVertex), GizmoArrow_indices, sizeof(GizmoArrow_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(gizmoArrowAsset);

	TSharedPtr<FStaticMeshAsset> PlaneAsset = MakeShared<FStaticMeshAsset>(BuiltInAssetID::PlaneMesh, FName("PlaneMesh"), *renderer, Plane_vertices, sizeof(Plane_vertices) / sizeof(FVertex), Plane_indices, sizeof(Plane_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(PlaneAsset);

	TSharedPtr<FTexture2DAssetLoader> TextureLoader = MakeShared<FTexture2DAssetLoader>(*renderer);
	TSharedPtr<FFontAssetLoader> FontLoader = MakeShared<FFontAssetLoader>(*mFontManager);

	{
		// NOTE: 이 부분은 임시로 ExplosionSpriteAtlas를 고정 Guid로 등록하는 코드이므로, 후에 스프라이트 아틀라스 에셋을 만드는 기능이 나오면 제거해야할 코드임.
		TSharedPtr<FTexture2DAsset> ExplosionTexture2DAsset = mAssetManager->GetAssetAs<FTexture2DAsset>(BuiltInAssetID::ExplosionTexture, true);
		if (ExplosionTexture2DAsset)
		{
			TSharedPtr<FSpriteAtlasAsset> ExplosionSpriteAtlasAsset = MakeShared<FSpriteAtlasAsset>(BuiltInAssetID::ExplosionSpriteAtlas, FName("ExplosionSpriteAtlas"), *renderer, ExplosionTexture2DAsset, 6, 6);
			mAssetManager->RegisterAsset(ExplosionSpriteAtlasAsset);
		}
	}

	// ScanDirectory로 파일 자동 스캔하여 uasset 등록하므로 아래 줄과 중복되어 삭제해도 되나,
	// 참고하고 있는 곳이 있어서 ScanDirectory와 동일한 파일명 규칙으로 수정해 둠.

	TSharedPtr<FFileAssetSource> FontAssetSource = MakeShared<FFileAssetSource>("Assets/Fonts/HMKMRHD.ttf");
	mAssetManager->RegisterAsset(FGuid::NewGuid(), FName("TestFont"), FontLoader, FontAssetSource);

	TSharedPtr<FFontAsset> TestFontAsset = mAssetManager->GetAssetAs<FFontAsset>(FName("TestFont"), true);
	TSharedPtr<FFontAtlasAsset> FontAtlasAsset = MakeShared<FFontAtlasAsset>(FGuid::NewGuid(), FName("TestFontAtlas"), *renderer, TestFontAsset, 512, 512, 2, 2);
	mAssetManager->RegisterAsset(FontAtlasAsset);
}

void FEditorEngine::OnCreateWorldContext(FWorldContext& NewContext)
{
	if (NewContext.mWorldType == EWorldType::Editor)
	{
		for (int32 i = 0; i < mWorldContexts.Num(); ++i)
		{
			if (&mWorldContexts[i] == &NewContext)
			{
				mEditorWorldContextIndex = i;
				return;
			}
		}
	}
}

void FEditorEngine::OnDestroyWorldContext(FWorldContext& Context)
{
	if (Context.mWorldType == EWorldType::Editor)
	{
		mEditorWorldContextIndex = -1;
	}
}

void FEditorEngine::Cleanup()
{
	SaveEditorSettings();

	delete mComponentVisualizerManager;
	delete mEditorUIManager;

	for (FEditorViewport& Viewport : mViewports)
	{
		Viewport.Release();
	}

	for (FWorldContext& context : mWorldContexts)
	{
		if (context.mWorld != nullptr)
		{
			FObjectFactory::DestroyObject(context.mWorld);
			context.mWorld = nullptr;
		}
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	FEngine::Cleanup();
}

FWorldContext& FEditorEngine::GetEditorWorldContext()
{
	if (mEditorWorldContextIndex == -1)
	{
		throw std::runtime_error("No editor world context found.");
	}

	return mWorldContexts[mEditorWorldContextIndex];
}

FWorldContext* FEditorEngine::GetPIEWorldContext()
{
	for (FWorldContext& context : mWorldContexts)
	{
		if (context.mWorldType == EWorldType::PIE)
		{
			return &context;
		}
	}

	return nullptr;
}

void FEditorEngine::Tick(float DeltaTime)
{
	ProcessPendingTasks();

	if (WindowApplication.bPendingResize)
	{
		mGraphicsManager->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
		WindowApplication.bPendingResize = false;
	}

	for (FWorldContext& Context : mWorldContexts)
	{
		UWorld* World = Context.mWorld;

		if (World == nullptr)
		{
			continue;
		}

		if (Context.mWorldType == EWorldType::Editor)
		{
			// 분할 화면은 첫 뷰, 단일 화면은 최대화된 뷰를 모든 메시의 LOD 기준으로 사용합니다.
			const int32 LODViewportIndex = mEditorLayout.bIsSplitView ? 0 : mEditorLayout.MaximizedViewportIndex;
			World->SetLODViewOrigin(mViewports[LODViewportIndex].Client->GetCamera().Transform.GetLocation());

#if ENABLE_OCCULSION_CULLING
			// Check if World AABBs are dirty and update GPU StructuredBuffer
			if (World->IsAABBsDirty())
			{
				FHiZOcclusionManager::Get().UpdateAABBs(
					mGraphicsManager->GetRenderer()->GetDevice(),
					mGraphicsManager->GetRenderer()->GetDeviceContext(),
					World->GetCachedEntryAABBs()
				);
				World->SetAABBsClean();
			}
#endif

			// TODO: 에디터 안에서 돌아야할 Tick만 호출할것. 현재는 별 다른 차이없음.
			World->Tick(DeltaTime);
		}
		else if (Context.mWorldType == EWorldType::PIE)
		{
			World->Tick(DeltaTime);
		}
	}
}

void FEditorEngine::Render(float DeltaTime)
{
	const bool bIsSplit = mEditorLayout.bIsSplitView;
	const int32 ActiveIndex = mEditorLayout.MaximizedViewportIndex;
	const int32 ViewportCount = mEditorLayout.bIsSplitView ? 4 : 1;

	if (bIsSplit)
	{
		for (int32 i = 0;i < 4;++i)
		{
			mViewports[i].Window = mEditorLayout.ViewportWindows[i];
		}
	}
	else
	{
		mViewports[ActiveIndex].Window = mEditorLayout.RootWindow;
	}

	const float NearZ = 0.1f;
	const float FarZ = 2000.0f;

	if (mGraphicsManager->GetRenderer())
	{
		PROFILE_SCOPE("Frame/BeginFrame");
		mGraphicsManager->GetRenderer()->BeginFrame();
	}

	{
		PROFILE_SCOPE("Frame/ProjectionTransition");
		mGraphicsManager->UpdateProjectionTransition(DeltaTime);
	}

	mGraphicsManager->UpdateGpuRenderTime();
	if (ConsoleWindow::Get().bShowStatRender)
	{
		mGraphicsManager->BeginGpuRenderTimer();
	}

	mGraphicsManager->GetRenderer()->ResetDrawCallCount();

#if ENABLE_OCCULSION_CULLING
	// Begin frame: read back previous frame's GPU culling results (zero-stall)
	FHiZOcclusionManager::Get().BeginFrame(mGraphicsManager->GetRenderer()->GetDeviceContext());
#endif

	{
		PROFILE_SCOPE("Frame/Viewports");

		FRenderCollector& RenderCollector = mGraphicsManager->GetRenderCollector();

		for (int32 i = 0; i < ViewportCount; ++i)
		{
			PROFILE_SCOPE("Viewport");
			int32 CurrentIndex = bIsSplit ? i : ActiveIndex;
			FEditorViewport* CurrentViewport = &mViewports[CurrentIndex];

			const FRect& ViewportRect = CurrentViewport->Window->Rect;

			UWorld* World = CurrentViewport->Client->GetWorld();

			FCamera& Camera = CurrentViewport->Client->GetCamera();
			Camera.mAspect = ViewportRect.Width / ViewportRect.Height;
			Camera.mNear = NearZ;
			Camera.mFar = FarZ;

			RenderCollector.Clear();
			RenderCollector.Camera = &Camera;

			bool bIsOrtho = CurrentViewport->Client->IsOrtho();
			float CurrentRatio = bIsOrtho ? 0.0f : mGraphicsManager->GetPerspectiveRatio();

			// 뷰포트가 직교 타입이면 현재 -5000 ~ 5000으로 보이게 하드코딩, 나중에 카메라 위치에 따라 랜더 거리를 늘려야 함
			Camera.mNear = bIsOrtho ? -5000.0f : Camera.mNear;
			Camera.mFar = bIsOrtho ? 5000.0f : Camera.mFar;

			{
				PROFILE_SCOPE("Viewport/Update");
				CurrentViewport->Client->Update(DeltaTime, CurrentRatio, RenderCollector);
			}

			FMatrix ViewProjection = Camera.GetViewMatrix() * Camera.GetUnifiedProjectionMatrix(Camera.mOrthoDistance, CurrentRatio);
			FMatrix InvViewProjection = Camera.GetInverseUnifiedProjectionMatrix(Camera.mOrthoDistance, CurrentRatio) * Camera.GetViewMatrix().AffineInverse();
			RenderCollector.Frustum = FFrustum::Create(ViewProjection);

			const FInputState& Input = WindowApplication.Input;
			bool bIsAssetDragging = (ImGui::GetDragDropPayload() != nullptr);
			RenderCollector.bNeedPickTargets = CurrentViewport->Client->IsActive() && Input.WasPressed(VK_LBUTTON) && !CurrentViewport->Client->mGizmo.IsDragging() && !CurrentViewport->Client->mGizmo.IsMouseOverHandle() && !bIsAssetDragging;

			{
				PROFILE_SCOPE("Viewport/Collect");
				World->Render(DeltaTime, RenderCollector);
			}

			// 마우스 피킹 처리
			// 뷰포트가 ImGui 창이 되면서 그 위에서는 io.WantCaptureMouse 가 항상 true 다.
			// 그대로 두면 씬을 클릭해도 선택이 되지 않는다. 카메라/기즈모와 같은 기준을 쓴다.
			{
				if (RenderCollector.bNeedPickTargets)
				{
					UActorComponent* HitComponent = nullptr;
					{
						PROFILE_SCOPE("MousePicking");
						HitComponent = CurrentViewport->Client->PerformMousePicking(CurrentViewport->Window->Rect, CurrentRatio, RenderCollector);
					}

					if (HitComponent)
					{
						GEditor.SetSelectedComponent(HitComponent);
					}
					else
					{
						GEditor.ResetSelectedComponent();
					}
				}
			}

			// 선택된 액터 처리
			TArray<UPrimitiveComponent*> HighlightedComponents;

			UActorComponent* SelectedComponent = GEditor.GetSelectedComponent();
			{
				PROFILE_SCOPE("Viewport/SelectionAndGizmo");
				if (SelectedComponent)
				{
					for (UActorComponent* Component : SelectedComponent->GetOwner()->GetComponents())
					{
						UPrimitiveComponent* PrimitiveComponent = Component->Cast<UPrimitiveComponent>();
						if (PrimitiveComponent)
						{
							// 선택된 액터의 AABB를 화면에 표시
							const FAABB& AABB = PrimitiveComponent->GetBoundingBox();

							AABB.ForEachCornerLines([&RenderCollector](const FVector& Start, const FVector& End) {
								FVector4 WorldStart = FVector4(Start, 1.f);
								FVector4 WorldEnd = FVector4(End, 1.f);

								FRenderLineInfo LineInfo;
								LineInfo.Start = WorldStart.ToVec3();
								LineInfo.End = WorldEnd.ToVec3();
								LineInfo.Color = FVector4(1.f, 0.f, 0.f, 1.f); // 빨간색
								LineInfo.Thickness = 5.0f;

								RenderCollector.LineInfos.Add(LineInfo);
							});

							HighlightedComponents.Add(PrimitiveComponent);
						}

						// 선택된 액터의 컴포넌트 시각화
						FComponentVisualizer* Visualizer = mComponentVisualizerManager->FindVisualizer(Component->GetClass());
						if (Visualizer)
						{
							Visualizer->VisualizeComponent(Component, RenderCollector);
						}
					}

					CurrentViewport->Client->mGizmo.Tick(SelectedComponent, CurrentViewport->Window->Rect, CurrentViewport->Client->IsActive(), InvViewProjection);
				}
			}

			//Render Threads
			{
				PROFILE_SCOPE("Viewport/Render");
				CurrentViewport->Viewport->Resize(*mGraphicsManager->GetRenderer(), ViewportRect.Width, ViewportRect.Height);
				mGraphicsManager->Prepare(&CurrentViewport->Client->mCamera, ViewportRect.Width, ViewportRect.Height, *CurrentViewport->Viewport, CurrentViewport->Client->GetViewMode(), CurrentViewport->Client->GetViewportType());
				mGraphicsManager->RenderHighLight(HighlightedComponents);
				mGraphicsManager->Render();

				CurrentViewport->Client->mGizmo.Render(SelectedComponent, CurrentViewport->Client->mCamera.Transform.GetLocation(), CurrentViewport->Window->Rect, ViewProjection, CurrentViewport->Client->IsOrtho(), CurrentViewport->Client->GetCamera().mOrthoDistance);
			}
		}
	}

	mGraphicsManager->EndGpuRenderTimer();

	FGuiReference GuiReference;
	GuiReference.WorldContext = mbIsPlayingInEditor ? GEditor.GetPIEWorldContext() : &GEditor.GetEditorWorldContext();
	GuiReference.SceneManager = &GEditor;
	GuiReference.GraphicsManager = mGraphicsManager;
	GuiReference.FileManager = mFileManager;
	GuiReference.AssetManager = mAssetManager;
	GuiReference.EditorLayout = &mEditorLayout;
	if (bIsSplit)
	{
		GuiReference.EditorCamera = &GetMainViewport().Client->GetCamera();
		GuiReference.ViewportClient = GetMainViewport().Client.get();
		GuiReference.Viewports = mViewports;
		GuiReference.ViewportCount = MaxViewportCount;
	}
	else
	{
		GuiReference.EditorCamera = &mViewports[MainViewportIndex].Client->GetCamera();
		GuiReference.ViewportClient = mViewports[ActiveIndex].Client.get();
		GuiReference.Viewports = &mViewports[ActiveIndex];
		GuiReference.ViewportCount = 1;
	}

	{
		PROFILE_SCOPE("Frame/EditorUI");
		mEditorUIManager->Render(GuiReference);

#if IS_OBJ_VIEWER
		mObjViewer.UpdateObjGUI(*mGraphicsManager);
#endif
		// 나중에 Ui 매니저에서 관리하도록 분리 필요
		ImGui::SetMouseCursor(mMouseCursor);

		FRect ViewportRect;
		ViewportRect.X = mEditorUIManager->GetViewportX();
		ViewportRect.Y = mEditorUIManager->GetViewportY();
		ViewportRect.Width = mEditorUIManager->GetViewportWidth();
		ViewportRect.Height = mEditorUIManager->GetViewportHeight();
		mEditorLayout.Resize(ViewportRect);
	}

	{
		PROFILE_SCOPE("Frame/ImGuiSubmit");
		mGraphicsManager->GetRenderer()->BindFrameBuffer();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	{
		PROFILE_SCOPE("Frame/Present");
		mGraphicsManager->Display();
	}
}

void FEditorEngine::SetSelectedComponent(UActorComponent* component)
{
	if (component == nullptr)
	{
		UE_LOG_WARN("SetSelectedComponent: Attempted to set selected component to nullptr.");
		return;
	}

	if (component == mSelectedComponent)
	{
		UE_LOG_WARN("SetSelectedComponent: Component with UUID %d is already selected.", component->UUID);
		return; // No change
	}

	UE_LOG_WARN("SetSelectedComponent: Component with UUID %d is now selected.", component->UUID);
	mSelectedComponent = component;
}

void FEditorEngine::SaveEditorSettings()
{
	PROFILE_FUNCTION();

	// Save camera sensitivity
	const std::string Value = std::format("{:.6f}", GetMainViewport().Client->GetCamera().Sensitivity);
	if (!WritePrivateProfileStringA("Camera", "Sensitivity", Value.c_str(), ".\\editor.ini"))
	{
		UE_LOG_ERROR("Failed to save camera sensitivity to editor.ini");
	}

	// Save grid gap
	const std::string ValueGrid = std::format("{:6d}", mGraphicsManager->GetGridGap());
	if (!WritePrivateProfileStringA("Grid", "Gap", ValueGrid.c_str(), ".\\editor.ini"))
	{
		UE_LOG_ERROR("Failed to save grid gap to editor.ini");
	}

	// Save split infos
	const std::string ValueH = std::format("{:.6f}", mEditorLayout.HSplitter->SplitterRatio);
	if (!WritePrivateProfileStringA("Split", "HorizontalRatio", ValueH.c_str(), ".\\editor.ini"))
	{
		UE_LOG_ERROR("Failed to save horizontal split ratio to editor.ini");
	}

	const std::string ValueV = std::format("{:.6f}", mEditorLayout.VSplitter[0]->SplitterRatio);
	if (!WritePrivateProfileStringA("Split", "VerticalRatio", ValueV.c_str(), ".\\editor.ini"))
	{
		UE_LOG_ERROR("Failed to save vertical split ratio to editor.ini");
	}

	WritePrivateProfileStringA("Layout", "IsSplitView", mEditorLayout.bIsSplitView ? "1" : "0", ".\\editor.ini");
	WritePrivateProfileStringA("Layout", "MaximizedIndex", std::to_string(mEditorLayout.MaximizedViewportIndex).c_str(), ".\\editor.ini");

	for (int32 i = 0; i < MaxViewportCount; ++i)
	{
		std::string ViewTypeKey = "ViewportType_" + std::to_string(i);
		std::string ViewTypeVal = std::to_string(static_cast<int32>(mViewports[i].Client->GetViewportType()));
		WritePrivateProfileStringA("Layout", ViewTypeKey.c_str(), ViewTypeVal.c_str(), ".\\editor.ini");

		std::string ViewModeKey = "ViewportViewMode_" + std::to_string(i);
		std::string ViewModeVal = std::to_string(static_cast<int32>(mViewports[i].Client->GetViewMode()));
		WritePrivateProfileStringA("Layout", ViewModeKey.c_str(), ViewModeVal.c_str(), ".\\editor.ini");
	}
}

void FEditorEngine::LoadEditorSettings()
{
	PROFILE_FUNCTION();

	char Value[64] = {};

	// Load camera sensitivity
	GetPrivateProfileStringA("Camera", "Sensitivity", "", Value, sizeof(Value), ".\\editor.ini");

	float Sensitivity = 1.0f;
	sscanf_s(Value, "%f", &Sensitivity);
	GetMainViewport().Client->GetCamera().Sensitivity = Sensitivity;

	// Load grid gap
	GetPrivateProfileStringA("Grid", "Gap", "", Value, sizeof(Value), ".\\editor.ini");

	int32 GridGap = 1;
	sscanf_s(Value, "%d", &GridGap);
	mGraphicsManager->SetGridGap(GridGap);

	// Load split infos
	GetPrivateProfileStringA("Split", "HorizontalRatio", "", Value, sizeof(Value), ".\\editor.ini");

	float HorizontalRatio = 0.5f;
	sscanf_s(Value, "%f", &HorizontalRatio);

	GetPrivateProfileStringA("Split", "VerticalRatio", "", Value, sizeof(Value), ".\\editor.ini");

	float VerticalRatio = 0.5f;
	sscanf_s(Value, "%f", &VerticalRatio);

	mEditorLayout.SetSplitRatios(HorizontalRatio, VerticalRatio);

	mEditorLayout.bIsSplitView = (GetPrivateProfileIntA("Layout", "IsSplitView", 0, ".\\editor.ini") == 1);
	mEditorLayout.MaximizedViewportIndex = GetPrivateProfileIntA("Layout", "MaximizedIndex", 0, ".\\editor.ini");

	for (int32 i = 0; i < MaxViewportCount; ++i)
	{
		std::string ViewTypeKey = "ViewportType_" + std::to_string(i);
		int32 ViewTypeVal = GetPrivateProfileIntA("Layout", ViewTypeKey.c_str(), -1, ".\\editor.ini");
		if (ViewTypeVal >= 0 && ViewTypeVal < static_cast<int32>(EViewportType::Max))
		{
			mViewports[i].Client->SetViewportType(static_cast<EViewportType>(ViewTypeVal));
		}

		std::string ViewModeKey = "ViewportViewMode_" + std::to_string(i);
		int32 ViewModeVal = GetPrivateProfileIntA("Layout", ViewModeKey.c_str(), -1, ".\\editor.ini");
		if (ViewModeVal >= 0 && ViewModeVal < static_cast<int32>(EViewModeIndex::VMI_Max))
		{
			mViewports[i].Client->SetViewMode(static_cast<EViewModeIndex>(ViewModeVal));
		}
	}
}

void FEditorEngine::CreateNewMapForEditing()
{
	if (mbIsPlayingInEditor)
	{
		UE_LOG_WARN("CreateNewMapForEditing: Cannot create a new map while in Play-In-Editor mode.");
		return;
	}

	EnqueuePendingTask([this] {
		FWorldContext& EditorWorldContext = CreateNewWorldContext(EWorldType::Editor);
		UWorld* EditorWorld = UWorld::CreateWorld(EWorldType::Editor);
		EditorWorldContext.SetCurrentWorld(EditorWorld);

		for (FEditorViewport& Viewport : mViewports)
		{
			Viewport.Client->SetWorld(EditorWorld);
		}
	});
}

void FEditorEngine::StartPIE()
{
	if (mbIsPlayingInEditor)
	{
		UE_LOG_WARN("StartPIE: Already in Play-In-Editor mode.");
		return;
	}

	EnqueuePendingTask([this] {
		UWorld* EditorWorld = GetEditorWorldContext().World();

		UWorld* PIEWorld = UWorld::DuplicateWorldForPIE(EditorWorld);

		FWorldContext& NewWorldContext = CreateNewWorldContext(EWorldType::PIE);
		NewWorldContext.SetCurrentWorld(PIEWorld);

		for (FEditorViewport& Viewport : mViewports)
		{
			Viewport.Client->SetWorld(PIEWorld);
		}

		// TODO: PIE 모드에서 필요한 초기화 작업을 수행합니다.

		mbIsPlayingInEditor = true;
	});
}

void FEditorEngine::EndPIE()
{
	if (!mbIsPlayingInEditor)
	{
		UE_LOG_WARN("EndPIE: Not currently in Play-In-Editor mode.");
		return;
	}

	EnqueuePendingTask([this] {
		FWorldContext* Context = GetPIEWorldContext();
		if (!Context)
		{
			return;
		}

		// TODO: PIE 모드에서 필요한 정리 작업을 수행합니다.

		DestroyWorldContext(Context);

		FWorldContext& EditorWorldContext = GetEditorWorldContext();
		for (FEditorViewport& Viewport : mViewports)
		{
			Viewport.Client->SetWorld(EditorWorldContext.World());
		}

		mbIsPlayingInEditor = false;
	});
}

void SaveMap(UWorld* World, FCamera* Camera, const std::filesystem::path& scenePath, const FFileManager& fileManager)
{
	uint32 version = 0;

	// 기존 파일이 있으면 Version을 유지한다.
	try
	{
		const FString previousSceneString =
			fileManager.ReadFileToString(scenePath);

		const json::JSON previousSceneJson =
			json::JSON::Load(previousSceneString);

		if (previousSceneJson.hasKey("Version") &&
			previousSceneJson.at("Version").JSONType() ==
			json::JSON::Class::Integral)
		{
			version =
				previousSceneJson.at("Version").ToInt();
		}
	}
	catch (const std::exception&)
	{
		// 새로 저장하는 파일이면 Version 0부터 시작한다.
		version = 0;
	}

	json::JSON WorldJson = json::JSON::Make(json::JSON::Class::Object);
	World->SerializeClass(WorldJson);

	json::JSON sceneJson = json::JSON::Make(json::JSON::Class::Object);
	sceneJson["Version"] = version;
	sceneJson["NextUUID"] = UEngineStatics::GetNextUUID();
	sceneJson["World"] = WorldJson;

	json::JSON& PerspectiveCameraJson = sceneJson["PerspectiveCamera"];
	PerspectiveCameraJson["Location"] = JsonUtils::ToJson(Camera->Transform.GetLocation());
	PerspectiveCameraJson["Rotation"] = JsonUtils::ToJson(ToEulerAngles(Camera->Transform.GetRotation()));
	PerspectiveCameraJson["FOV"] = Camera->mFovDegree;
	PerspectiveCameraJson["Near"] = Camera->mNear;
	PerspectiveCameraJson["Far"] = Camera->mFar;

	const FString jsonString(sceneJson.dump(1, "  "));

	fileManager.WriteStringToFile(scenePath, jsonString);
}

void LoadMap(FWorldContext& WorldContext, FCamera* Camera, const std::filesystem::path& scenePath, const FFileManager& fileManager)
{
	if (WorldContext.World() == nullptr)
	{
		throw std::runtime_error("World context does not have a valid world.");
	}

	const FString jsonString = fileManager.ReadFileToString(scenePath);

	const json::JSON sceneJson = json::JSON::Load(jsonString);

	if (!sceneJson.hasKey("NextUUID") || sceneJson.at("NextUUID").JSONType() != json::JSON::Class::Integral)
	{
		throw std::runtime_error(std::format("Scene file '{}' does not contain valid NextUUID data.", scenePath.string()));
	}

	if (!sceneJson.hasKey("World") || sceneJson.at("World").JSONType() != json::JSON::Class::Object)
	{
		throw std::runtime_error(std::format("Scene file '{}' does not contain valid World data.", scenePath.string()));
	}

	const uint32 nextUUID = sceneJson.at("NextUUID").ToInt();
	UEngineStatics::SetNextUUID(nextUUID);

	const json::JSON WorldJson = sceneJson.at("World");

	WorldContext.World()->DeserializeClass(WorldJson);

	json::JSON PerspectiveCameraJson = sceneJson.at("PerspectiveCamera");
	Camera->Transform.SetLocation(JsonUtils::FromJson<FVector>(PerspectiveCameraJson.at("Location")));
	Camera->Transform.SetRotation(JsonUtils::FromJson<FRotator>(PerspectiveCameraJson.at("Rotation")));
	Camera->mFovDegree = PerspectiveCameraJson.at("FOV").ToFloat();
	Camera->mNear = PerspectiveCameraJson.at("Near").ToFloat();
	Camera->mFar = PerspectiveCameraJson.at("Far").ToFloat();
}

