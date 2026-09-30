#pragma once
#include "Core/Math/Vector.h"

#include "ViewportTypes.h"
#include "../Rendering/SceneView.h"
#include "Engine/World.h"
#include "Engine/EngineStatics.h"
#include "Rendering/Camera.h"
#include "Rendering/RenderInfo.h"
#include "Gizmo.h"
#include "Core/Math/FBoundingBox.h"
#include "Core/AssetManager.h"


class AActor;
class FSceneManager;
struct FViewportSharedSettings;

struct FEditorViewportClient
{
public:

	explicit FEditorViewportClient(FViewportSharedSettings& sharedSettings, EViewportType viewporttype) :
		mSharedSettings(sharedSettings)
	{
		configureCamera(viewporttype);
	}

	void Initialize(FAssetManager& assetManagerRef);
	bool RaycastBounds(const FVector& rayStart,const FVector& rayEnd,const FBoundingBox& bounds);
	void RayCast(const FViewRect& viewrect, const TArray<FRenderInfo>& renderInfos, bool bCheckObject);
	float GetFov() const { return mCamera.mFovDegree; }
	void Update(float deltaTime, const FViewRect& viewrect, FSceneManager* sceneManager, bool bViewportHoverd, bool bViewportFocused);
	bool IsMouseHit() const { return bMouseHit; }
	FViewportSharedSettings& getSharedSettings() const { return mSharedSettings; };

	void Reset();

	void startProjectionTransition(bool orthographic);
	void updateProjectionTransition(float deltatime);

	float getProjectionRatio() const { return mProjectionRatio; }
	bool isOrthographicTarget() const { return mProjectionTargetRatio == 0.0f;  }

	FCamera& GetCamera() { return mCamera; }
	const FCamera& GetCamera() const { return mCamera; }

	void UpdateGizmoForView(const AActor* selectedActor);

	FGizmo& GetGizmo() { return mGizmo; }
	const FGizmo& GetGizmo() const{ return mGizmo; }
	void setViewportSettings(EViewportType type) { configureCamera(type); mProjectionElapsed = 0.0f; bProjectionTransitioning = false; }

	FCamera mCamera;
	FGizmo mGizmo;

private:
	/* Reference */
	FAssetManager* mAssetManagerRef = nullptr;

	//마우스 밑 무언가의
	FRenderInfo mHoveredRenderInfo;

	// 선택된 액터의 RenderInfo는 캐시하지 않는다. 필요할 때 ClickedActor->GetRenderInfos()로 그때그때 뽑는다.
	//마우스 밑 무언가가 Actor이면 저장. RayCast 에서 채워야 함 (아직 미구현)
	// INFO: mClickedActor moved to FSceneManager::mSelectedActor.
	//AActor* mClickedActor = nullptr;


	bool RayIntersectsTriangle( // 두개의 
		const FVector& Origin,
		const FVector& Dir,
		const FVector& V0,
		const FVector& V1,
		const FVector& V2,
		float& OutT, float& OutU, float& OutV);

	void DeprojectScreenToWorld(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		FVector& OutNearPoint, FVector& OutFarPoint);

	void DeprojectScreenToWorldForOrtho(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		FVector& OutNearPoint, FVector& OutFarPoint);

	void DeprojectScreenToWorldForUnified(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		float orthoDistance, float perspectiveRatio,
		FVector& OutNearPoint, FVector& OutFarPoint
	);

	void configureCamera(EViewportType viewportType);

	bool bMouseHit = false;
	float mProjectionRatio = 1.0f;
	float mProjectionStartRatio = 1.0f;
	float mProjectionTargetRatio = 1.0f;
	float mProjectionElapsed = 0.0f;
	float mProjectionDuration = 1.0f;
	bool bProjectionTransitioning = false;
	
	// RayCast가 이번 프레임에 쏜 광선. 기즈모 드래그가 같은 광선을 다시 쓴다
	FVector mRayNear;
	FVector mRayFar;

	// 복사용 클립보드
	TMap<int32, int32> UUIDChangeMap;
	json::JSON mActorClipBoard;
	json::JSON copyObject;

	FViewportSharedSettings& mSharedSettings;
};
