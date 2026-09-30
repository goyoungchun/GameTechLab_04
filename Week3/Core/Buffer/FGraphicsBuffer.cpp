#include "PCH.h"
#include "FGraphicsBuffer.h"

#include <cstring>

bool FGraphicsBuffer::Initialize(ID3D11Device* Device, const FGraphicsBufferDescription& InDescription, const void* InitialData) {
	if (!Device || InDescription.ByteSize == 0) {
		return false;
	}

	D3D11_BUFFER_DESC BufferDesc{};
	BufferDesc.ByteWidth = InDescription.ByteSize;
	BufferDesc.Usage = InDescription.Usage;
	BufferDesc.BindFlags = InDescription.BindFlags;
	BufferDesc.CPUAccessFlags = InDescription.CPUAccessFlags;
	BufferDesc.MiscFlags = InDescription.MiscFlags;
	BufferDesc.StructureByteStride = InDescription.Stride;

	D3D11_SUBRESOURCE_DATA SubresourceData{};
	SubresourceData.pSysMem = InitialData;

	Microsoft::WRL::ComPtr<ID3D11Buffer> NewBuffer;

	const HRESULT Result = Device->CreateBuffer(&BufferDesc, InitialData ? &SubresourceData : nullptr, NewBuffer.GetAddressOf());
	if (FAILED(Result)) {
		return false;
	}

	Buffer = std::move(NewBuffer);
	Description = InDescription;

	return true;
}

bool FGraphicsBuffer::Update(ID3D11DeviceContext* Context, const void* Data, uint32 InByteSize, uint32 DestinationOffset) {
	if (!Context || !Buffer || !Data || InByteSize == 0) {
		return false;
	}

	if (Description.Usage != D3D11_USAGE_DEFAULT) {
		return false;
	}

	if (DestinationOffset > Description.ByteSize || InByteSize > Description.ByteSize - DestinationOffset) {
		return false;
	}

	if (DestinationOffset == 0 && InByteSize == Description.ByteSize) {
		Context->UpdateSubresource(Buffer.Get(), 0, nullptr, Data, 0, 0);
		return true;
	}

	D3D11_BOX Box{};
	Box.left = DestinationOffset;
	Box.right = DestinationOffset + InByteSize;
	Box.top = 0;
	Box.bottom = 1;
	Box.front = 0;
	Box.back = 1;

	Context->UpdateSubresource(Buffer.Get(), 0, &Box, Data, 0, 0);

	return true;
}

bool FGraphicsBuffer::WriteDiscard(ID3D11DeviceContext* Context, const void* Data, uint32 InByteSize) {
	if (!Context || !Buffer || !Data || InByteSize == 0) {
		return false;
	}

	if (Description.Usage != D3D11_USAGE_DYNAMIC || !(Description.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)) {
		return false;
	}

	if (InByteSize > Description.ByteSize) {
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE MappedResource{};

	const HRESULT Result = Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(Result)) {
		return false;
	}

	std::memcpy(MappedResource.pData, Data, InByteSize);

	Context->Unmap(Buffer.Get(), 0);

	return true;
}

bool FGraphicsBuffer::WriteNoOverwrite(ID3D11DeviceContext* Context, const void* Data, uint32 InByteSize, uint32 DestinationOffset) {
	if (!Context || !Buffer || !Data || InByteSize == 0) {
		return false;
	}

	if (Description.Usage != D3D11_USAGE_DYNAMIC || !(Description.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)) {
		return false;
	}

	if (DestinationOffset > Description.ByteSize || InByteSize > Description.ByteSize - DestinationOffset) {
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE MappedResource{};

	const HRESULT Result = Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &MappedResource);
	if (FAILED(Result)) {
		return false;
	}

	std::memcpy(static_cast<uint8*>(MappedResource.pData) + DestinationOffset, Data, InByteSize);

	Context->Unmap(Buffer.Get(), 0);

	return true;
}

bool FGraphicsBuffer::CopyFrom(ID3D11DeviceContext* Context, const FGraphicsBuffer& Source) {
	if (!Context || !Buffer || !Source.Buffer) {
		return false;
	}

	if (Description.ByteSize != Source.Description.ByteSize) {
		return false;
	}

	Context->CopyResource(Buffer.Get(), Source.Buffer.Get());

	return true;
}

bool FGraphicsBuffer::CopyFrom(ID3D11DeviceContext* Context, uint32 DestinationOffset, const FGraphicsBuffer& Source, uint32 SourceOffset, uint32 InByteSize) {
	if (!Context || !Buffer || !Source.Buffer || InByteSize == 0) {
		return false;
	}

	if (DestinationOffset > Description.ByteSize || InByteSize > Description.ByteSize - DestinationOffset) {
		return false;
	}

	if (SourceOffset > Source.Description.ByteSize || InByteSize > Source.Description.ByteSize - SourceOffset) {
		return false;
	}

	D3D11_BOX SourceBox{};
	SourceBox.left = SourceOffset;
	SourceBox.right = SourceOffset + InByteSize;
	SourceBox.top = 0;
	SourceBox.bottom = 1;
	SourceBox.front = 0;
	SourceBox.back = 1;

	Context->CopySubresourceRegion(Buffer.Get(), 0, DestinationOffset, 0, 0, Source.Buffer.Get(), 0, &SourceBox);

	return true;
}

void FGraphicsBuffer::Reset() {
	Buffer.Reset();
	Description = {};
}