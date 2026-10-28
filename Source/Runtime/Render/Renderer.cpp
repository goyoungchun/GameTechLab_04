#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include "Engine/PrimitiveSceneProxy.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "DeferredViewTargets.h"

#include "Camera/CameraComponent.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <bit>

#include "SceneRenderData.h"
#include "LightCulling/TileLightCullingResources.h"

DECLARE_CYCLE_STAT("Draw Render Packets", STAT_DrawRenderPackets);

namespace
{
	struct FDeferredLightConstants
	{
		FMatrix InverseViewProjection;
		FVector4 CameraPosition; // W: perspective projection
		FVector4 CameraForward;
		FVector4 ViewportLightCount;
		FPointLightRenderData PointLights[DeferredMaxPointLights];
	};
	// static_assert(sizeof(FPointLightRenderData) % 16 == 0);
	static_assert(sizeof(FDeferredLightConstants) % 16 == 0);
	// VSSetConstantBuffers1의 오프셋은 16개 상수(256바이트) 단위여야 하므로 오브젝트마다 256바이트 칸을 쓴다.
	constexpr uint32 PerObjectSlotConstants = 16;
	static_assert(sizeof(FPerObjectConstants) <= ObjectSlotBytes);

	struct FFrameConstants
	{
		FMatrix VP;
		FMatrix V;
		FVector CameraPosition;
		uint32 NumDirectionalLights;
		uint32 NumPointLights;
		uint32 NumSpotLights;
		uint32 TileCountX;
		uint32 MaxLightsPerTile;
		FAmbientLightRenderData AmbientLight;
		uint32 TileSize = LIGHT_CULLING_TILE_SIZE;
		float ShadowFilterSpacing;
		FVector2 ShadowMapTexelSize;
		float Near;
		float Far;
		FVector2 LocalAtlasTexelSize;
	};
	static_assert(sizeof(FFrameConstants) % 16 == 0);
	static_assert(offsetof(FFrameConstants, TileCountX) == 152);
	static_assert(offsetof(FFrameConstants, AmbientLight) == 160);
	static_assert(offsetof(FFrameConstants, TileSize) == 160 + sizeof(FAmbientLightRenderData));

	struct FShadowFrameConstants
	{
		FMatrix VP;
		float ShadowDepthBias;
		float ShadowSlopeBias;
		float ShadowBiasClamp;
		float Padding;
	};
	static_assert(sizeof(FShadowFrameConstants) % 16 == 0);

	constexpr uint32 MinPerObjectSlots = 1024;

	const FMatrix& GetPacketWorld(const FRenderPacket& Packet)
	{
		if (Packet.Proxy) return Packet.Proxy->GetLocalToWorld();
		return Packet.Model ? *Packet.Model : FMatrix::Identity;
	}

	// 불투명 패킷과 같은 규칙의 키 (묶음은 전부 불투명)
	uint64 MakeGroupKey(const FStaticDrawGroup& Group)
	{
		return (static_cast<uint64>(Group.Material->SortID) << 47)
			| (static_cast<uint64>(Group.Mesh->SortID) << 31)
			| (static_cast<uint64>(Group.LODIndex & 0x3) << 29);
	}

	struct FStaticMeshShaderMacroKey
	{
		bool bLightingEnabled = false;
		EShadingModel ShadingModel = EShadingModel::BlinnPhong;
		bool bShadowEnabled = false;
		EShadowFilterMode ShadowFilterMode = EShadowFilterMode::PCSS;
	};

	TArray<FShaderMacro> MakeStaticMeshShaderMacro(const FStaticMeshShaderMacroKey& Key)
	{
		TArray<FShaderMacro> Macros;

		Macros.Add({ "LIGHTING_ENABLED", Key.bLightingEnabled ? "1" : "0" });

		switch (Key.ShadingModel)
		{
		case EShadingModel::Lambert:
			Macros.Add({ "SHADING_MODEL", "SHADING_MODEL_LAMBERT" });
			break;
		case EShadingModel::Phong:
			Macros.Add({ "SHADING_MODEL", "SHADING_MODEL_PHONG" });
			break;
		case EShadingModel::BlinnPhong:
			Macros.Add({ "SHADING_MODEL", "SHADING_MODEL_BLINN_PHONG" });
			break;
		}

		Macros.Add({ "SHADOW_ENABLED", Key.bShadowEnabled ? "1" : "0" });

		switch (Key.ShadowFilterMode)
		{
		case EShadowFilterMode::Hard:
			Macros.Add({ "SHADOW_FILTER_MODE", "SHADOW_FILTER_HARD" });
			break;
		case EShadowFilterMode::PCF3x3:
			Macros.Add({ "SHADOW_FILTER_MODE", "SHADOW_FILTER_PCF3x3" });
			break;
		case EShadowFilterMode::PCF5x5:
			Macros.Add({ "SHADOW_FILTER_MODE", "SHADOW_FILTER_PCF5x5" });
			break;
		case EShadowFilterMode::Poisson:
			Macros.Add({ "SHADOW_FILTER_MODE", "SHADOW_FILTER_POISSON" });
			break;
		case EShadowFilterMode::PCSS:
			Macros.Add({ "SHADOW_FILTER_MODE", "SHADOW_FILTER_PCSS" });
			break;
		}

		return Macros;
	}

	bool EnsureBufferCapacity(
		TUniquePtr<FStructuredBuffer>& Buffer,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& SRV,
		uint32& Capacity,
		uint32 RequiredCount,
		uint32 Stride)
	{
		if (Buffer && SRV && Capacity >= RequiredCount && Capacity > 0)
		{
			return true;
		}

		uint32 NewCapacity = std::max(Capacity, 1u);
		while (NewCapacity < RequiredCount)
		{
			NewCapacity *= 2;
		}

		SRV.Reset();
		Buffer = RenderCommand::CreateStructuredBuffer(NewCapacity, Stride, D3D11_USAGE_DYNAMIC, false);
		if (!Buffer || !Buffer->GetBuffer())
		{
			Buffer.reset();
			Capacity = 0;
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC Desc{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
			.Buffer = {
				.NumElements = NewCapacity,
			},
		};
		if (FAILED(RenderCommand::GetDevice()->CreateShaderResourceView(Buffer->GetBuffer(), &Desc, SRV.ReleaseAndGetAddressOf())))
		{
			SRV.Reset();
			Buffer.reset();
			Capacity = 0;
			return false;
		}

		Capacity = NewCapacity;
		return true;
	}

	// ScreenOverlayShader의 cbuffer와 순서가 같아야 한다 (48바이트)
	struct FScreenOverlayConstants
	{
		FVector4 FadeColor;
		float LetterboxRatio;
		float VignetteIntensity;
		float VignetteRadius;
		float VignetteSoftness;
		float AspectRatio;
		float Padding[3];
	};
	static_assert(sizeof(FScreenOverlayConstants) % 16 == 0, "Constant buffer size must be a multiple of 16 bytes");
}

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FFrameConstants));
	DeferredLightCB = RenderCommand::CreateConstantBuffer(sizeof(FDeferredLightConstants));
	ScreenOverlayCB = RenderCommand::CreateConstantBuffer(sizeof(FScreenOverlayConstants));

	EnsureBufferCapacity(DirectionalLightBuffer, DirectionalLightBufferSRV, DirectionalLightCapacity, 1, sizeof(FDirectionalLightRenderData));
	EnsureBufferCapacity(PointLightBuffer, PointLightBufferSRV, PointLightCapacity, 1, sizeof(FPointLightRenderData));
	EnsureBufferCapacity(SpotLightBuffer, SpotLightBufferSRV, SpotLightCapacity, 1, sizeof(FSpotLightRenderData));
	EnsureBufferCapacity(DirectionalLightBuffer, DirectionalLightBufferSRV, DirectionalLightCapacity, 1, sizeof(FDirectionalLightRenderData));

	GPUOcclusion.Init();   // 실패해도 오클루전만 못 쓸 뿐 렌더링은 된다

	ShadowRenderer.Initialize();

	return true;
}

// 필요한 칸 수가 용량을 넘을 때만 두 배씩 키워 재할당을 드물게 한다.
void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity)
		return;

	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount)
		NewCapacity *= 2;

	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * ObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer ({} slots) creation failed. Falling back to per-draw updates.", NewCapacity);
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

// 시점 상수 버퍼 및 렌더링 상태 설정
void FRenderer::SetupView(const FSceneView& View, const FFrameLightData* Lights, const FTileLightCullingResources* TileLights)
{
	const float LocalAtlasSize = (float)FShadowRenderer::LocalAtlasSize;
	const float DirectionalResolution = static_cast<float>(ShadowRenderer.GetDirectionalShadowMapResolution());
	CurrentTileLights = TileLights && TileLights->IsValid() && Lights && (View.ViewMode == EViewportMode::Lit || View.ViewMode == EViewportMode::Wireframe) ? TileLights : nullptr;
	bCurrentWireframe = View.ViewMode == EViewportMode::Wireframe;
	FFrameConstants Constants{
		.VP = View.ViewProjectionMatrix,
		.V = View.ViewMatrix,
		.CameraPosition = View.ViewLocation,
		.TileCountX = CurrentTileLights ? CurrentTileLights->TileCountX : 0,
		.MaxLightsPerTile = CurrentTileLights ? CurrentTileLights->MaxLightsPerTile : 0,
		.ShadowFilterSpacing = ShadowFilterSpacing,
		.ShadowMapTexelSize = FVector2(1.0f / DirectionalResolution, 1.0f / DirectionalResolution),
		.Near = View.NearClip,
		.Far = View.FarClip,
		.LocalAtlasTexelSize = FVector2(1.0f / LocalAtlasSize, 1.0f / LocalAtlasSize),
	};

	if (Lights)
	{
		Constants.NumDirectionalLights = Lights->DirectionalLights.Num();
		Constants.NumPointLights = Lights->PointLights.Num();
		Constants.NumSpotLights = Lights->SpotLights.Num();
		Constants.AmbientLight = Lights->AmbientLight;
	}

	if (View.ViewMode == EViewportMode::Unlit)
	{
		const auto Macros = MakeStaticMeshShaderMacro({
			.bLightingEnabled = false,
			.bShadowEnabled = false,
			});
		CurrentOpaquePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Opaque, Macros);
	}
	else if (View.ViewMode == EViewportMode::Lit)
	{
		const auto Macros = MakeStaticMeshShaderMacro({
			.bLightingEnabled = true,
			.ShadingModel = View.ShadingModel,
			.bShadowEnabled = bShadowEnabled,
			.ShadowFilterMode = ShadowFilterMode,
			});
		CurrentOpaquePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Opaque, Macros);
	}
	else if (View.ViewMode == EViewportMode::Wireframe)
	{
		const auto Macros = MakeStaticMeshShaderMacro({
			.bLightingEnabled = true,
			.ShadingModel = View.ShadingModel,
			.bShadowEnabled = false,
			.ShadowFilterMode = ShadowFilterMode,
			});
		CurrentOpaquePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Wireframe, Macros);
	}
	else if (View.ViewMode == EViewportMode::SceneDepth)
	{
		const auto Macros = MakeStaticMeshShaderMacro({
			.bLightingEnabled = false,
			.bShadowEnabled = false,
			});
		CurrentOpaquePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Opaque, Macros);
	}
	else if (View.ViewMode == EViewportMode::WorldNormal)
	{
		const auto Macros = MakeStaticMeshShaderMacro({
			.bLightingEnabled = false,
			.bShadowEnabled = false,
			});
		CurrentOpaquePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_WorldNormal, Macros);
	}

	RenderCommand::UpdateBufferData(ViewCB.get(), &Constants);

	RenderCommand::BindShaderResource(10, DirectionalLightBufferSRV.Get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(11, PointLightBufferSRV.Get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(12, SpotLightBufferSRV.Get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(13, ShadowRenderer.GetSRV(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(14, ShadowRenderer.GetLocalAtlasSRV(), EShaderBindFlagBits::Pixel);

	RenderCommand::BindSamplerState(1, ESamplerState::ComparisonLinear, EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(2, ESamplerState::PointClamp, EShaderBindFlagBits::Pixel);
}

// 전체 렌더 큐 렌더링
void FRenderer::RenderAll(const FSceneView& View, const FRenderQueue& InQueue)
{
	RenderOpaque(View, InQueue);
	RenderTranslucent(View, InQueue);
}

// 불투명 요소 렌더링
void FRenderer::RenderOpaque(const FSceneView& View, const FRenderQueue& InQueue, const FFrameLightData* Lights, bool bGBufferPass, const FTileLightCullingResources* TileLights, bool bReuseDepth)
{
	SetupView(View, Lights, bGBufferPass ? nullptr : TileLights);
	const EMeshPass Pass = bGBufferPass ? EMeshPass::GBuffer : (bReuseDepth ? EMeshPass::ForwardAfterPrepass : EMeshPass::Forward);
	DrawStaticGroups(Pass);
	DrawPackets(InQueue, 0, InQueue.GetFirstTranslucentIndex(), View.ViewProjectionMatrix, Pass);
	if (CurrentTileLights)
	{
		RenderCommand::UnbindShaderResources(8, 2, EShaderBindFlagBits::Pixel);
	}
	RenderCommand::UnbindShaderResources(10, 4, EShaderBindFlagBits::Pixel);
	CurrentTileLights = nullptr;
}

void FRenderer::RenderShadowPass(const FMatrix& LightVP, const FRenderQueue& InQueue, float ShadowBias, float ShadowSlopeBias)
{
	float BaseDepthBias = 0.00001f;
	float BaseSlopeBias = 0.5f;

	{
		// SetupView();
		// TODO: 별도 함수로 분리. 아마 DepthPrepass에서도 쓸 수 있을듯
		FShadowFrameConstants Constants{
			.VP = LightVP,
			.ShadowDepthBias = 2.0f * BaseDepthBias * ShadowBias,
			.ShadowSlopeBias = 2.0f * BaseSlopeBias * ShadowSlopeBias,
			.ShadowBiasClamp = 0.0005f,
		};

		CurrentOpaquePSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Shadow);

		RenderCommand::UpdateBufferData(ViewCB.get(), &Constants);
	}

	DrawStaticGroups(EMeshPass::Shadow);
	DrawPackets(InQueue, 0, InQueue.GetFirstTranslucentIndex(), LightVP, EMeshPass::Shadow);
}

void FRenderer::RenderDepthPrepass(const FSceneView& View, const FRenderQueue& InQueue)
{
	SetupView(View, nullptr);
	DrawStaticGroups(EMeshPass::DepthPrepass);
	DrawPackets(InQueue, 0, InQueue.GetFirstTranslucentIndex(), View.ViewProjectionMatrix, EMeshPass::DepthPrepass);
}

void FRenderer::DrawDeferredLighting(const FSceneView& View, const FDeferredViewTargets& Targets,
	const TArray<FPointLightRenderData>& PointLights)
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::DeferredLighting);
	if (!PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader || !DeferredLightCB)
		return;
	FDeferredLightConstants Constants{};
	Constants.InverseViewProjection = View.ViewProjectionMatrix.Inverse();
	Constants.CameraPosition = FVector4(View.ViewLocation.X, View.ViewLocation.Y,
		View.ViewLocation.Z, View.bIsPerspective ? 1.0f : 0.0f);
	Constants.CameraForward = FVector4(View.ViewForward.X, View.ViewForward.Y, View.ViewForward.Z, 0.0f);
	const uint32 LightCount = std::min<uint32>(static_cast<uint32>(PointLights.Num()), DeferredMaxPointLights);
	Constants.ViewportLightCount = FVector4(
		static_cast<float>(Targets.SceneColor->GetWidth()),
		static_cast<float>(Targets.SceneColor->GetHeight()),
		static_cast<float>(LightCount), 0.0f);
	for (uint32 Index = 0; Index < LightCount; ++Index)
		Constants.PointLights[Index] = PointLights[Index];
	RenderCommand::UpdateBufferData(DeferredLightCB.get(), &Constants, sizeof(Constants));

	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindConstantBuffer(0, DeferredLightCB.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, Targets.BaseColorMetallic.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, Targets.NormalRoughness.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(2, Targets.Depth.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	if (LightCount > 0)
	{
		FPipelineState* AdditivePSO = FRenderResourceManager::GetPSO(EPSOType::DeferredPointLighting);
		if (AdditivePSO && AdditivePSO->Shader && AdditivePSO->Shader->VertexShader && AdditivePSO->Shader->PixelShader)
		{
			Constants.ViewportLightCount.W = 1.0f;
			RenderCommand::UpdateBufferData(DeferredLightCB.get(), &Constants, sizeof(Constants));
			RenderCommand::BindPipelineState(*AdditivePSO);
			RenderCommand::Draw(3);
		}
	}

	RenderCommand::UnbindShaderResources(0, 2, EShaderBindFlagBits::Pixel);
}

void FRenderer::DrawToneMap(FTexture2D* LightingHDR, FTexture2D* Depth)
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::ToneMap);
	if (!PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader)
		return;

	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, LightingHDR, EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, Depth, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);

	RenderCommand::UnbindShaderResources(0, 2, EShaderBindFlagBits::Pixel);
}

// 반투명 요소 렌더링
void FRenderer::RenderTranslucent(const FSceneView& View, const FRenderQueue& InQueue, const FFrameLightData* Lights, const FTileLightCullingResources* TileLights)
{
	SetupView(View, Lights, TileLights);
	DrawPackets(InQueue, InQueue.GetFirstTranslucentIndex(), InQueue.Num(), View.ViewProjectionMatrix);
	if (CurrentTileLights)
	{
		RenderCommand::UnbindShaderResources(8, 2, EShaderBindFlagBits::Pixel);
	}
	RenderCommand::UnbindShaderResources(10, 4, EShaderBindFlagBits::Pixel);
	CurrentTileLights = nullptr;
	StaticGroups.clear();
}

// 스태틱 메시 묶음을 그린다
void FRenderer::DrawStaticGroups(EMeshPass Pass)
{
	if (StaticGroups.empty())
		return;

	// 스태틱 메시 묶음 정렬
	std::erase_if(StaticGroups, [](const FStaticDrawGroup* Group) { return Group->Items.empty(); });
	std::sort(StaticGroups.begin(), StaticGroups.end(),
		[](const FStaticDrawGroup* A, const FStaticDrawGroup* B) { return MakeGroupKey(*A) < MakeGroupKey(*B); });

	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);
	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	UMaterial* BoundMaterial = nullptr;
	for (const FStaticDrawGroup* Group : StaticGroups)
	{
		RenderCommand::BindMesh(Group->Mesh, Group->LODIndex);
		if (Group->Material != BoundMaterial)
		{
			BoundMaterial = Group->Material;
			BindMaterial(BoundMaterial, bCurrentWireframe, Pass);
			FRenderPacket MaterialOnly;
			MaterialOnly.Material = BoundMaterial;
			if (Pass != EMeshPass::DepthPrepass && Pass != EMeshPass::Shadow)
				UpdateMaterialParams(MaterialOnly);
		}

		for (const FStaticDrawItem& Item : Group->Items)
		{
			if (Pass == EMeshPass::Shadow)
			{
				if (!Item.Proxy->GetCastShadow())
				{
					continue;
				}

				RenderCommand::SetRasterizerState(Item.Proxy->GetBackfaceDepth() ? ERasterizerState::SolidNone : ERasterizerState::SolidBack);
			}

			if (bUsePerObjectSlots && Item.Slot != InvalidObjectSlot)
			{
				RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), Item.Slot * PerObjectSlotConstants, PerObjectSlotConstants,
					EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
			}
			else
			{
				RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
				UpdatePerObjectConstants(Item.Proxy->GetLocalToWorld(), Item.Proxy->GetReceiveShadow());
			}
			RenderCommand::DrawIndexed(Item.IndexCount, Item.StartIndex);
		}
	}

}

void FRenderer::DrawScreenOverlay(const FSceneView& View)
{
	if (View.FadeAmount <= 0.0f && View.LetterboxRatio <= 0.0f && View.VignetteIntensity <= 0.0f)
	{
		return;
	}

	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::ScreenOverlay);
	if (!PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader || !ScreenOverlayCB)
	{
		return;
	}

	FScreenOverlayConstants Constants{};
	Constants.FadeColor = FVector4(View.FadeColor.X, View.FadeColor.Y, View.FadeColor.Z, FMath::Clamp(View.FadeAmount, 0.0f, 1.0f));
	Constants.LetterboxRatio = FMath::Clamp(View.LetterboxRatio, 0.0f, 0.5f);
	Constants.VignetteIntensity = FMath::Clamp(View.VignetteIntensity, 0.0f, 1.0f);
	Constants.VignetteRadius = FMath::Clamp(View.VignetteRadius, 0.0f, 1.0f);
	Constants.VignetteSoftness = std::max(View.VignetteSoftness, 0.001f); // smoothstep 구간이 0이 되지 않게
	Constants.AspectRatio = View.AspectRatio > 0.0f ? View.AspectRatio : 1.0f;

	RenderCommand::UpdateBufferData(ScreenOverlayCB.get(), &Constants, sizeof(Constants));
	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindConstantBuffer(0, ScreenOverlayCB.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);

}

TUniquePtr<FTexture2D>* FRenderer::DrawGammaCorrection(FTexture2D* Source)
{
	FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::GammaCorrection);
	if (!Source || !PSO || !PSO->Shader || !PSO->Shader->VertexShader || !PSO->Shader->PixelShader)
		return nullptr;

	const uint32 Width = Source->GetWidth();
	const uint32 Height = Source->GetHeight();
	if (!Width || !Height)
		return nullptr;

	// 같은 텍스처를 읽으면서 쓸 수 없으므로 크기/포맷이 같은 출력 버퍼를 찾거나 만든다.
	TUniquePtr<FTexture2D>* Output = nullptr;
	for (TUniquePtr<FTexture2D>& Candidate : GammaOutputs)
	{
		if (Candidate && Candidate.get() != Source && Candidate->GetWidth() == Width &&
			Candidate->GetHeight() == Height && Candidate->GetFormat() == Source->GetFormat())
		{
			Output = &Candidate;
			break;
		}
	}

	if (!Output)
	{
		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Width;
		Desc.Height = Height;
		Desc.Format = Source->GetFormat();
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		TUniquePtr<FTexture2D> NewTarget = RenderCommand::CreateTexture2D(Desc);
		if (!NewTarget || !NewTarget->GetRTV() || !NewTarget->GetSRV())
			return nullptr;

		// 뷰포트 크기를 자주 바꿔도 무한히 쌓이지 않게 상한을 둔다.
		constexpr int32 MaxGammaOutputs = 8;
		if (GammaOutputs.Num() >= MaxGammaOutputs)
			GammaOutputs.RemoveAt(0, 1);
		GammaOutputs.Add(std::move(NewTarget));
		Output = &GammaOutputs[GammaOutputs.Num() - 1];
	}

	// Source가 렌더 타깃으로 묶여 있으면 SRV로 읽을 수 없어서 먼저 해제한다.
	RenderCommand::EndRenderPass();
	RenderCommand::BindShaderResource(0, static_cast<FTexture2D*>(nullptr), EShaderBindFlagBits::Pixel);
	RenderCommand::BindRenderPassNoClear(Output->get(), nullptr, Width, Height);
	RenderCommand::BindPipelineState(*PSO);
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::BindShaderResource(0, Source, EShaderBindFlagBits::Pixel);
	RenderCommand::Draw(3);
	RenderCommand::BindShaderResource(0, static_cast<FTexture2D*>(nullptr), EShaderBindFlagBits::Pixel);
	RenderCommand::EndRenderPass();
	return Output;
}

// 정렬된 패킷 중 [Begin, End) 범위를 그린다
void FRenderer::DrawPackets(const FRenderQueue& InQueue, uint32 Begin, uint32 End, const FMatrix& ViewProjection, EMeshPass Pass)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);

	UStaticMesh* LastMesh = nullptr;
	UMaterial* LastMaterial = nullptr;
	uint8 LastLODIndex = 0;

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

	for (uint32 k = Begin; k < End; ++k)
	{
		const FRenderPacket& RenderPacket = InQueue[k];

		if (Pass == EMeshPass::Shadow && (!RenderPacket.Proxy || !RenderPacket.Proxy->GetCastShadow()))
		{
			continue;
		}

		if (RenderPacket.Mesh != LastMesh || RenderPacket.LODIndex != LastLODIndex) {
			RenderCommand::BindMesh(RenderPacket.Mesh, RenderPacket.LODIndex);
		}
		if (RenderPacket.Material != LastMaterial) {
			BindMaterial(RenderPacket.Material, bCurrentWireframe, Pass);
		}
		if (Pass == EMeshPass::Shadow)
		{
			RenderCommand::SetRasterizerState(RenderPacket.Proxy->GetBackfaceDepth() ? ERasterizerState::SolidNone : ERasterizerState::SolidBack);
		}
		if ((Pass != EMeshPass::DepthPrepass && Pass != EMeshPass::Shadow) && (RenderPacket.Material != LastMaterial || RenderPacket.MaterialParamData))
			UpdateMaterialParams(RenderPacket);
		if (bUsePerObjectSlots && RenderPacket.Slot != InvalidObjectSlot)
		{
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(), RenderPacket.Slot * PerObjectSlotConstants, PerObjectSlotConstants,
				EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		}
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
			UpdatePerObjectConstants(RenderPacket, ViewProjection);
		}

		RenderCommand::DrawIndexed(
			RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.Mesh->GetIndexBuffer(RenderPacket.LODIndex)->GetIndexCount(),
			RenderPacket.StartIndex
		);
		LastMaterial = RenderPacket.Material;
		LastMesh = RenderPacket.Mesh;
		LastLODIndex = RenderPacket.LODIndex;
	}
}

FOcclusionMeasureResult FRenderer::MeasureOpaqueOcclusion(const FMatrix& ViewProjection, const FRenderQueue& InQueue)
{
	FOcclusionMeasureResult Result;

	// 불투명 드로우를 두 경로(스태틱 묶음, 일반 패킷)에서 한 목록으로 모은다. 그린 순서와 같게 묶음 먼저.
	struct FMeasureDraw
	{
		UStaticMesh* Mesh;
		UMaterial* Material;
		uint8 LODIndex;
		uint32 Slot;
		uint32 StartIndex;
		uint32 IndexCount;
		const FMatrix* World;       // 칸이 없을 때만 쓴다
		const void* ObjectKey;      // 같은 물체의 섹션을 한 물체로 센다
		bool bOccludedByGpu;
		const FRenderPacket* Packet;
		bool bReceiveShadow;
	};
	std::vector<FMeasureDraw> Draws;
	for (const FStaticDrawGroup* Group : StaticGroups)
		for (const FStaticDrawItem& Item : Group->Items)
			Draws.push_back({ Group->Mesh, Group->Material, Group->LODIndex, Item.Slot, Item.StartIndex, Item.IndexCount,
				&Item.Proxy->GetLocalToWorld(), Item.Proxy, Item.bOccludedByGpu != 0, nullptr, Item.Proxy->GetReceiveShadow() });
	for (uint32 k = 0; k < InQueue.GetFirstTranslucentIndex(); ++k)
	{
		const FRenderPacket& Packet = InQueue[k];
		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Packet.Mesh->GetIndexBuffer(Packet.LODIndex)->GetIndexCount();
		// 프록시가 없는 패킷(빌보드 등)은 패킷 자체를 한 물체로 센다.
		const void* Key = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);
		const bool bReceiveShadow = Packet.Proxy ? Packet.Proxy->GetReceiveShadow() : true;
		Draws.push_back({ Packet.Mesh, Packet.Material, Packet.LODIndex, Packet.Slot, Packet.StartIndex, IndexCount,
			&GetPacketWorld(Packet), Key, Packet.bOccludedByGpu, &Packet, bReceiveShadow });
	}

	const uint32 Count = static_cast<uint32>(Draws.size());
	if (Count == 0)
		return Result;

	const auto StartTime = std::chrono::high_resolution_clock::now();

	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	// 같은 셰이더·같은 행렬로 다시 그리면 깊이가 비트 단위로 같으므로,
	// LESS_EQUAL이면 최종 깊이 버퍼에서 이 물체가 이긴 픽셀만 통과한다.
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf())))
			return Result;
	}

	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC QueryDesc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&QueryDesc, Query.GetAddressOf())))
			return Result;
		OcclusionQueries.Add(std::move(Query));
	}

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	if (!bUsePerObjectSlots)
		RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	UStaticMesh* BoundMesh = nullptr;
	UMaterial* BoundMaterial = nullptr;
	uint8 BoundLOD = 0;

	// 1) 드로우마다 쿼리를 걸고 다시 그린다. 본 패스와 같은 바인딩(칸 또는 PerObjectCB)을 쓴다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		if (Draw.Mesh != BoundMesh || Draw.LODIndex != BoundLOD)
			RenderCommand::BindMesh(Draw.Mesh, Draw.LODIndex);
		if (Draw.Material != BoundMaterial)
		{
			BindMaterial(Draw.Material);
			// BindMaterial이 바꾼 상태를 측정용으로 덮어쓴다.
			RenderCommand::SetBlendState(EBlendState::NoColorWrite);
			Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		}
		if (Draw.Packet && (Draw.Material != BoundMaterial || Draw.Packet->MaterialParamData))
			UpdateMaterialParams(*Draw.Packet);

		if (bUsePerObjectSlots && Draw.Slot != InvalidObjectSlot)
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(),
				Draw.Slot * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		else
		{
			RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);
			UpdatePerObjectConstants(*Draw.World, Draw.bReceiveShadow);
		}

		ID3D11Query* Query = OcclusionQueries[Index].Get();
		Context->Begin(Query);
		RenderCommand::DrawIndexed(Draw.IndexCount, Draw.StartIndex);
		Context->End(Query);

		BoundMesh = Draw.Mesh;
		BoundMaterial = Draw.Material;
		BoundLOD = Draw.LODIndex;
	}

	// 2) 결과를 기다려 모은다. 한 물체가 Section 여러 개로 나뉘면 하나라도 보이면 보이는 것으로 친다.
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMeasureDraw& Draw = Draws[Index];

		UINT64 Samples = 0;
		while (Context->GetData(OcclusionQueries[Index].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}

		const uint64 Triangles = Draw.IndexCount / 3;
		const bool bVisible = Samples > 0;

		// Cull을 끄고 판정만 한 드로우: 가렸다고 했는데 최종 화면에 픽셀이 남았으면 잘못 가린 것이다.
		if (Draw.bOccludedByGpu)
		{
			++Result.GPUOccludedDraws;
			if (bVisible)
				++Result.FalseCulls;
		}

		++Result.TotalDraws;
		Result.TotalTriangles += Triangles;
		if (bVisible)
		{
			++Result.VisibleDraws;
			Result.VisibleTriangles += Triangles;
		}

		bool& bObjectVisible = ObjectVisible[Draw.ObjectKey];
		bObjectVisible = bObjectVisible || bVisible;
	}

	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;

	// 뒤따르는 Grid·반투명 패스를 위해 상태를 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

	Result.ElapsedMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
}

uint8* FRenderer::BeginObjectConstants(uint32 MaxSlots)
{
	if (!bUsePerObjectSlots || MaxSlots == 0) return nullptr;
	EnsurePerObjectSlotCapacity(MaxSlots);
	if (!PerObjectSlotCB) { bUsePerObjectSlots = false; return nullptr; }
	uint8* Destination = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Destination) bUsePerObjectSlots = false;
	return Destination;
}

void FRenderer::EndObjectConstants()
{
	if (PerObjectSlotCB)
	{
		RenderCommand::Unmap(PerObjectSlotCB.get());
	}
}

void FRenderer::UploadLocalLights(const FFrameLightData& Lights)
{
	const uint32 NumDirectional = static_cast<uint32>(Lights.DirectionalLights.size());
	const uint32 NumPoint = static_cast<uint32>(Lights.PointLights.size());
	const uint32 NumSpot = static_cast<uint32>(Lights.SpotLights.size());

	if (EnsureBufferCapacity(DirectionalLightBuffer, DirectionalLightBufferSRV, DirectionalLightCapacity, NumDirectional, sizeof(FDirectionalLightRenderData)) && NumDirectional > 0)
	{
		RenderCommand::UpdateBufferData(DirectionalLightBuffer.get(), Lights.DirectionalLights.GetData(), NumDirectional * sizeof(FDirectionalLightRenderData));
	}
	if (EnsureBufferCapacity(PointLightBuffer, PointLightBufferSRV, PointLightCapacity, NumPoint, sizeof(FPointLightRenderData)) && NumPoint > 0)
	{
		RenderCommand::UpdateBufferData(PointLightBuffer.get(), Lights.PointLights.GetData(), NumPoint * sizeof(FPointLightRenderData));
	}
	if (EnsureBufferCapacity(SpotLightBuffer, SpotLightBufferSRV, SpotLightCapacity, NumSpot, sizeof(FSpotLightRenderData)) && NumSpot > 0)
	{
		RenderCommand::UpdateBufferData(SpotLightBuffer.get(), Lights.SpotLights.GetData(), NumSpot * sizeof(FSpotLightRenderData));
	}
	if (EnsureBufferCapacity(DirectionalLightBuffer, DirectionalLightBufferSRV, DirectionalLightCapacity, NumDirectional, sizeof(FDirectionalLightRenderData)) && NumDirectional > 0)
	{
		RenderCommand::UpdateBufferData(DirectionalLightBuffer.get(), Lights.DirectionalLights.GetData(), NumDirectional * sizeof(FDirectionalLightRenderData));
	}
}

// 머티리얼 파이프라인 상태 및 텍스처 바인딩
void FRenderer::BindMaterial(UMaterial* material, bool bInWireframe, EMeshPass Pass)
{
	if (Pass == EMeshPass::DepthPrepass)
	{
		RenderCommand::BindPipelineState(*FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Depth));
		if (bInWireframe)
			RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
		return;
	}

	// TODO: TEMP: 임시
	if (Pass == EMeshPass::Shadow)
	{
		RenderCommand::BindPipelineState(*FRenderResourceManager::GetPSO(EPSOType::StaticMesh_Shadow));
		return;
	}

	EPSOType PSOType = material->PSOType;
	bool bNormalMap = material->Textures.Num() > 1 && material->Textures[1];

	if (PSOType == EPSOType::StaticMesh_Opaque)
	{
		if (Pass == EMeshPass::GBuffer)
		{
			if (FPipelineState* PSO = FRenderResourceManager::GetPSO(EPSOType::StaticMesh_GBuffer))
			{
				RenderCommand::BindPipelineState(*PSO);

				if (bInWireframe)
					RenderCommand::SetRasterizerState(ERasterizerState::Wireframe);
			}
		}
		else
		{
			RenderCommand::BindPipelineState(*CurrentOpaquePSO);
		}
	}
	else
	{
		if (FPipelineState* PSO = FRenderResourceManager::GetPSO(PSOType))
		{
			RenderCommand::BindPipelineState(*PSO);
		}
	}

	if (Pass == EMeshPass::ForwardAfterPrepass)
		RenderCommand::SetDepthStencilState(EDepthStencilState::ReadOnlyLessEqual);

	// 텍스처와 샘플러 바인딩
	for (size_t i = 0; i < material->Textures.size(); i++)
	{
		RenderCommand::BindShaderResource(static_cast<uint32>(i), material->Textures[i], EShaderBindFlagBits::Pixel);
	}
	RenderCommand::BindSamplerState(0, material->SamplerState, EShaderBindFlagBits::Pixel);
	// StaticMeshUberLit의 타일 목록은 t8/t9. 재질 텍스처 설정 후 연결한다.
	if (CurrentTileLights)
	{
		RenderCommand::BindShaderResource(8, CurrentTileLights->TileLightCountSRV.Get(), EShaderBindFlagBits::Pixel);
		RenderCommand::BindShaderResource(9, CurrentTileLights->TileLightIndicesSRV.Get(), EShaderBindFlagBits::Pixel);
	}
}

// b1 내용 채우고 꽂기
void FRenderer::UpdateMaterialParams(const FRenderPacket& RenderPacket)
{

	if (!RenderPacket.Material || !RenderPacket.Material->ParamBuffer)
	{
		return;
	}

	switch (RenderPacket.Material->PSOType)
	{
	case EPSOType::StaticMesh_Opaque:
	case EPSOType::StaticMesh_Translucent:
	case EPSOType::StaticMesh_Wireframe:
	{
		const float TotalTime = EngineTimer::GetTotalTime();
		FStaticMeshMaterialParams Params{};
		Params.BaseColor = RenderPacket.Material->BaseColor;
		Params.SpecularColor = RenderPacket.Material->SpecularColor;
		Params.Shininess = RenderPacket.Material->Shininess;
		Params.UVOffset = RenderPacket.Material->UVScrollSpeed * TotalTime;
		Params.bUseNormalMap = RenderPacket.Material->Textures.Num() > 1 && RenderPacket.Material->Textures[1] != nullptr;
		RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
		RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		break;
	}
	case EPSOType::Particle_AlphaBlend:
	case EPSOType::Particle_Additive:
	{
		if (RenderPacket.MaterialParamData != nullptr)
		{
			RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
			RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		}
		break;
	}
	default:
		break;
	}
}

// b0 MVP 채우고 꽂기
void FRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection)
{
	// rp.Transform 과 Camera VP 행렬 곱
	// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요

	bool ProxyReceiverShadow = RenderPacket.Proxy ?
		RenderPacket.Proxy->GetReceiveShadow() : true;
	UpdatePerObjectConstants(GetPacketWorld(RenderPacket), ProxyReceiverShadow);

}

void FRenderer::UpdatePerObjectConstants(const FMatrix& World, bool ReceiverShadow)
{
	const FPerObjectConstants Constants =
		MakePerObjectConstants(World, ReceiverShadow);

	RenderCommand::UpdateBufferData(
		PerObjectCB.get(),
		&Constants,
		sizeof(Constants));
}
