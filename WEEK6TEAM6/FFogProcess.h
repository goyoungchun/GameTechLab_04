#pragma once

#include "FPostProcess.h"
#include "FRenderGraph.h"
#include "FRenderPipeline.h"
#include "Renderer.h"

class FFogProcess : public FPostProcess
{
public:
	FFogProcess()
	{
		SetEnabled(false);
	}

    ~FFogProcess() override = default;

	void RegisterFogComponent() { ++FogComponentCount; }
	void UnregisterFogComponent() { --FogComponentCount; }
	bool HasFogComponent() const { return FogComponentCount > 0; }

    FRGTextureRef AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context) override;

private:
	friend class UHeightFogComponent;

	struct FFogConstants
	{
		FMatrix InvViewProjectionMatrix;
		FVector ViewPosition;
		float FogDensity = 0.2f;
		FLinearColor FogInscatteringColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
		float FogHeightFalloff = 0.2f;
		float StartDistance = 0.0f;
		float FogCutoffDistance = 1000.0f;
		float FogMaxOpacity = 1.0f;
		float FogHeight = 0.0f;
		float Padding[3];
	};

	TSharedPtr<FRenderPipeline> FogPipeline;

	FFogConstants FogConstants;
	FRenderTarget2D RenderTarget;

	int32 FogComponentCount = 0;
};
