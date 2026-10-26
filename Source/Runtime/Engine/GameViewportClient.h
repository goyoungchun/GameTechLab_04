#pragma once

#include "Engine/ViewportClient.h"
#include "Render/SceneView.h"
#include "Render/SceneRenderData.h"

class UWorld;
class UGameInstance;
class UEngine;
class UCameraComponent;
class APawn;

struct FWorldContext;

// 게임 뷰포트 클라이언트 클래스
class FGameViewportClient : public FViewportClient
{
public:
	FGameViewportClient() = default;
	virtual ~FGameViewportClient() override = default;

	// 월드 및 게임 인스턴스 초기화
	void Init(const FWorldContext& InWorldContext, UEngine* InEngine);

	virtual UWorld* GetWorld() const override { return World; }
	void SetWorld(UWorld* InWorld);

	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FViewport* Viewport, int32 Key, bool bDown) override;
	virtual void LostFocus() override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) override;
	bool ConsumeExitRequest() {
		const bool bRequested = bExitRequested;
		bExitRequested = false;
		return bRequested;
	}

	bool ConsumeSIERequest() {
		const bool bRequested = bSIEModeRequested;
		bSIEModeRequested = false;
		return bRequested;
	}

	// 게임 카메라 시점 계산
	FSceneView CalcSceneView(const FRect& InViewRect);

	// 렌더 타깃 및 크기 관리
	virtual void Resize(FViewport* Viewport, uint32 NewWidth, uint32 NewHeight) override { Resize(NewWidth, NewHeight); }
	virtual void Resize(uint32 InWidth, uint32 InHeight) = 0;
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	virtual FTexture2D* GetColorTarget() const = 0;
	virtual FTexture2D* SwapSceneColorAndBind(TUniquePtr<FTexture2D>& Output) = 0;
	virtual FTexture2D* GetDepthTarget() const = 0;
	FSceneRenderData& GetRenderData() { return RenderData; }

	void SetGameInstance(UGameInstance* InGameInstance) { GameInstance = InGameInstance; }
	UGameInstance* GetGameInstance() const { return GameInstance; }

	void SetCameraComponent(UCameraComponent* InCameraComponent) { CameraComponent = InCameraComponent; }

	void Reset();

	bool BeginSIEMode();
	void EndSIEMode();
	void GetKeyInputBySIEMode(int32 Key, bool bDown);
	void GetAxisInputBySIEMode(EGameInputAxis Key, float Delta);
	void TickSIEInput(float DeltaTime, float MoveSpeed, float MouseSensitivity);
	virtual bool IsSIEMode() const { return bSIEMode; }

	void SetPlayerIndex(int32 InIndex) { PlayerIndex = InIndex; }
	int32 GetPlayerIndex() const { return PlayerIndex; }

	// 화면분할
	void SyncSplitClients();

	int GetSplitCount() const { return 1 + static_cast<int32>(SplitClients.Num()); }
	FGameViewportClient* GetSplitClient(int32 Index);
	static FRect GetSplitRect(const FRect& FullRect, int32 Index, int32 SplitCount);



protected:
	// Optional HUD layer: return true when consumed. Game input is forwarded otherwise.
	virtual bool HandleUIKey(int32 Key, bool bDown) { return false; }
	virtual bool HandleUIAxis(EGameInputAxis AxisKey, float Delta) { return false; }
	virtual bool HandleUIMouseMove(int32 X, int32 Y) { return false; }

	virtual void ResetViewTargets() = 0;

	// 멀티뷰포트 용 화면 만들기
	virtual TSharedPtr<FGameViewportClient> CreateSplitClient() const { return nullptr; }

	// 렌더 타깃 버퍼 관련 정보
	uint32 Width = 0;
	uint32 Height = 0;

private:
	UCameraComponent* FindPlayerCamera() const;

	UWorld* World = nullptr;
	UGameInstance* GameInstance = nullptr;
	UEngine* Engine = nullptr;
	UCameraComponent* CameraComponent = nullptr;
	bool bSIEMode = false;
	bool bPreviousMainCameraExternalInputManaged = false;
	bool bSIEKeyDown[256]{};
	float SIEMouseDeltaX = 0.0f;
	float SIEMouseDeltaY = 0.0f;
	float SIEWheelDelta = 0.0f;
	bool bExitRequested = false;
	bool bSIEModeRequested = false;

	FSceneRenderData RenderData;
	

	// 주의: 로컬 멀티플레이 구조가 아니라 실제로 서버가 붙는다면 클라이언트에서 멀티 뷰포트 지원하지 않도록 해야함
	// 14주차까지 이 엔진이 살아있고, 14주차에 서버를 붙이게 된다면 깨닫게 되실겁니다...
	
	// 로켓리그 화면 분할
	int32 PlayerIndex = 0;	// 지금 뷰포트가 담당하는 플레이어 인덱스 (World의 PlayerControllers의 인덱스와 일치함)

	// 2p의 Viewport를 1p가 관리 - 그냥 포인터 1개여도 되긴 하지만 World에서 PlayerController 배열로 해서 구조 맞춤.
	TArray<TSharedPtr<FGameViewportClient>> SplitClients;

	APawn* CachedPawn = nullptr;
};
