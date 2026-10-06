#include "FDepthPreviewProcess.h"

FRGTextureRef FDepthPreviewProcess::AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context)
{
	if (Inputs.OverrideOutputTexture == InvalidRGTextureRef)
	{
		throw std::runtime_error("FDepthPreviewProcess: OverrideOutputTexture must be set");
	}

	RenderGraph.AddPass(FName("DepthPreviewProcess"), [this, &RenderGraph, Inputs, Context](URenderer& Renderer)
		{
			if (!Pipeline)
			{
				Pipeline = Renderer.CreateRenderPipeline();
				Pipeline->SetShader("Assets/Shaders/DepthPreviewProcess.hlsl");
				Pipeline->SetRasterRizerState(D3D11_CULL_BACK);
				Pipeline->SetDepthStencilState(false, false);
				Pipeline->SetBlendState(ERenderBlendMode::Transparent);
				Pipeline->AddConstantBuffer<FDepthPreviewConstants>();
				Pipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
			}

			FRGTexture& OutputTexture = RenderGraph.GetTexture(Inputs.OverrideOutputTexture);
			FRGTexture& InputColorTexture = RenderGraph.GetTexture(Inputs.InputColorTexture);
			FRGTexture& InputDepthTexture = RenderGraph.GetTexture(Inputs.InputDepthTexture);

			RenderTarget.Texture = OutputTexture.Texture->Texture;
			RenderTarget.RTV = OutputTexture.RTV.Get();
			RenderTarget.Width = OutputTexture.Texture->Width;
			RenderTarget.Height = OutputTexture.Texture->Height;

			DepthPreviewConstants.FarPlane = Context.FarPlane;
			DepthPreviewConstants.NearPlane = Context.NearPlane;

			Pipeline->SetShaderResource(0, InputDepthTexture.SRV);
			Pipeline->UpdateConstantBuffer(0, DepthPreviewConstants);
			
			Renderer.BindRenderTarget(&RenderTarget, nullptr, true);
			Renderer.Render(Pipeline.get(), 6);
			Renderer.ClearAllShaderResources();
		});

	return Inputs.OverrideOutputTexture;
}
