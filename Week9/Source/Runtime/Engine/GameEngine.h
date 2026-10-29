#pragma once

#include "Engine.h"
#include "Render/SkyboxRenderer.h"
#include "Text/TextRenderer.h"
#include "UI/ScreenQuadRenderer.h"
#include "UI/ScreenUIRenderer.h"

class FRenderer;
class FGameViewportClient;
class FForwardGameViewportClient;
class FSceneRenderer;
struct FSceneView;

// 에디터 없이 씬 하나를 바로 플레이하는 게임 실행 엔진.
// 에디터 PIE와 같은 경로(씬 로드 → 월드 복제 → BeginPlay)로 세션을 만들고,
// 플레이어 화면마다 장면과 게임 UI를 그린 뒤 백버퍼에 나란히 옮긴다.
class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine, UEngine)
public:
	FEngineConfig GetConfig() const override;

	virtual bool Init() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PreExit() override;

protected:
	// 게임 세션 없이 UGameEngine의 창 설정만 쓰는 서브클래스(Benchmark)는 false를 반환한다.
	virtual bool ShouldRunGameSession() const { return true; }

private:
	bool StartGameSession();
	void StopGameSession();
	FGameViewportClient* GetGameClient() const;

	void RenderGame();
	void RenderPlayerView(FForwardGameViewportClient* Client, const FSceneView& SceneView, FSceneRenderer& SceneRenderer,
		int32 SplitIndex, int32 SplitCount);
	void RenderSceneText(UWorld* TargetWorld, const FSceneView& SceneView);

	FString StartupScenePath;

	FRenderer* Renderer = nullptr;
	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	FScreenUIRenderer ScreenUIRenderer;
	FScreenQuadRenderer PresentRenderer;
};
