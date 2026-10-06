#pragma once

#include "FPostProcess.h"
#include "FRenderGraph.h"
#include "FRenderPipeline.h"
#include "Renderer.h"

class FDepthPreviewProcess : public FPostProcess
{
public:
	FDepthPreviewProcess()
	{
		SetEnabled(false);
	}

	~FDepthPreviewProcess() override = default;

	FRGTextureRef AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context) override;

private:
	struct FDepthPreviewConstants
	{
		float NearPlane;
		float FarPlane;
	};

	TSharedPtr<FRenderPipeline> Pipeline;

	FDepthPreviewConstants DepthPreviewConstants;
	FRenderTarget2D RenderTarget;
};
