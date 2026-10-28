#include "EnginePCH.h"
#include "GameViewportClient.h"

#include "Engine.h"
#include "Camera/ViewportCameraMovement.h"
#include "Math/EngineMath.h"
#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	// Camera view matrices use world location and rotation, independent of scale.
	bool SetCameraWorldPose(UCameraComponent& Camera, const FVector& Location, const FRotator& Rotation)
	{
		FTransform Relative = Camera.GetTransform();
		if (const USceneComponent* Parent = Camera.GetAttachParent())
		{
			const FMatrix ParentWorld = Parent->GetWorldMatrix();
			const float Determinant = ParentWorld.Determinant();
			if (!std::isfinite(Determinant) || Determinant == 0.0f)
				return false;
			Relative.Location = ParentWorld.Inverse().TransformPosition(Location);
			if (!std::isfinite(Relative.Location.X) || !std::isfinite(Relative.Location.Y) ||
				!std::isfinite(Relative.Location.Z)) return false;
			Relative.Rotation = (Parent->GetWorldRotation().Quaternion().Inverse() *
				Rotation.Quaternion()).ToFRotator();
		}
		else
		{
			Relative.Location = Location;
			Relative.Rotation = Rotation;
		}
		Camera.SetTransform(Relative);
		return true;
	}
}

void FGameViewportClient::Init(const FWorldContext& InWorldContext, UEngine* InEngine)
{
	SetWorld(InWorldContext.World());
	GameInstance = InWorldContext.OwningGameInstance;
	Engine = InEngine;
}

void FGameViewportClient::SetWorld(UWorld* InWorld)
{
	if (World == InWorld) return;
	EndSIEMode();
	LostFocus();
	RenderData.ResetScene();
	bExitRequested = bSIEModeRequested = false;
	World = InWorld;
}

void FGameViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	HandleUIMouseMove(X, Y);
}

bool FGameViewportClient::InputKey(FViewport* Viewport, int32 Key, bool bDown)
{
	if (HandleUIKey(Key, bDown))
		return true;
	if (bDown && Key == static_cast<int32>(EKeyCode::Escape))
	{
		bExitRequested = true;
		return true;
	}

	if (bDown && Key == static_cast<int32>(EKeyCode::F8))
	{
		bSIEModeRequested = true;
		return true;
	}


	if (bSIEMode && Key == static_cast<int32>(EKeyCode::LButton)) return true;

	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			PC->InputKey(Key, bDown);
			return true;
		}
	}
	return false;
}

void FGameViewportClient::LostFocus()
{
	std::fill(std::begin(bSIEKeyDown), std::end(bSIEKeyDown), false);
	SIEMouseDeltaX = SIEMouseDeltaY = SIEWheelDelta = 0.0f;
	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
			PC->ClearInputState();
	}
}

bool FGameViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta,
                                    float DeltaTime)
{
	if (HandleUIAxis(AxisKey, Delta))
		return true;

	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			PC->InputAxis(AxisKey, Delta);
			return true;
		}
	}
	return false;
}

FSceneView FGameViewportClient::CalcSceneView(const FRect& InViewRect)
{
	FSceneView OutView;
	OutView.ViewIndex = PlayerIndex;
	OutView.bGameView = !bSIEMode;
	OutView.ViewRect = InViewRect;

	Resize(
		static_cast<uint32>(std::max(InViewRect.Width, 1.0f)),
		static_cast<uint32>(std::max(InViewRect.Height, 1.0f))
	); //여기서 바로 texture들 생성

	const float Aspect = static_cast<float>(Width) / static_cast<float>(Height);

	FMinimalViewInfo DesiredView;
	GetCurrentView(DesiredView);

	DesiredView.AspectRatio = Aspect;
	OutView.ViewLocation = DesiredView.Location;
	OutView.ViewRotation = DesiredView.Rotation;
	OutView.ViewForward = DesiredView.Rotation.Quaternion().GetForwardVector();
	OutView.ViewMatrix = DesiredView.CalculateViewMatrix();
	OutView.ProjectionMatrix = DesiredView.CalculateProjectionMatrix();
	OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;
	OutView.FieldOfView = DesiredView.FOV;
	OutView.AspectRatio = DesiredView.AspectRatio;
	OutView.NearClip = DesiredView.NearClipPlane;
	OutView.FarClip = DesiredView.FarClipPlane;
	OutView.OrthoWidth = DesiredView.OrthoWidth;
	OutView.bIsPerspective = DesiredView.ProjectionMode == ECameraProjectionMode::Perspective;
	OutView.ViewMode = EViewportMode::Lit;
	OutView.ShadingModel = EShadingModel::BlinnPhong;

	// 절두체 평면 계산
	OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);

	return OutView;
}

void FGameViewportClient::Reset()
{
	for (TSharedPtr<FGameViewportClient>& Client: SplitClients)
	{
		if (Client)
		{
			Client->Reset();
		}
	}
	SplitClients.Reset();
	RenderData.ResetScene();
	EndSIEMode();
	LostFocus();
	bExitRequested = false;
	bSIEModeRequested = false;
	World = nullptr;
	GameInstance = nullptr;
	Engine = nullptr;

	// 타깃 버퍼 정리
	ResetViewTargets();
	Width = 0;
	Height = 0;
}

bool FGameViewportClient::BeginSIEMode()
{
	if (bSIEMode)
		return true;
	if (!World || !World->GetMainCamera())
		return false;

	UCameraComponent* MainCamera = World->GetMainCamera()->GetCameraComponent();
	if (!MainCamera) 
		return false;

	FMinimalViewInfo CurrentView;
	GetCurrentView(CurrentView);

	if (!SetCameraWorldPose(*MainCamera, CurrentView.Location, CurrentView.Rotation))
		return false;

	MainCamera->SetFieldOfView(CurrentView.FOV);
	MainCamera->SetNearZ(CurrentView.NearClipPlane);
	MainCamera->SetFarZ(CurrentView.FarClipPlane);
	MainCamera->SetIsOrthogonal(CurrentView.ProjectionMode == ECameraProjectionMode::Orthographic);
	MainCamera->SetOrthoWidth(CurrentView.OrthoWidth);
	bPreviousMainCameraExternalInputManaged = MainCamera->IsExternalInputManaged();
	MainCamera->SetExternalInputManaged(true);
	bSIEMode = true;
	LostFocus();
	return true;
}

void FGameViewportClient::EndSIEMode()
{
	if (!bSIEMode)
		return;
	if (World && World->GetMainCamera())
		if (UCameraComponent* MainCamera = World->GetMainCamera()->GetCameraComponent())
			MainCamera->SetExternalInputManaged(bPreviousMainCameraExternalInputManaged);
	bSIEMode = false;
	LostFocus();
}

void FGameViewportClient::GetKeyInputBySIEMode(int32 Key, bool bDown)
{
	if (bSIEMode && Key >= 0 && Key < 256)
		bSIEKeyDown[Key] = bDown;
}

void FGameViewportClient::GetAxisInputBySIEMode(EGameInputAxis Key, float Delta)
{
	if (!bSIEMode)
		return;
	switch (Key)
	{
	case EGameInputAxis::MouseX: SIEMouseDeltaX += Delta; break;
	case EGameInputAxis::MouseY: SIEMouseDeltaY += Delta; break;
	case EGameInputAxis::MouseWheel: SIEWheelDelta += Delta; break;
	}
}

void FGameViewportClient::TickSIEInput(float DeltaTime, float MoveSpeed, float MouseSensitivity)
{
	if (!bSIEMode || !World)
		return;

	ACameraActor* MainCamera = World->GetMainCamera();

	if (!MainCamera)
		return;

	UCameraComponent* CameraComponent = MainCamera->GetCameraComponent();

	if (!CameraComponent)
		return;
	
	FViewportCameraInput Input;
	Input.bCaptured = bSIEKeyDown[static_cast<int32>(EKeyCode::RButton)];
	Input.bAllowWheel = true; // The dispatcher only sends axes to the input owner.
	Input.MouseX = SIEMouseDeltaX;
	Input.MouseY = SIEMouseDeltaY;
	Input.Wheel = SIEWheelDelta;
	Input.bForward = bSIEKeyDown[static_cast<int32>(EKeyCode::W)];
	Input.bBackward = bSIEKeyDown[static_cast<int32>(EKeyCode::S)];
	Input.bRight = bSIEKeyDown[static_cast<int32>(EKeyCode::D)];
	Input.bLeft = bSIEKeyDown[static_cast<int32>(EKeyCode::A)];
	Input.bUp = bSIEKeyDown[static_cast<int32>(EKeyCode::E)];
	Input.bDown = bSIEKeyDown[static_cast<int32>(EKeyCode::Q)];
	FViewportCameraState Camera{ CameraComponent->GetWorldLocation(),
		CameraComponent->GetWorldRotation(), CameraComponent->GetOrthoWidth(),
		Width > 0 ? static_cast<float>(Width) : 800.0f, !CameraComponent->GetIsOrthogonal() };
	ApplyViewportCameraMovement(Camera, Input, DeltaTime, MoveSpeed, MouseSensitivity);
	if (SetCameraWorldPose(*CameraComponent, Camera.Location, Camera.Rotation))
		CameraComponent->SetOrthoWidth(Camera.OrthoWidth);
	SIEMouseDeltaX = SIEMouseDeltaY = SIEWheelDelta = 0.0f;
}

void FGameViewportClient::SyncSplitClients()
{
	const bool bSplit = World && World->IsSplitScreenEnabled();

	const int32 PlayerCount = bSplit ? static_cast<int32>(World->GetPlayerCount()) : 0;
	const int32 Wanted = std::max(1, PlayerCount) - 1;	// 2인 테스트하려면 1 넣어주세요

	while (static_cast<int32>(SplitClients.Num()) > Wanted)
	{
		const int32 Last = static_cast<int32>(SplitClients.Num()) - 1;
		SplitClients[Last]->Reset();
		SplitClients.RemoveAt(Last, 1);
	}

	while (static_cast<int32>(SplitClients.Num()) < Wanted)
	{
		TSharedPtr<FGameViewportClient> Client = CreateSplitClient();
		if (!Client) break;

		Client->SetPlayerIndex(static_cast<int32>(SplitClients.Num()) + 1);
		Client->SetWorld(World);
		Client->SetGameInstance(GameInstance);
		SplitClients.Add(Client);
	}
}

FGameViewportClient* FGameViewportClient::GetSplitClient(int32 Index)
{
	if (Index == 0)return this;
	const int32 Child = Index - 1;
	if (Child < 0 || Child >= static_cast<int32>(SplitClients.Num())) return nullptr;
	return SplitClients[Child].get();
}

FRect FGameViewportClient::GetSplitRect(const FRect& FullRect, int32 Index, int32 Count)
{
	if (Count <= 1) return FullRect;

	const float Left = std::floor(FullRect.Width * Index / Count);
	const float Right = std::floor(FullRect.Width * (Index + 1) / Count);
	return FRect{ FullRect.X + Left, FullRect.Y, Right - Left, FullRect.Height };
}

void FGameViewportClient::GetCurrentView(FMinimalViewInfo& View) const
{
	if (!World)
		return;

	// 게임 화면은 플레이어 카메라 매니저의 결과만 보여준다. F8과 매니저가 없을 때만 MainCamera
	if (!bSIEMode)
	{
		if (APlayerController* PC = World->GetPlayerController(static_cast<uint32>(PlayerIndex)))
		{
			const APlayerCameraManager* CameraManager = PC->GetCameraManager();

			if (CameraManager && CameraManager->IsCameraCacheValid())
			{
				View = CameraManager->GetCameraCacheView();
				return;
			}
		}
	}

	if (World->GetMainCamera())
	{
		World->GetMainCamera()->CalcCamera(View);
		return;
	}
}