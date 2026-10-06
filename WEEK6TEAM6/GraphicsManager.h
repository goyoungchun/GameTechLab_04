#pragma once

#include "Matrix.h"
#include "Enum.h"

#include "TArray.h"
#include "TMap.h"
#include "Renderer.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Vector.h"
#include "ShowFlags.h"
#include "FFogProcess.h"
#include "FDepthPreviewProcess.h"
#include "FRenderGraph.h"

class FAssetManager;
struct FViewport;
class UWorld;

struct FBuffer
{
	ID3D11Buffer* Buffer;
	uint32 SourceNum;
};

class FGraphicsManager
{
public:
	FGraphicsManager(HWND hWindow);
	~FGraphicsManager();

	void Prepare(const FCamera* mCamera, float Aspect, const FMatrix& ViewProjection, const FMatrix& InvViewProjection, FViewport& Viewport, UWorld* World, const EViewModeIndex InViewMode, const EViewportType InViewportType);

	void RenderHighLight(const TArray<UPrimitiveComponent*>& Primitives);
	void Render();

	void Display();

	float GetAspect() const { return mAspect; }
	EViewModeIndex GetViewModeIndex() const { return mViewModeIndex; }
	void SetViewModeIndex(EViewModeIndex viewModeIndex) { mViewModeIndex = viewModeIndex; }

	bool IsPerspectiveProjection() const;
	void SetPerspectiveProjection(bool bPerspectiveProjection);

	float GetPerspectiveRatio() const { return mProjectionRatio; }
	const FMatrix& GetViewProjectionMatrix() const { return mViewUnifiedProjectionMatrix; }
	void SetPerspectiveRatio(float ratio) { mProjectionRatio = FMath::Clamp(ratio, 0.0f, 1.0f); }

	float GetCameraOrthoDistance() const { return mCameraOrthoDistance; }
	void SetCameraOrthoDistance(float distance) { mCameraOrthoDistance = distance; }

	// Todo: Change name
	URenderer* GetRenderer() const;
	void OnResize(UINT width, UINT height);

	// Projection ratio smoothing
	void StartProjectionTransition(bool orthographic);
	bool IsOrthographicTarget() const;
	void UpdateProjectionTransition(float deltaTime);

	inline FRenderCollector& GetRenderCollector() { return mRenderCollector; }
	inline FFogProcess& GetFogProcess() { return mFogProcess; }
	inline FDepthPreviewProcess& GetDepthPreviewProcess() { return mDepthPreviewProcess; }

	inline int32 GetGridGap() { return GridGap; }
	void SetGridGap(int32 GridGap);

	struct FGpuTimerQuerySet
	{
		Microsoft::WRL::ComPtr<ID3D11Query> Disjoint;
		Microsoft::WRL::ComPtr<ID3D11Query> Begin;
		Microsoft::WRL::ComPtr<ID3D11Query> End;
		bool bIssued = false;
	};

	TArray<FGpuTimerQuerySet> GpuQueries;
	uint32 GpuQueryIndex = 0;

	float GpuRenderTime = 0.0f;
	void BeginGpuRenderTimer();
	void EndGpuRenderTimer();
	void UpdateGpuRenderTime();
	float GetGpuRenderTime() { return GpuRenderTime; }

private:
	struct FOutlineConstants
	{
		FVector4 OutlineColor;
		int32 StencilTexWidth;
		int32 StencilTexHeight;
		int32 OutlineRadius;
		int32 Padding;
	};

	struct FLightInfo
	{
		FVector Position;
		ELightType Type;
		FLinearColor Color;
		float Range;
		float Intensity;
		float FallOf;
	};

	struct FMeshContants
	{
		FMatrix Matrix;
		FVector4 Color;
		FVector2 UVOffset;
		int32 UseVertexColor;
		int32 HasTexture;
		int32 LightCount;
		int32 Padding[3];
	};

	URenderer* mRenderer;
	FMatrix mViewMatrix;
	FMatrix mProjectionMatrix;
	FMatrix mViewProjectionMatrix;
	FMatrix mInvViewProjectionMatrix;
	FMatrix mViewOrthogonalProjectionMatrix;
	FMatrix mViewUnifiedProjectionMatrix;
	FViewport* mViewport;

	// Prepare에서 갱신. 하이라이트 두께의 픽셀 → 월드 환산에 쓴다
	FVector mCameraLocation;
	FVector mCameraForward;
	float mCameraFovDegree = 60.0f;
	float mCameraOrthoDistance = 10.0f;
	float mCameraNear = 0.1f;
	float mCameraFar = 1000.0f;

	EViewModeIndex mViewModeIndex = EViewModeIndex::VMI_Lit;
	EViewportType mViewportType = EViewportType::Perspective;
	bool mbPerspectiveProjection;
	float mAspect;
	float mProjectionRatio; // 0.0f ~ 1.0f, 0이면 직교, 1이면 원근, 그 사이면 혼합

	// Projection ratio smoothing
	float mProjectionStartRatio = 1.0f;
	float mProjectionTargetRatio = 1.0f;
	float mProjectionElapsed = 0.0f;
	float mProjectionDuration = 1.0f;
	bool mbProjectionTransitioning = false;

	TArray<FLightInfo> mLightInfos;
	TSharedPtr<FStructuredBuffer> mLightInfoBuffer;

	TSharedPtr<FRenderPipeline> mMeshPipeline;

	TSharedPtr<FRenderPipeline> mHighlightMarkPipeline;
	TSharedPtr<FRenderPipeline> mHighlightDrawPipeline;
	TSharedPtr<FVertexBuffer> mHighlightVertexBuffer;
	TSharedPtr<FIndexBuffer> mHighlightIndexBuffer;

	FRenderCollector mRenderCollector;
	FRenderGraph mRenderGraph;
	FFogProcess mFogProcess;
	FDepthPreviewProcess mDepthPreviewProcess;

	int32 GridGap = 1;
	bool bGpuTimerActive = false;
};
