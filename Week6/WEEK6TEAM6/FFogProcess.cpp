#include "FFogProcess.h"
#include <stdexcept>

FRGTextureRef FFogProcess::AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context)
{
    if (Inputs.OverrideOutputTexture == InvalidRGTextureRef)
    {
        throw std::runtime_error("FFogProcess: OverrideOutputTexture must be set");
    }

    RenderGraph.AddPass(FName("FogProcess"), [this, &RenderGraph, Inputs, Context](URenderer& Renderer)
    {
		if (!FogPipeline)
		{
			FogPipeline = Renderer.CreateRenderPipeline();
			FogPipeline->SetShader("Assets/Shaders/FogProcess.hlsl");
            FogPipeline->SetRasterRizerState(D3D11_CULL_BACK);
            FogPipeline->SetDepthStencilState(false, false);
            FogPipeline->SetBlendState(ERenderBlendMode::Transparent);
            FogPipeline->AddConstantBuffer<FFogConstants>();
            FogPipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
		}

        FRGTexture& OutputTexture = RenderGraph.GetTexture(Inputs.OverrideOutputTexture);
		FRGTexture& InputColorTexture = RenderGraph.GetTexture(Inputs.InputColorTexture);
		FRGTexture& InputDepthTexture = RenderGraph.GetTexture(Inputs.InputDepthTexture);

		RenderTarget.Texture = OutputTexture.Texture->Texture;
		RenderTarget.RTV = OutputTexture.RTV.Get();
		RenderTarget.Width = OutputTexture.Texture->Width;
		RenderTarget.Height = OutputTexture.Texture->Height;

		FogConstants.InvViewProjectionMatrix = Context.InvViewProjectionMatrix;
		FogConstants.ViewMatrix = Context.ViewMatrix;
		FogConstants.ViewPosition = Context.ViewPosition;

		FogPipeline->SetShaderResource(0, InputColorTexture.SRV);
		FogPipeline->SetShaderResource(1, InputDepthTexture.SRV);
		FogPipeline->UpdateConstantBuffer(0, FogConstants);

		Renderer.BindRenderTarget(&RenderTarget, nullptr, true);
		Renderer.Render(FogPipeline.get(), 6);
		Renderer.ClearAllShaderResources();
    });

    return Inputs.OverrideOutputTexture;
}
