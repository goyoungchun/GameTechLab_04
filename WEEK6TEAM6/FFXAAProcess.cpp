#include "FFXAAProcess.h"

FRGTextureRef FFXAAProcess::AddPasses(FRenderGraph& RenderGraph, const FPostProcessInputs& Inputs, const FPostProcessContext& Context)
{
	if (Inputs.OverrideOutputTexture == InvalidRGTextureRef)
	{
		throw std::runtime_error("FFXAAProcess: OverrideOutputTexture must be set");
	}

	RenderGraph.AddPass(FName("FFXAAProcess"), [this, &RenderGraph, Inputs, Context](URenderer& Renderer) {
		if (!Pipeline)
		{
			Pipeline = Renderer.CreateRenderPipeline();
			Pipeline->SetShader("Assets/Shaders/FXAAProcess.hlsl");
			Pipeline->SetRasterRizerState(D3D11_CULL_BACK);
			Pipeline->SetDepthStencilState(false, false);
			Pipeline->SetBlendState(ERenderBlendMode::Opaque);
			Pipeline->AddConstantBuffer<FFXAAContants>();
			Pipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
		}

		FRGTexture& OutputTexture = RenderGraph.GetTexture(Inputs.OverrideOutputTexture);
		FRGTexture& InputColorTexture = RenderGraph.GetTexture(Inputs.InputColorTexture);

		RenderTarget.Texture = OutputTexture.Texture->Texture;
		RenderTarget.RTV = OutputTexture.RTV.Get();
		RenderTarget.Width = OutputTexture.Texture->Width;
		RenderTarget.Height = OutputTexture.Texture->Height;

		Contants.Width = static_cast<float>(InputColorTexture.Texture->Width);
		Contants.Height = static_cast<float>(InputColorTexture.Texture->Height);

		Pipeline->SetShaderResource(0, InputColorTexture.SRV);
		Pipeline->UpdateConstantBuffer(0, Contants);

		Renderer.BindRenderTarget(&RenderTarget, nullptr, true);
		Renderer.Render(Pipeline.get(), 6);
		Renderer.ClearAllShaderResources();
	});

	return Inputs.OverrideOutputTexture;
}
