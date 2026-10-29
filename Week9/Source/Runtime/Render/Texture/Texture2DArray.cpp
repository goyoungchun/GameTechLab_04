#include "EnginePCH.h"
#include "Texture2DArray.h"

FTexture2DArray::FTexture2DArray(
	ID3D11Device* Device,
	const D3D11_TEXTURE2D_DESC& InDesc,
	const TArray<const void*>& InitialDatas
)
{
	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::Texture2DArray;
	ArraySize = InDesc.ArraySize;

	TArray<D3D11_SUBRESOURCE_DATA> SubDatas;
	if (!InitialDatas.IsEmpty())
	{
		uint32 Count = static_cast<uint32>(InitialDatas.Num());

		assert(Count == ArraySize * InDesc.MipLevels);

		SubDatas.SetNum(Count);
		for (uint32 i = 0; i < Count; ++i)
		{
			SubDatas[i].pSysMem = InitialDatas[i];
			SubDatas[i].SysMemPitch= InDesc.Width * FormatToBytes(InDesc.Format);
			SubDatas[i].SysMemSlicePitch = 0;
		}
	}

	HRESULT hr = Device->CreateTexture2D(&InDesc, InitialDatas.IsEmpty() ? nullptr : SubDatas.GetData(), (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr))
	{
		HTR_LOG(Error, "[Texture2DArray] CreateTexture2D failed (hr=0x{:08X}, {}x{})", (uint32)hr, InDesc.Width, InDesc.Height);
		return;
	}

	CreateViews(Device, InDesc);
}

void FTexture2DArray::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc)
{
	HRESULT hr;

	if (InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = InDesc.Format;
		if (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
			SrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		SrvDesc.Texture2DArray.MostDetailedMip = 0;
		SrvDesc.Texture2DArray.MipLevels = static_cast<UINT>(-1);
		SrvDesc.Texture2DArray.FirstArraySlice = 0;
		SrvDesc.Texture2DArray.ArraySize = ArraySize;

		hr = Device->CreateShaderResourceView(Texture.Get(), &SrvDesc, SRV.GetAddressOf());

		if (FAILED(hr))
			HTR_LOG(Error, "[Texture2DArray] CreateShaderResourceView failed (hr=0x{:08X})", (uint32)hr);

		SRVs.SetNum(ArraySize);
		for (int32 i = 0; i < SRVs.Num(); ++i)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
			SrvDesc.Format = InDesc.Format;
			if (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
				SrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			SrvDesc.Texture2DArray.MostDetailedMip = 0;
			SrvDesc.Texture2DArray.MipLevels = static_cast<UINT>(-1);
			SrvDesc.Texture2DArray.FirstArraySlice = i;
			SrvDesc.Texture2DArray.ArraySize = 1;

			hr = Device->CreateShaderResourceView(Texture.Get(), &SrvDesc, SRVs[i].GetAddressOf());
			if (FAILED(hr))
				HTR_LOG(Error, "[Texture2DArray] CreateShaderResourceView failed (index={}, hr=0x{:08X})", i, (uint32)hr);
		}
	}

	if (InDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		RTVs.SetNum(ArraySize);
		for (int32 i = 0; i < RTVs.Num(); ++i)
		{
			D3D11_RENDER_TARGET_VIEW_DESC RtvDesc{};
			RtvDesc.Format = InDesc.Format;
			RtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			RtvDesc.Texture2DArray.MipSlice = 0;
			RtvDesc.Texture2DArray.FirstArraySlice = i;
			RtvDesc.Texture2DArray.ArraySize = 1;

			hr = Device->CreateRenderTargetView(Texture.Get(), &RtvDesc, RTVs[i].GetAddressOf());
			if (FAILED(hr))
				HTR_LOG(Error, "[Texture2DArray] CreateRenderTargetView failed (index={}, hr=0x{:08X})", i, (uint32)hr);
		}
	}

	if (InDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		DSVs.SetNum(ArraySize);
		for (int32 i = 0; i < DSVs.Num(); ++i)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
			DsvDesc.Format = InDesc.Format;
			if (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
				DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			DsvDesc.Texture2DArray.MipSlice = 0;
			DsvDesc.Texture2DArray.FirstArraySlice = i;
			DsvDesc.Texture2DArray.ArraySize = 1;

			hr = Device->CreateDepthStencilView(Texture.Get(), &DsvDesc, DSVs[i].GetAddressOf());
			if (FAILED(hr))
				HTR_LOG(Error, "[Texture2DArray] CreateDepthStencilView failed (index={}, hr=0x{:08X})", i, (uint32)hr);
		}
	}
}