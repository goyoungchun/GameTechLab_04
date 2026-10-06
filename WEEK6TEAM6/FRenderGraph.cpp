#include "FRenderGraph.h"
#include "Renderer.h"
#include <stdexcept>

void FRenderGraph::Initialize(URenderer& InRenderer)
{
	Renderer = &InRenderer;
}

FRGTextureRef FRenderGraph::RegisterExternalTexture(FRenderTarget2D* Texture)
{
    FRGTextureRef Handle = TextureHandles.Num();

    FRGTexture& RenderGraphTexture = TextureHandles.Emplace();
    RenderGraphTexture.Texture = Texture;
    RenderGraphTexture.RTV = Texture->RTV;
    RenderGraphTexture.SRV = Texture->SRV;

    return Handle;
}

FRGTextureRef FRenderGraph::RegisterExternalTexture(FDepthStencil* Texture)
{
    FRGTextureRef Handle = TextureHandles.Num();

    FRGTexture& RenderGraphTexture = TextureHandles.Emplace();
    RenderGraphTexture.Texture = Texture;
    RenderGraphTexture.DSV = Texture->DSV;
    RenderGraphTexture.SRV = Texture->DepthSRV;

    return Handle;
}

FRGTexture& FRenderGraph::GetTexture(FRGTextureRef Handle)
{
    if (Handle == InvalidRGTextureRef)
    {
        throw std::runtime_error("FRenderGraph: Invalid texture handle");
    }

    return TextureHandles[Handle];
}

void FRenderGraph::AddPass(const FName& PassName, const FExecuteFunc& ExecuteFunc)
{
    Passes.Emplace(FRenderPass{ PassName, ExecuteFunc });
}

void FRenderGraph::Execute()
{
    for (int32 i = 0; i < Passes.Num(); ++i)
    {
        Passes[i].ExecuteFunc(*Renderer);
    }
}

void FRenderGraph::Clear()
{
    TextureHandles.Empty();
    Passes.Empty();
}
