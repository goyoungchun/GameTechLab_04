#include "EnginePCH.h"
#include "GameEngine.h"

#include "Launch/LaunchEngineLoop.h"
#include "Core/Window.h"
#include "Component/TextRenderComponent.h"
#include "Engine/ForwardGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"
#include "Serialization/JsonArchive.h"
#include "UObject/UObjectIterator.h"

#include <filesystem>
#include <fstream>

namespace
{
	constexpr const char* DefaultScenePath = "Scenes/RocketArena.scene";
	constexpr const char* SkyPanoramaPath = "Assets/SkySphere/Sky.jpg";
	constexpr const char* LogFilePath = "Saved/Logs/HitoriGame.log";

	// 게임 빌드에는 로그 패널이 없으므로 Lua 에러 등을 파일과 디버거 출력으로 남긴다.
	class FGameLogSink : public ILogSink
	{
	public:
		FGameLogSink()
		{
			std::error_code ErrorCode;
			std::filesystem::create_directories(std::filesystem::path(LogFilePath).parent_path(), ErrorCode);
			File.open(LogFilePath, std::ios::out | std::ios::trunc);
		}

		void OnLog(ELogVerbosity Verbosity, const FString& Message) override
		{
			const char* Prefix = Verbosity == ELogVerbosity::Error ? "[Error] "
				: Verbosity == ELogVerbosity::Warning ? "[Warning] " : "";
			const FString Line = Prefix + Message + "\n";
			OutputDebugStringA(Line.c_str());
			if (File.is_open())
			{
				File << Line;
				File.flush();
			}
		}

	private:
		std::ofstream File;
	};

	FGameLogSink& GetGameLogSink()
	{
		static FGameLogSink Sink;
		return Sink;
	}
}

FEngineConfig UGameEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori";
	Desc.Width = 0;
	Desc.Height = 0;
	Desc.bBorderless = true;
	Desc.SyncInterval = 0;   // 0 = VSync 끔 (FPS 측정용)
	Desc.bCreateDepthBuffer = true;
	Desc.bExitOnEscape = true;
	if (!ShouldRunGameSession())
		return Desc;

	Desc.Title = L"Jungle League";
	Desc.SyncInterval = 1;
	// 장면은 플레이어 화면마다 자기 깊이 버퍼에 그리고, 백버퍼에는 결과 이미지만 옮긴다.
	Desc.bCreateDepthBuffer = false;
	Desc.SplashImage = "Resources/Splash.png";
	return Desc;
}

bool UGameEngine::Init()
{
	if (!Super::Init()) return false;
	if (!ShouldRunGameSession()) return true;

	FLog::AddSink(&GetGameLogSink());
	Renderer = GetEngineLoop().GetRenderer();

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init(SkyPanoramaPath);
	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();
	ScreenUIRenderer.Init();
	PresentRenderer.Init(16);

	Renderer->SetShadowEnabled(true);
	Renderer->SetShadowFilterMode(EShadowFilterMode::PCF5x5);
	FShadowRenderer& ShadowRenderer = Renderer->GetShadowRenderer();
	ShadowRenderer.SetShadowEnabled(true);
	ShadowRenderer.SetCSMEnabled(true);
	ShadowRenderer.SetCacheEnabled(true);

	// 실행 인자로 씬 경로를 받으면 그 씬을, 없으면 RocketArena를 연다. 예: HitoriGame.exe Scenes/Test.scene
	StartupScenePath = DefaultScenePath;
	if (__argc > 1 && __argv[1] && __argv[1][0] != '\0')
		StartupScenePath = __argv[1];

	if (!FJsonArchive::LoadWorld(EditorWorld, StartupScenePath))
	{
		HTR_LOG(Error, "Game: Failed to load scene {}", StartupScenePath);
		return false;
	}
	return StartGameSession();
}

void UGameEngine::Tick(float DeltaTime)
{
	if (!ShouldRunGameSession())
		return;

	if (ConsumeQuitGameRequest())
	{
		GetEngineLoop().RequestExit();
		return;
	}
	if (ConsumeRestartLevelRequest())
	{
		StopGameSession();
		if (!StartGameSession())
		{
			GetEngineLoop().RequestExit();
			return;
		}
	}

	if (PlayWorld)
		PlayWorld->Tick(EWorldTick::All, DeltaTime);

	RenderGame();
}

void UGameEngine::PreExit()
{
	StopGameSession();
	if (ShouldRunGameSession())
		FLog::RemoveSink(&GetGameLogSink());
	Super::PreExit();
}

// 에디터 PIE와 같은 순서로 세션을 만든다. 씬을 담은 EditorWorld를 복제해 플레이 월드로 쓴다.
bool UGameEngine::StartGameSession()
{
	FWorldContext& Context = CreateNewWorldContext(EWorldType::PIE);
	Context.PIEInstance = 0;

	UGameInstance* GameInstance = FObjectFactory::ConstructObject<UGameInstance>();
	Context.OwningGameInstance = GameInstance;
	if (!GameInstance || !GameInstance->InitializeForPlayInEditor(0) ||
		!GameInstance->StartPlayInEditorGameInstance())
	{
		HTR_LOG(Error, "Game: Failed to start game session");
		StopGameSession();
		return false;
	}

	HTR_LOG(Info, "Game: Started {}", StartupScenePath);
	return true;
}

void UGameEngine::StopGameSession()
{
	PlayWorld = nullptr;
	for (int32 Index = WorldContextlist.Num() - 1; Index >= 0; --Index)
	{
		if (WorldContextlist[Index]->WorldType == EWorldType::PIE)
			DestroyWorldContext(*WorldContextlist[Index]);
	}
}

FGameViewportClient* UGameEngine::GetGameClient() const
{
	for (const TUniquePtr<FWorldContext>& Context : WorldContextlist)
	{
		if (Context->WorldType == EWorldType::PIE && Context->PIEInstance == 0 && Context->GameViewport)
			return Context->GameViewport.get();
	}
	return nullptr;
}

void UGameEngine::RenderGame()
{
	FGameViewportClient* GameClient = GetGameClient();
	FWindow* Window = GetEngineLoop().GetMainWindow();
	if (!GameClient || !PlayWorld || !Window || !Renderer)
		return;

	const uint32 ScreenWidth = Window->GetWidth();
	const uint32 ScreenHeight = Window->GetHeight();
	if (ScreenWidth == 0 || ScreenHeight == 0)
		return;

	GameClient->SyncSplitClients();
	const int32 SplitCount = GameClient->GetSplitCount();
	const FRect ScreenRect{ 0.0f, 0.0f, static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight) };

	// 카메라 시점을 먼저 모두 계산해야 그림자 요청을 한 번에 모을 수 있다.
	TArray<FSceneView> Views;
	TArray<FRect> ViewRects;
	for (int32 Split = 0; Split < SplitCount; ++Split)
	{
		FGameViewportClient* SplitClient = GameClient->GetSplitClient(Split);
		const FRect Rect = FGameViewportClient::GetSplitRect(ScreenRect, Split, SplitCount);
		ViewRects.Add(Rect);
		Views.Add(SplitClient ? SplitClient->CalcSceneView(Rect) : FSceneView{});
	}

	FShadowRenderer& ShadowRenderer = Renderer->GetShadowRenderer();
	ShadowRenderer.BeginFrame();
	FScene& Scene = PlayWorld->GetScene();
	ShadowRenderer.AllocateDirectionalSlots(Scene);
	ShadowRenderer.GatherLocalRequests(Scene, Views);
	ShadowRenderer.RenderLocalAtlas(*Renderer);

	for (int32 Split = 0; Split < SplitCount; ++Split)
	{
		auto* SplitClient = dynamic_cast<FForwardGameViewportClient*>(GameClient->GetSplitClient(Split));
		if (!SplitClient)
			continue;

		FSceneRenderer SceneRenderer(PlayWorld, Views[Split], SplitClient->GetRenderData());
		SceneRenderer.InitViews(Renderer);
		RenderPlayerView(SplitClient, Views[Split], SceneRenderer, Split, SplitCount);
	}

	// 플레이어 화면 이미지를 백버퍼의 자기 영역에 옮긴다.
	GetEngineLoop().BeginBackbufferPass();
	PresentRenderer.BeginFrame(ScreenWidth, ScreenHeight);
	for (int32 Split = 0; Split < SplitCount; ++Split)
	{
		FGameViewportClient* SplitClient = GameClient->GetSplitClient(Split);
		if (!SplitClient || !SplitClient->GetColorTarget())
			continue;
		const FRect& Rect = ViewRects[Split];
		PresentRenderer.AddImage(Rect.X, Rect.Y, Rect.Width, Rect.Height, SplitClient->GetColorTarget(),
			FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	PresentRenderer.Flush();
	GetEngineLoop().EndBackbufferPass();
}

// 에디터의 Forward 게임 화면 경로에서 에디터 전용 효과(안개·FXAA·디버그 뷰)를 뺀 것이다.
void UGameEngine::RenderPlayerView(FForwardGameViewportClient* Client, const FSceneView& SceneView, FSceneRenderer& SceneRenderer,
	int32 SplitIndex, int32 SplitCount)
{
	FForwardViewTargets& Targets = Client->GetViewTargets();
	FTexture2D* ColorTarget = Targets.SceneColor.get();
	FTexture2D* DepthTarget = Targets.Depth.get();
	if (!ColorTarget || !DepthTarget)
		return;
	const uint32 Width = ColorTarget->GetWidth();
	const uint32 Height = ColorTarget->GetHeight();
	if (!Width || !Height || !Targets.IsValidFor(Width, Height))
		return;

	SceneRenderer.RenderDepthPrepass(Renderer, Targets, Width, Height);
	const bool bHasTileLights = SceneRenderer.CullLights(Renderer, Targets, Width, Height);
	SceneRenderer.RenderForwardOpaque(Renderer, Targets, Width, Height, bHasTileLights);

	RenderCommand::BeginRenderPass(ColorTarget, nullptr, Width, Height);
	if (SkyboxRenderer && SceneView.bIsPerspective)
		SkyboxRenderer->OnRender(SceneView.ViewProjectionMatrix, SceneView.ViewLocation);
	SceneRenderer.RenderToneMapForward(Renderer, Targets);
	RenderCommand::EndRenderPass();

	RenderCommand::BindRenderPassNoClear(ColorTarget, DepthTarget, Width, Height);
	SceneRenderer.RenderTranslucent(Renderer, nullptr, bHasTileLights ? &Targets.TileLightResources : nullptr);
	RenderSceneText(PlayWorld, SceneView);

	ScreenUIRenderer.Render(PlayWorld->GetGameUIBuffer(), Width, Height, SplitIndex, SplitCount);
	RenderCommand::EndRenderPass();
}

void UGameEngine::RenderSceneText(UWorld* TargetWorld, const FSceneView& SceneView)
{
	if (!TextRenderer) return;
	for (TObjectIterator<UTextRenderComponent> Text; Text; ++Text)
	{
		if (!Text || !Text->IsRegistered() || !Text->GetFont() || !Text->IsShown(SceneView.bGameView) || !Text->GetOwner() ||
			Text->GetOwner()->GetWorld() != TargetWorld)
			continue;
		TextRenderer->OnRender(Text->GetText(), Text->GetWorldMatrix(), Text->GetTextSize(),
			*Text->GetFont(), SceneView.ViewProjectionMatrix);
	}
}
