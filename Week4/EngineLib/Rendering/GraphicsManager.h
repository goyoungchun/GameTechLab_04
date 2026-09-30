#pragma once

#include<wrl/client.h>

#include "Core/Math/Matrix.h"
#include "Core/enum.h"

#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Renderer.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Core/Math/Vector.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/Mesh/StaticMesh.h"
#include "Rendering/GpuResourceManager.h"
#include "SceneView.h"

struct FFrustum;
struct FTexture
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
};

enum ERenderQueueType
{
	RQT_SimplePrimitive,
	RQT_TexturedPrimitive,
	RQT_BillboardText,
	RQT_WorldAxis,
	RQT_Gizmo,
	RQT_BoundingBox,
	RQT_Particle,
	RQT_StaticMesh,
};

class FGraphicsManager
{
public:
	FGraphicsManager();
	~FGraphicsManager();

	void Initialize(HWND hWindow,
		FGpuResourceManager& gpuResourceManager,
		FAssetManager& assetManager);

	//void Prepare(const Camera* mCamera);
	void BeginFrame();
	void PrepareForUI();

	/* Rendering functions */
	void RenderSceneView(
		const TArray<FRenderInfo>& scenerRenderInfos,
		const TArray<FRenderInfo>& axisRenderInfos,
		const FSceneView& view,
		const AActor* selectedActor);
	void RenderGizmoView(
		const TArray<FRenderInfo>& gizmoRenderInfos,
		const FSceneView& view
	);

	void Display();
	void Update(float deltaTime);

	bool GetWireFrame() const { return mbWireFrame; }
	void SetWireFrame(bool bWireFrame) { mbWireFrame = bWireFrame; }

	float GetGridWidth() const;
	void SetGridWidth(float width);

	// Todo: Change name
	//void CreateBuffer(EPrimitive ePrimitive, FVertexSimple* vertices, uint32 verticesSize);
	//// temp
	//void CreateStaticMeshBuffer(const FStaticMesh& staticMesh);
	//void CreateTexturedBuffer(EPrimitive ePrimitive, const FVertexTextured* vertices, uint32 verticesSize);
	//void CreatePrimitiveTexture(EPrimitive ePrimitive, const wchar_t* texturePath);

	URenderer* GetRenderer() const;

	//Highlight
	//Line batch
	// 호출 즉시 그리지 않고 배열에 쌓는다. FlushLines()에서 한 번에 그린다.
	void DrawLine(const FVector& start, const FVector& end, const FVector4& color);
	void DrawAABBLine(const FBoundingBox& bounds, const FVector4& color);
	void FlushLines(const FSceneView& view);

	void RenderLoadingScreen();

	bool GetShowWorldAxis() const { return mbShowWorldAxis; }
	void SetShowWorldAxis(bool bShow) { mbShowWorldAxis = bShow; }

	void SetViewMode(EViewModeIndex InViewMode);
	EViewModeIndex GetViewMode() const { return mViewMode; }

	static FVector GetPrimitiveCenter(FName meshName);
	static FVector GetPrimitiveHalfExtent(FName meshName);

	bool HasShowFlag(EEngineShowFlags Flag) const;
	uint32 GetShowFlags() const { return mShowFlags; }
	void SetShowFlag(EEngineShowFlags Flag, bool bEnable);
	void SetShowFlags(uint32 flags) { mShowFlags = flags; }

	void CalculateLineBuffer(const TArray<const FRenderInfo*>& renderInfos);

	void ClearDepth() { mRenderer->ClearDepth(); }

private:
	/* Manager References */
	FGpuResourceManager* mGpuResourceManagerRef;
	FAssetManager* mAssetManagerRef;

	std::unique_ptr<URenderer> mRenderer;
	FMatrix mViewUnifiedProjectionMatrix;

	//ID3D11ShaderResourceView* mLoadingScreenSRV = nullptr;


	//TMap<EPrimitive, FBuffer> mBufferMap;

	// 텍스처 정점으로 만든 버퍼
	//TMap<EPrimitive, FBuffer> mTexturedBufferMap;

	//// Texture sub resource view and sampler for each primitive type
	//TMap<EPrimitive, FTexture> mPrimitiveTextureMap;
	//std::unique_ptr<FTexture> mDefaultWhiteTexture;

	// Debug for static mesh
	//TMap<const FStaticMesh*, FBuffer> mStaticMeshBuffer;

	// Graphics config
	//// 이번 프레임에 쌓인 선분. 정점 2개가 선분 하나
	TArray<FVertexSimple> mLineVertices;
	TArray<uint32> mLineIndices;

	bool mbWireFrame;
	bool mbPerspectiveProjection;
	bool mbShowWorldAxis = true;

	// Projection ratio smoothing
	float mProjectionStartRatio = 1.0f;
	float mProjectionTargetRatio = 1.0f;
	float mProjectionElapsed = 0.0f;
	float mProjectionDuration = 1.0f;
	bool mbProjectionTransitioning = false;

	// Grid 간격, 최대 한계선
	float mgridExtent = 1000.0f;
	float mgridSpacing = 0.5f;

	EViewModeIndex mViewMode = EViewModeIndex::VMI_Lit;

	uint32 mShowFlags = ~0;
		//static_cast<uint32>(EEngineShowFlags::SF_Primitives) |
		//static_cast<uint32>(EEngineShowFlags::SF_BillboardText) |
		//static_cast<uint32>(EEngineShowFlags::SF_WorldAxis);

	bool mbShowPrimitives = true;
	//void RenderBillboardText();

	void updateRenderQueue(
		const TArray<FRenderInfo>& renderInfos,
		TMap<ERenderQueueType, TArray<const FRenderInfo*>>& outRenderQueueMap,
		const FFrustum* frustum, uint32 showFlags);

	static bool HasViewShowFlag(uint32 showFlags, EEngineShowFlags flag){return (showFlags & static_cast<uint32>(flag)) != 0;}

	/* Rendering Functions */
	void renderSimplePrimitive(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);
	void renderTexturedPrimitive(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);
	void renderBillboardText(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);
	void renderWorldAxis(const TArray<const FRenderInfo*>& renderInfos);
	void renderBoundingBox(const TArray<const FRenderInfo*>& renderInfos, const FRotator& cameraRotation);
	// Instancing
	void renderSimplePrimitiveInstanced(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);
	//void RenderOverlay(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	void renderHighLight(const FRenderInfo& RI, const FSceneView& view);
	void renderGrid(const FSceneView& view);
	void renderGizmo(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);
	void renderParticle(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);
	void renderStaticMesh(const TArray<const FRenderInfo*>& renderInfos, const FSceneView& view);


	// Instancing Test
	//void RenderInstancingTest();
	//ID3D11Buffer* mTestInstanceIndexBuffer = nullptr;
};
