#pragma once

#include "Texture.h"

class FTexture2DArray : public FTexture
{
public:
	FTexture2DArray(
		ID3D11Device* Device, 
		const D3D11_TEXTURE2D_DESC& InDesc, 
		const TArray<const void*>& InitialDatas = {}
	);

	~FTexture2DArray() = default;

	uint32 GetArraySize() const { return ArraySize; }

	ID3D11RenderTargetView* GetSliceRTV(int32 i) const
	{
		return i < RTVs.Num() ? RTVs[i].Get() : nullptr;
	}

	ID3D11DepthStencilView* GetSliceDSV(int32 i) const
	{
		return i < DSVs.Num() ? DSVs[i].Get() : nullptr;
	}

	ID3D11ShaderResourceView* GetSliceSRV(int32 i) const
	{
		return i < SRVs.Num() ? SRVs[i].Get() : nullptr;
	}

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);

private:
	uint32 ArraySize = 0;
	TArray<ComPtr<ID3D11RenderTargetView>> RTVs;
	TArray<ComPtr<ID3D11DepthStencilView>> DSVs;
	TArray<ComPtr<ID3D11ShaderResourceView>> SRVs;
};