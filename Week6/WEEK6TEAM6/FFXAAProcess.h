#pragma once

#include "FPostProcess.h"
#include "FRenderGraph.h"
#include "FRenderPipeline.h"
#include "Renderer.h"

class FFXAAProcess : public FPostProcess
{
public:
	FFXAAProcess()
	{
		SetEnabled(false);
	}

	~FFXAAProcess() override = default;

	FRGTextureRef AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context) override;

private:
	struct FFXAAContants
	{
		float Width;
		float Height;
		float Padding[2];
	};

	TSharedPtr<FRenderPipeline> Pipeline;

	FFXAAContants Contants;
	FRenderTarget2D RenderTarget;
};
