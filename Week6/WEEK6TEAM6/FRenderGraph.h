#pragma once

#include "Core.h"
#include "FName.h"
#include "TArray.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <functional>

struct FTexture2D;
struct FRenderTarget2D;
struct FDepthStencil;
class URenderer;

using FRGTextureRef = int32;

constexpr FRGTextureRef InvalidRGTextureRef = -1;

struct FRGTexture
{
    FTexture2D* Texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DSV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
};

class FRenderGraph
{
public:
    using FExecuteFunc = std::function<void(URenderer&)>;

    void Initialize(URenderer& InRenderer);

    FRGTextureRef RegisterExternalTexture(FRenderTarget2D* Texture);
    FRGTextureRef RegisterExternalTexture(FDepthStencil* Texture);
    
    FRGTexture& GetTexture(FRGTextureRef Handle);

    void AddPass(const FName& PassName, const FExecuteFunc& ExecuteFunc);

    void Execute();

    void Clear();

private:
    struct FRenderPass
    {
        FName Name;
        FExecuteFunc ExecuteFunc;
    };

	URenderer* Renderer;

    TArray<FRGTexture> TextureHandles;
    TArray<FRenderPass> Passes;
};
