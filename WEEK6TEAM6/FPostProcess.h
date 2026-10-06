#pragma once

#include "FRenderGraph.h"
#include "Matrix.h"

struct FPostProcessInputs
{
    FRGTextureRef InputColorTexture = InvalidRGTextureRef;
    FRGTextureRef InputDepthTexture = InvalidRGTextureRef;

    // If set, the post-process will write to this texture instead of the default output.
    FRGTextureRef OverrideOutputTexture = InvalidRGTextureRef;
};

struct FPostProcessContext
{
	FMatrix ViewProjectionMatrix;
	FMatrix InvViewProjectionMatrix;
    FVector ViewPosition;
    float NearPlane;
    float FarPlane;
};

class FPostProcess
{
public:
    virtual ~FPostProcess();

	virtual void SetEnabled(bool bEnabled) { this->bEnabled = bEnabled; }
    virtual bool IsEnabled() const { return bEnabled; }

    virtual FRGTextureRef AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context) = 0;

private:
	bool bEnabled = true;
};
