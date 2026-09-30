#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Defines.h"

class FShader {
public:
    FShader() = default;
    ~FShader() = default;

    FShader(const FShader&) = delete;
    FShader& operator=(const FShader&) = delete;

    FShader(FShader&&) noexcept = default;
    FShader& operator=(FShader&&) noexcept = default;

public:
    bool Initialize(ID3D11Device* Device, const FShaderDescription& Description);
    void Reset();

public:
    EShaderStage GetStage() const noexcept { return Stage; }

    const void* GetByteCodeData() const noexcept { return ByteCode.data(); }
    std::size_t GetByteCodeSize() const noexcept { return ByteCode.size(); }

    ID3D11VertexShader* GetVertexShader() const noexcept { return VertexShader.Get(); }
    ID3D11PixelShader* GetPixelShader() const noexcept { return PixelShader.Get(); }
    ID3D11GeometryShader* GetGeometryShader() const noexcept { return GeometryShader.Get(); }
    ID3D11HullShader* GetHullShader() const noexcept { return HullShader.Get(); }
    ID3D11DomainShader* GetDomainShader() const noexcept { return DomainShader.Get(); }
    ID3D11ComputeShader* GetComputeShader() const noexcept { return ComputeShader.Get(); }

private:
    EShaderStage Stage = EShaderStage::Vertex;
    std::vector<std::uint8_t> ByteCode;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> GeometryShader;
    Microsoft::WRL::ComPtr<ID3D11HullShader> HullShader;
    Microsoft::WRL::ComPtr<ID3D11DomainShader> DomainShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShader;
};