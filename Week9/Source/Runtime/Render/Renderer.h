#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture/Texture2D.h"
#include "Texture/Texture2DArray.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "LightCulling/TileLightCulling.h"
#include "ShadowRenderer.h"

struct FFrameLightData;

// 오클루전 컬링의 효과 상한을 재기 위한 측정 결과 (디버그 전용)
struct FOcclusionMeasureResult
{
	bool bValid = false;
	uint32 TotalObjects = 0;      // 불투명 패스에 제출된 물체 수
	uint32 VisibleObjects = 0;    // 최종 깊이 버퍼에 픽셀을 남긴 물체 수
	uint32 TotalDraws = 0;
	uint32 VisibleDraws = 0;
	uint64 TotalTriangles = 0;
	uint64 VisibleTriangles = 0;
	double ElapsedMs = 0.0;       // 측정 자체에 걸린 시간
	// GPU 오클루전 검증
	uint32 GPUOccludedDraws = 0;  // GPU가 가렸다고 판정한 드로우 수
	uint32 FalseCulls = 0;        // 그중 실제로는 픽셀이 보인 드로우 수
};

class FRenderer
{
public:
	bool Init();
	void DrawDeferredLighting(const FSceneView& View, const struct FDeferredViewTargets& Targets,
		const TArray<FPointLightRenderData>& PointLights);
	void DrawToneMap(FTexture2D* LightingHDR, FTexture2D* Depth);
	void DrawScreenOverlay(const FSceneView& View);

	// Source(선형)를 sRGB로 인코딩해 같은 크기의 출력 버퍼에 그린다.
	// 성공하면 그 출력 버퍼를 돌려주고, 호출자는 SwapSceneColorAndBind로 SceneColor와 바꾼다.
	TUniquePtr<FTexture2D>* DrawGammaCorrection(FTexture2D* Source);
	// 시점 상수 버퍼 및 렌더링 상태 설정
	void SetupView(const FSceneView& View, const FFrameLightData* Lights, const FTileLightCullingResources* TileLights = nullptr);

	// ObjViewer uses this forward path. Editor/PIE use SceneRenderer and GBuffer.
	void RenderAll(const FSceneView& View, const FRenderQueue& InQueue);

	// 불투명 요소 렌더링
	void RenderOpaque(const FSceneView& View, const FRenderQueue& InQueue, const FFrameLightData* Lights = nullptr, bool bGBufferPass = false, const FTileLightCullingResources* TileLights = nullptr, bool bReuseDepth = false);
	void RenderShadowPass(const FMatrix& LightVP, const FRenderQueue& InQueue, float ShadowBias, float ShadowSlopeBias);
	void RenderDepthPrepass(const FSceneView& View, const FRenderQueue& InQueue);

	// 반투명 요소 렌더링
	void RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue, const FFrameLightData* Lights = nullptr, const FTileLightCullingResources* TileLights = nullptr);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 캐시된 스태틱 메시 경로
	void ResetStaticDrawGroups() { StaticGroups.clear(); }
	void AddStaticDrawGroup(const FStaticDrawGroup* Group) { StaticGroups.push_back(Group); }

	// 측정 전용 불투명 오클루전 검사
	FOcclusionMeasureResult MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

	void UploadLocalLights(const FFrameLightData& Lights);

	FTileLightCulling& GetTileLightCulling() { return TileLightCulling; }

	void SetShadowEnabled(bool ShadowEnabled) { bShadowEnabled = ShadowEnabled; }
	FShadowRenderer& GetShadowRenderer() { return ShadowRenderer; }
	bool IsShadowEnabled() { return bShadowEnabled; }
	void SetShadowFilterMode(EShadowFilterMode FilteMode) { ShadowFilterMode = FilteMode; }
private:
	enum class EMeshPass { Forward, GBuffer, DepthPrepass, Shadow, ForwardAfterPrepass };

	TUniquePtr<FConstantBuffer> PerObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	TUniquePtr<FConstantBuffer> ScreenOverlayCB;

	// 감마 보정 출력 버퍼. 뷰포트마다 크기가 달라서 크기/포맷별로 보관해 매 프레임 재생성을 피한다.
	// Swap 후에는 뷰의 이전 SceneColor가 같은 크기로 들어오므로 다음 프레임에도 재사용된다.
	TArray<TUniquePtr<FTexture2D>> GammaOutputs;

	// 오브젝트 상수 버퍼
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	bool bCurrentWireframe = false;
	bool bShadowEnabled = true;

	// 광원 StructuredBuffer
	TUniquePtr<FStructuredBuffer> DirectionalLightBuffer;
	ComPtr<ID3D11ShaderResourceView> DirectionalLightBufferSRV;
	TUniquePtr<FStructuredBuffer> PointLightBuffer;
	ComPtr<ID3D11ShaderResourceView> PointLightBufferSRV;
	TUniquePtr<FStructuredBuffer> SpotLightBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SpotLightBufferSRV;

	uint32 PointLightCapacity = 0;
	uint32 SpotLightCapacity = 0;
	uint32 DirectionalLightCapacity = 0;

	FPipelineState* CurrentOpaquePSO = nullptr;

	// 측정 전용 자원
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;

	// Shadow Map
	FShadowRenderer ShadowRenderer;

	FGPUOcclusion GPUOcclusion;

	FTileLightCulling TileLightCulling;
	// 현재 불투명 패스 동안만 참조하며 소유권은 뷰에 있다.
	const FTileLightCullingResources* CurrentTileLights = nullptr;

	

	void DrawPackets(const FRenderQueue& InQueue, uint32 Begin, uint32 End, const FMatrix& ViewProjection, EMeshPass Pass = EMeshPass::Forward);

	void DrawStaticGroups(EMeshPass Pass = EMeshPass::Forward);

	void UpdatePerObjectConstants(const FMatrix& World, bool ReceiverShadow);

	// 스태틱 메시 묶음
	std::vector<const FStaticDrawGroup*> StaticGroups;
	// 머티리얼 바인딩
	void BindMaterial(UMaterial* material, bool bInWireframe = false, EMeshPass Pass = EMeshPass::Forward);
	void UpdateMaterialParams(const FRenderPacket& RenderPacket);
	void UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection);
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);

	EShadowFilterMode ShadowFilterMode = EShadowFilterMode::Hard;
	float ShadowFilterSpacing = 1.0f;
};
