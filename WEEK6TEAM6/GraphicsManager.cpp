#include "GraphicsManager.h"
#include "FInstrumentor.h"
#include "Renderer.h"
#include "Camera.h"
#include "Console.h"
#include "FLogManager.h"
#include "FAssetManager.h"
#include "Assets.h"
#include "ObjectFactory.h"
#include "UTextComponent.h"
#include "FEditorViewportClient.h"
#include "World.h"
//#include "FInstrumentor.h"
#include <algorithm>
#include "FHiZOcclusionManager.h"

// 선분 하나당 정점 2개. 축 6개 + 앞으로 붙을 그리드까지 감당할 만큼 잡아둔다
static constexpr uint32 LINE_VERTEX_CAPACITY = 8192;

FGraphicsManager::FGraphicsManager(HWND hWindow) 
	: mbPerspectiveProjection(true)
	, mProjectionRatio(1.0f)
{
	mRenderer = new URenderer;
	mRenderer->Create(hWindow);

	mRenderGraph.Initialize(*mRenderer);

	mAspect = mRenderer->GetWidth() / static_cast<float>(mRenderer->GetHeight());

	mLightInfoBuffer = mRenderer->CreateStructuredBuffer<FLightInfo>(1);

	mMeshPipeline = mRenderer->CreateRenderPipeline();
	mMeshPipeline->SetRasterRizerState(D3D11_CULL_BACK, 0, { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe });
	mMeshPipeline->SetDepthStencilState(true, true);
	mMeshPipeline->SetShader("Assets/Shaders/StaticMeshShader.hlsl");
	mMeshPipeline->AddConstantBuffer<FMeshContants>();
	mMeshPipeline->AddConstantBuffer<FMatrix>();

	mHighlightMarkPipeline = mRenderer->CreateRenderPipeline();
	mHighlightMarkPipeline->SetRasterRizerState(D3D11_CULL_BACK);
	mHighlightMarkPipeline->SetDepthStencilState(true, false, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_REPLACE);
	mHighlightMarkPipeline->SetBlendState(ERenderBlendMode::Opaque, false);
	mHighlightMarkPipeline->SetShader("Assets/Shaders/StaticMeshShader.hlsl");
	mHighlightMarkPipeline->AddConstantBuffer<FMeshContants>();
	mHighlightMarkPipeline->AddConstantBuffer<FMatrix>();
	mHighlightMarkPipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);

	mHighlightDrawPipeline = mRenderer->CreateRenderPipeline();
	mHighlightDrawPipeline->SetRasterRizerState(D3D11_CULL_BACK);
	mHighlightDrawPipeline->SetDepthStencilState(false, false, D3D11_COMPARISON_NOT_EQUAL, D3D11_STENCIL_OP_KEEP);
	mHighlightDrawPipeline->SetShader("Assets/Shaders/Outline.hlsl");
	mHighlightDrawPipeline->AddConstantBuffer<FOutlineConstants>();

	mHighlightVertexBuffer = mRenderer->CreateVertexBuffer<FVertex>(nullptr, 1024, D3D11_USAGE_DYNAMIC); // 초기 용량 1024개, 필요하면 늘어난다
	mHighlightIndexBuffer = mRenderer->CreateIndexBuffer(nullptr, 1024, D3D11_USAGE_DYNAMIC); // 초기 용량 1024개, 필요하면 늘어난다
	
	GpuQueries.SetNum(3);

	for (FGpuTimerQuerySet& QuerySet : GpuQueries)
	{
		D3D11_QUERY_DESC QueryDesc{};
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		mRenderer->GetDevice()->CreateQuery(&QueryDesc, &QuerySet.Disjoint);

		QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
		mRenderer->GetDevice()->CreateQuery(&QueryDesc, &QuerySet.Begin);
		mRenderer->GetDevice()->CreateQuery(&QueryDesc, &QuerySet.End);
	}
}

FGraphicsManager::~FGraphicsManager()
{
	mHighlightVertexBuffer.reset();
	mHighlightIndexBuffer.reset();
	mHighlightMarkPipeline.reset();
	mHighlightDrawPipeline.reset();
	mMeshPipeline.reset();
	mRenderCollector.Clear();
	mRenderer->Release();
	delete mRenderer;
}

void FGraphicsManager::Prepare(const FCamera* mCamera, float Aspect, const FMatrix& ViewProjection, const FMatrix& InvViewProjection, FViewport& Viewport, UWorld* World, const EViewModeIndex InViewMode, const EViewportType InViewportType)
{
	//PROFILE_SCOPE("Viewport/Prepare");

	mViewportType = InViewportType;
	const bool bIsOrtho = (InViewportType != EViewportType::Perspective);

	float d = mCamera->mOrthoDistance;
	mAspect = Aspect;

	FMatrix view = mCamera->GetViewMatrix();
	FMatrix projection_u_o = mCamera->GetUnifiedProjectionMatrix(d, 0.0f);
	FMatrix projection_u = mCamera->GetUnifiedProjectionMatrix(d, bIsOrtho ? 0.0f : mProjectionRatio);

	mViewMatrix = view;
	mProjectionMatrix = projection_u;
	mViewProjectionMatrix = ViewProjection;
	mInvViewProjectionMatrix = InvViewProjection;
	mViewport = &Viewport;

	// 뷰 모드를 렌더러에 전달한다. BindPipeline이 드로우마다 이 값을 보고
	// 솔리드/와이어프레임 래스터라이저를 고른다.
	mViewModeIndex = InViewMode;
	mRenderer->SetViewModeIndex(mViewModeIndex);

	mRenderer->Prepare(view * projection_u);

	float orthoHeight = mCamera->mOrthoHeight;
	float orthoWidth = orthoHeight * mAspect;
	//mViewOrthogonalProjectionMatrix = view * mCamera->GetOrthographicMatrix(orthoWidth, orthoHeight, nearZ, farZ);
	mViewOrthogonalProjectionMatrix = view * projection_u_o;
	mViewUnifiedProjectionMatrix = view * projection_u;

	// 하이라이트 두께를 화면 픽셀 기준으로 환산할 때 쓴다
	mCameraLocation = mCamera->Transform.GetLocation();
	mCameraForward = mCamera->GetForwardVector();
	mCameraFovDegree = mCamera->mFovDegree;
	mCameraOrthoDistance = mCamera->mOrthoDistance;
	mCameraNear = mCamera->mNear;
	mCameraFar = mCamera->mFar;

	// NOTE: LightInfos를 모으고 StructuredBuffer에 업데이트합니다.
	mLightInfos.Empty();
	for (TObjectIterator<UPointLightComponent> It; It; ++It)
	{
		UPointLightComponent* PointLight = *It;
		if (PointLight->GetOwner()->GetWorld() != World)
		{
			continue;
		}

		FLightInfo& Info = mLightInfos.Emplace();
		Info.Type = ELightType::Point;
		Info.Position = PointLight->GetWorldLocation();
		Info.Color = PointLight->GetColor();
		Info.Intensity = PointLight->GetIntensity();
		Info.Range = PointLight->GetRadius();
		Info.FallOf = PointLight->GetRadiusFallOff();
	}

	if (mLightInfos.Num() * sizeof(FLightInfo) > mLightInfoBuffer->GetBufferSize())
	{
		mLightInfoBuffer = mRenderer->CreateStructuredBuffer<FLightInfo>(mLightInfos.Num());
	}

	if (!mLightInfos.IsEmpty())
	{
		mLightInfoBuffer->UpdateBuffer(mLightInfos.Data(), mLightInfos.Num());
	}

	mRenderer->BindRenderTarget(Viewport.GetFrontRenderTarget(), Viewport.GetDepthStencil());
}

void FGraphicsManager::RenderHighLight(const TArray<UPrimitiveComponent*>& Primitives)
{
	//PROFILE_SCOPE("Viewport/RenderHighLight");

	if (Primitives.Num() == 0)
	{
		return;
	}

	FRenderTarget2D* CurrentRenderTarget = mRenderer->GetBindedRenderTarget();
	FDepthStencil* CurrentDepthStencil = mRenderer->GetBindedDepthStencil();

	if (CurrentDepthStencil == nullptr)
	{
		UE_DEBUG_LOG_WARN("RenderHighLight: CurrentDepthStencil is nullptr. Skipping highlight rendering.");
		return;
	}

	mHighlightMarkPipeline->UpdateConstantBuffer(1, mViewUnifiedProjectionMatrix);
	mHighlightDrawPipeline->UpdateConstantBuffer(1, mViewUnifiedProjectionMatrix);

	// Mark Pass: 스텐실에 마크만 찍는다.
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		const TArray<FVertex>& Vertices = Primitive->GetMeshVertices();
		const TArray<uint32>& Indices = Primitive->GetMeshIndices();

		if (Vertices.Num() * sizeof(FVertex) > mHighlightVertexBuffer->GetBufferSize())
		{
			mHighlightVertexBuffer = mRenderer->CreateVertexBuffer<FVertex>(Vertices.Data(), Vertices.Num(), D3D11_USAGE_DYNAMIC);
		}

		if (Indices.Num() * sizeof(uint32) > mHighlightIndexBuffer->GetBufferSize())
		{
			mHighlightIndexBuffer = mRenderer->CreateIndexBuffer(Indices.Data(), Indices.Num(), D3D11_USAGE_DYNAMIC);
		}

		mHighlightVertexBuffer->UpdateBuffer(Vertices.Data(), Vertices.Num());
		mHighlightIndexBuffer->UpdateBuffer(Indices.Data(), Indices.Num());

		FMeshContants Constants{};
		Constants.Matrix = Primitive->GetWorldMatrix();
		Constants.Color = FVector4(0.f, 0.f, 0.f, 0.f);
		Constants.HasTexture = 0;
		Constants.UseVertexColor = 0;
		Constants.UVOffset = FVector2(0.f, 0.f);
		Constants.LightCount = 0;

		mHighlightMarkPipeline->UpdateConstantBuffer(0, Constants);

		FRenderInfo RenderInfo{};
		RenderInfo.VertexBuffer = mHighlightVertexBuffer->Buffer.Get();
		RenderInfo.VertexCount = static_cast<uint32>(Vertices.Num());
		RenderInfo.IndexBuffer = mHighlightIndexBuffer->Buffer.Get();
		RenderInfo.StartIndex = 0;
		RenderInfo.IndexCount = static_cast<uint32>(Indices.Num());
		RenderInfo.Model = Primitive->GetWorldMatrix();

		mRenderer->RenderPrimitiveIndexed(mHighlightMarkPipeline.get(), RenderInfo, 1);
	}

	// Draw Pass: 잠시 DepthStencil을 해제
	mRenderer->BindRenderTarget(CurrentRenderTarget, nullptr, false);
	mHighlightDrawPipeline->SetShaderResource(0, CurrentDepthStencil->StencilSRV);

	// Draw Pass: 스텐실에 마크가 찍힌 영역만 그린다.
	FOutlineConstants OutlineConstants{};
	OutlineConstants.OutlineColor = FVector4(1.f, 0.6f, 0.f, 1.f);
	OutlineConstants.StencilTexWidth = CurrentDepthStencil->Width;
	OutlineConstants.StencilTexHeight = CurrentDepthStencil->Height;
	OutlineConstants.OutlineRadius = 5;
	
	mHighlightDrawPipeline->UpdateConstantBuffer(0, OutlineConstants);

	mRenderer->Render(mHighlightDrawPipeline.get(), 6);

	// Draw Pass가 끝나면 원래 DepthStencil을 복원한다.
	mRenderer->ClearAllShaderResources();
	mRenderer->BindRenderTarget(CurrentRenderTarget, CurrentDepthStencil, false);
}

void FGraphicsManager::Render()
{
	mRenderGraph.Clear();

	FRGTextureRef FrontRenderTargetHandle = mRenderGraph.RegisterExternalTexture(mViewport->GetFrontRenderTarget());
	FRGTextureRef BackRenderTargetHandle = mRenderGraph.RegisterExternalTexture(mViewport->GetBackRenderTarget());
	FRGTextureRef DepthStencilHandle = mRenderGraph.RegisterExternalTexture(mViewport->GetDepthStencil());

	// TODO: 후에 아래 코드들을 ScenePass로 옮기고, ScenePass에서 RenderCollector를 받아서 처리하도록 한다.
    PROFILE_SCOPE("Viewport/GraphicsRender");
    {
        PROFILE_SCOPE("Viewport/GraphicsRender/RenderLines");
        mRenderer->RenderLines(mRenderCollector.LineInfos);
    }

	const auto& RenderInfoPool = mRenderCollector.GetRenderInfoPool();
	const auto& RenderInfos = RenderInfoPool.GetPool();
	auto& VisibleRenderInfoIndices = mRenderCollector.GetVisibleRenderInfoIndices();

    {
        PROFILE_SCOPE("Viewport/GraphicsRender/sort");
        std::sort(VisibleRenderInfoIndices.begin(), VisibleRenderInfoIndices.end(), [&RenderInfoPool, &RenderInfos, &VisibleRenderInfoIndices](int32 A, int32 B) { 
			return RenderInfos[A].SortKey < RenderInfos[B].SortKey;
		});
    }

	{
		PROFILE_SCOPE("Viewport/GraphicsRender/SubmitMeshes");

		// 카메라 상수 갱신은 Renderer의 영속 바인딩 캐시와 별도로 뷰마다 수행합니다.
		FRenderPipeline* LastViewPipeline = nullptr;

		for (const int32 Index : VisibleRenderInfoIndices)
		{
			const FRenderInfo& Info = RenderInfos[Index];

			const auto& Pipeline = Info.Pipeline ? Info.Pipeline : mMeshPipeline.get();

			if (Pipeline != LastViewPipeline)
			{
				Pipeline->UpdateConstantBuffer(1, mViewUnifiedProjectionMatrix);
				Pipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
			}

			if (Info.Texture)
			{
				Pipeline->SetShaderResource(0, Info.Texture->GetSRV());
			}

			Pipeline->SetShaderResource(1, mLightInfoBuffer->SRV);
			
			// 개별 Draw의 상수는 기존 동적 상수 버퍼에 Map/Unmap으로 갱신합니다.
			FMeshContants Constants;
			Constants.Matrix = Info.Model;
			Constants.Color = Info.Color;
			Constants.UVOffset = Info.UVOffset;
			Constants.UseVertexColor = Info.UseVertexColor;
			Constants.HasTexture = Info.Texture ? 1 : 0;
			Constants.LightCount = mLightInfos.Num();
			
			Pipeline->UpdateConstantBuffer(0, Constants);

			if (Info.IndexBuffer)
			{
				mRenderer->RenderPrimitiveIndexed(Pipeline, Info);
			}
			else
			{
				mRenderer->RenderPrimitive(Pipeline, Info.VertexBuffer, Info.VertexCount);
			}

			LastViewPipeline = Pipeline;
		}
	}
	
#if ENABLE_OCCULSION_CULLING
	// Hi-Z Occlusion Culling: Downsamples depth buffer into Hi-Z pyramid and tests scene AABBs
	if (FShowFlags::Get().IsEnabled(EShowFlag::OcclusionCulling) && mViewportType == EViewportType::Perspective)
	{
		PROFILE_SCOPE("Viewport/GraphicsRender/HiZOcclusion");
		FDepthStencil* CurrentDepthStencil = mRenderer->GetBindedDepthStencil();
		if (CurrentDepthStencil)
		{
			FHiZOcclusionManager::Get().GenerateHiZAndDispatchCull(mRenderer, CurrentDepthStencil, mViewUnifiedProjectionMatrix, mCameraNear, mCameraFar);
		}
	}
#endif

	{
		PROFILE_SCOPE("Viewport/GraphicsRender/RenderQuad");

		const auto& RenderQuadInfoPool = mRenderCollector.GetRenderQuadInfoPool();
		const auto& RenderQuadInfos = RenderQuadInfoPool.GetPool();
		auto& VisibleRenderInfoIndices = mRenderCollector.GetVisibleRenderQuadInfoIndices();
		for (const uint32 Index : VisibleRenderInfoIndices)
		{
			const FRenderQuadInfo& QuadInfo = RenderQuadInfos[Index];
			mRenderer->RenderQuad(QuadInfo);
		}

		const auto& RenderTransparentQuadInfoPool = mRenderCollector.GetRenderTransparentQuadInfoPool();
		const auto& RenderTransparentQuadInfos = RenderTransparentQuadInfoPool.GetPool();
		auto& VisibleRenderTransparentInfoIndices = mRenderCollector.GetVisibleRenderTransparentQuadInfoIndices();
		for (const uint32 Index : VisibleRenderTransparentInfoIndices)
		{
			const FRenderQuadInfo& QuadInfo = RenderTransparentQuadInfos[Index];
			mRenderer->RenderQuad(QuadInfo);
		}
	}

	mFogProcess.SetEnabled(FShowFlags::Get().IsEnabled(EShowFlag::Fog) && mFogProcess.HasFogComponent());
	mFXAAProcess.SetEnabled(FShowFlags::Get().IsEnabled(EShowFlag::FXAA));
	mDepthPreviewProcess.SetEnabled(FShowFlags::Get().IsEnabled(EShowFlag::SceneDepth));

	FPostProcess* PostProcesses[] = { 
		&mFogProcess,
		&mFXAAProcess,
		&mDepthPreviewProcess,
	};

	FPostProcessContext PostProcessContext;
	PostProcessContext.ViewProjectionMatrix = mViewProjectionMatrix;
	PostProcessContext.InvViewProjectionMatrix = mInvViewProjectionMatrix;
	PostProcessContext.ViewPosition = mCameraLocation;
	PostProcessContext.NearPlane = mCameraNear;
	PostProcessContext.FarPlane = mCameraFar;

	for (FPostProcess* PostProcess : PostProcesses)
	{
		if (!PostProcess->IsEnabled())
		{
			continue;
		}

		FPostProcessInputs PostProcessInputs;
		PostProcessInputs.InputColorTexture = FrontRenderTargetHandle;
		PostProcessInputs.InputDepthTexture = DepthStencilHandle;
		PostProcessInputs.OverrideOutputTexture = BackRenderTargetHandle;

		PostProcess->AddPasses(mRenderGraph, PostProcessInputs, PostProcessContext);

		std::swap(FrontRenderTargetHandle, BackRenderTargetHandle);
		mViewport->Swap();
	}

	mRenderGraph.Execute();

	mRenderer->BindRenderTarget(mViewport->GetFrontRenderTarget(), mViewport->GetDepthStencil(), false);

	if (FShowFlags::Get().IsEnabled(EShowFlag::Grid))
	{
		FMatrix GridWorldMatrix = FMatrix::Identity;

		if (mViewportType == EViewportType::Front)
		{
			GridWorldMatrix = FMatrix::RotateY(90);
		}
		else if (mViewportType == EViewportType::Side)
		{
			GridWorldMatrix = FMatrix::RotateX(90);
		}

		// Match the grid's world-space half-width of 0.001.
		mRenderer->RenderWorldAxis(mViewMatrix, mProjectionMatrix, FVector4(0.f, 0.f, 1.f, 1.f), FVector3(0.f, 0.f, 1.f), 0.002f);
		mRenderer->RenderWorldGrid(GridWorldMatrix * mViewUnifiedProjectionMatrix, mCameraLocation, GridGap);
	}

	const auto& RenderOverlayQuadInfoPool = mRenderCollector.GetRenderOverlayQuadInfoPool();
	const auto& RenderOverlayQuadInfos = RenderOverlayQuadInfoPool.GetPool();
	const auto& VisibleRenderOverlayInfoIndices = mRenderCollector.GetVisibleRenderOverlayQuadInfoIndices();
	for (const uint32 Index : VisibleRenderOverlayInfoIndices)
	{
		const FRenderQuadInfo& QuadInfo = RenderOverlayQuadInfos[Index];
		mRenderer->RenderQuad(QuadInfo);
	}

	for (const FRenderQuad2DInfo& Quad2DInfo : mRenderCollector.GetQuad2DInfos())
	{
		mRenderer->RenderQuad2D(Quad2DInfo);
	}
}

void FGraphicsManager::Display()
{
	mRenderer->SwapBuffer();
}

bool FGraphicsManager::IsPerspectiveProjection() const
{
	return mbPerspectiveProjection;
}

void FGraphicsManager::SetPerspectiveProjection(bool bPerspectiveProjection)
{
	mbPerspectiveProjection = bPerspectiveProjection;
}

URenderer* FGraphicsManager::GetRenderer() const
{
	assert(mRenderer != nullptr);

	return mRenderer;
}

void FGraphicsManager::OnResize(UINT width, UINT height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	mRenderer->OnResize(width, height);
}

// 월드 공간 반지름이 worldHalfExtent인 축을 worldThickness 만큼 키우는 배율
static float GetOutlineAxisScale(float worldHalfExtent, float worldThickness)
{
	if (worldHalfExtent <= SMALL_NUMBER)
	{
		return 1.0f;   // 납작하게 눌린 축은 건드리지 않는다. 안 그러면 배율이 발산한다
	}

	return 1.0f + worldThickness / worldHalfExtent;
}

void FGraphicsManager::StartProjectionTransition(bool orthographic)
{
	mProjectionStartRatio = mProjectionRatio;
	mProjectionTargetRatio = orthographic ? 0.0f : 1.0f;
	mProjectionElapsed = 0.0f;

	mbProjectionTransitioning = mProjectionStartRatio != mProjectionTargetRatio;
}

bool FGraphicsManager::IsOrthographicTarget() const
{
	return mProjectionTargetRatio == 0.0f;
}

void FGraphicsManager::UpdateProjectionTransition(float deltaTime)
{
	if (!mbProjectionTransitioning)
	{
		return;
	}

	mProjectionElapsed += deltaTime;

	const float u = FMath::Clamp(
		mProjectionElapsed / mProjectionDuration, 0.0f, 1.0f);

	// Smoothstep interpolation for a smoother transition
	const float blend = u * u * (3.0f - 2.0f * u);

	mProjectionRatio = mProjectionStartRatio + (mProjectionTargetRatio - mProjectionStartRatio) * blend;

	if (u >= 1.0f)
	{
		mProjectionRatio = mProjectionTargetRatio;
		mbProjectionTransitioning = false;
	}
}

void FGraphicsManager::SetGridGap(int32 GridGap)
{
	if (GridGap > 75000)
		GridGap = 100000;
	else if (GridGap > 30000)
		GridGap = 50000;
	else if (GridGap > 7500)
		GridGap = 10000;
	else if (GridGap > 3000)
		GridGap = 5000;
	else if (GridGap > 750)
		GridGap = 1000;
	else if (GridGap > 300)
		GridGap = 500;
	else if (GridGap > 75)
		GridGap = 100;
	else if (GridGap > 30)
		GridGap = 50;
	else if (GridGap > 7)
		GridGap = 10;
	else if (GridGap > 3)
		GridGap = 5;
	else
		GridGap = 1;
	this->GridGap = GridGap;
}

void FGraphicsManager::BeginGpuRenderTimer()
{
	bGpuTimerActive = false;

	ID3D11DeviceContext* Context = mRenderer->GetDeviceContext();
	FGpuTimerQuerySet& QuerySet = GpuQueries[GpuQueryIndex];

	if (QuerySet.bIssued)
	{
		return; // 이전 결과 미회수
	}

	Context->Begin(QuerySet.Disjoint.Get());
	Context->End(QuerySet.Begin.Get());

	bGpuTimerActive = true;
}

void FGraphicsManager::EndGpuRenderTimer()
{
	if (!bGpuTimerActive)
	{
		return;
	}

	ID3D11DeviceContext* Context = mRenderer->GetDeviceContext();
	FGpuTimerQuerySet& QuerySet = GpuQueries[GpuQueryIndex];

	Context->End(QuerySet.End.Get());
	Context->End(QuerySet.Disjoint.Get());

	QuerySet.bIssued = true;

	GpuQueryIndex = (GpuQueryIndex + 1) % GpuQueries.Num();

	bGpuTimerActive = false;
}

void FGraphicsManager::UpdateGpuRenderTime()
{
	ID3D11DeviceContext* Context = mRenderer->GetDeviceContext();
	FGpuTimerQuerySet& QuerySet = GpuQueries[GpuQueryIndex];

	if (!QuerySet.bIssued)
	{
		return;
	}

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Disjoint{};
	UINT64 BeginTimestamp = 0;
	UINT64 EndTimestamp = 0;

	if (Context->GetData(QuerySet.Disjoint.Get(), &Disjoint, sizeof(Disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
	{
		return; 
	}

	if (Context->GetData(QuerySet.Disjoint.Get(), &Disjoint, sizeof(Disjoint), 0) != S_OK ||
		Context->GetData(QuerySet.Begin.Get(), &BeginTimestamp, sizeof(BeginTimestamp), 0) != S_OK ||
		Context->GetData(QuerySet.End.Get(), &EndTimestamp, sizeof(EndTimestamp), 0) != S_OK)
	{
		return; // 아직 GPU가 해당 프레임을 끝내지 않음
	}

	if (!Disjoint.Disjoint)
	{
		GpuRenderTime =
			static_cast<float>(EndTimestamp - BeginTimestamp) * 1000.0f /
			static_cast<float>(Disjoint.Frequency);
	}

	QuerySet.bIssued = false;
}
