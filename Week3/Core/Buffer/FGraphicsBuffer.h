#pragma once

#include <d3d11.h>
#include <wrl/client.h>

struct FGraphicsBufferDescription {
	uint32 ByteSize = 0;
	uint32 Stride = 0;

	D3D11_USAGE Usage = D3D11_USAGE_DEFAULT;

	uint32 BindFlags = 0;
	uint32 CPUAccessFlags = 0;
	uint32 MiscFlags = 0;
};

class FGraphicsBuffer {
public:
	FGraphicsBuffer() = default;
	~FGraphicsBuffer() = default;

	FGraphicsBuffer(const FGraphicsBuffer&) = delete;
	FGraphicsBuffer& operator=(const FGraphicsBuffer&) = delete;

	FGraphicsBuffer(FGraphicsBuffer&&) noexcept = default;
	FGraphicsBuffer& operator=(FGraphicsBuffer&&) noexcept = default;

public:
	bool Initialize(ID3D11Device* Device, const FGraphicsBufferDescription& Description, const void* InitialData = nullptr);

	bool Update(ID3D11DeviceContext* Context, const void* Data, uint32 InByteSize, uint32 DestinationOffset = 0);

	bool WriteDiscard(ID3D11DeviceContext* Context, const void* Data, uint32 InByteSize);
	bool WriteNoOverwrite(ID3D11DeviceContext* Context, const void* Data, uint32 InByteSize, uint32 DestinationOffset);

	bool CopyFrom(ID3D11DeviceContext* Context, const FGraphicsBuffer& Source);
	bool CopyFrom(ID3D11DeviceContext* Context, uint32 DestinationOffset, const FGraphicsBuffer& Source, uint32 SourceOffset, uint32 InByteSize);

	void Reset();

public:
	[[nodiscard]] ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }

	[[nodiscard]] uint32 GetByteSize() const { return Description.ByteSize; }
	[[nodiscard]] uint32 GetStride() const { return Description.Stride; }
	[[nodiscard]] uint32 GetBindFlags() const { return Description.BindFlags; }
	[[nodiscard]] D3D11_USAGE GetUsage() const { return Description.Usage; }

	[[nodiscard]] const FGraphicsBufferDescription& GetDescription() const { return Description; }

	[[nodiscard]] bool IsValid() const { return Buffer != nullptr; }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	FGraphicsBufferDescription Description{};
};