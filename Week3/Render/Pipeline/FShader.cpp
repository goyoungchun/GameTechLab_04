#include "PCH.h"
#include "FShader.h"

#include "../../ErrorHandler.h"

#include <d3dcompiler.h>

#include <cstring>

using Microsoft::WRL::ComPtr;

bool FShader::Initialize(ID3D11Device* Device, const FShaderDescription& Description) {
    if (Device == nullptr) {
        ErrorHandler::Report("Shader::Initialize", "A valid Direct3D device is required to initialize a shader.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    FShader::Reset();

    UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
    CompileFlags |= D3DCOMPILE_DEBUG;
    CompileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> ShaderBlob{};
    ComPtr<ID3DBlob> ErrorBlob{};

    HRESULT Result = D3DCompileFromFile(Description.Source.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, Description.EntryPoint.c_str(), Description.Profile.c_str(), CompileFlags, 0, ShaderBlob.GetAddressOf(), ErrorBlob.GetAddressOf());

    if (FAILED(Result)) {
        if (ErrorBlob) {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
        }

        ErrorHandler::ReportHRESULT(Result, "Shader::Initialize", "Shader compilation failed.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    Stage = Description.Stage;

    ByteCode.resize(ShaderBlob->GetBufferSize());
    std::memcpy(ByteCode.data(), ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize());

    switch (Stage) {
    case EShaderStage::Vertex:
        Result = Device->CreateVertexShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, VertexShader.GetAddressOf());
        break;

    case EShaderStage::Pixel:
        Result = Device->CreatePixelShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, PixelShader.GetAddressOf());
        break;

    case EShaderStage::Geometry:
        Result = Device->CreateGeometryShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, GeometryShader.GetAddressOf());
        break;

    case EShaderStage::Hull:
        Result = Device->CreateHullShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, HullShader.GetAddressOf());
        break;

    case EShaderStage::Domain:
        Result = Device->CreateDomainShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, DomainShader.GetAddressOf());
        break;

    case EShaderStage::Compute:
        Result = Device->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, ComputeShader.GetAddressOf());
        break;

    default:
        ErrorHandler::Report("Shader::Initialize", "The shader description contains an invalid shader stage.", ErrorHandler::EErrorLevel::Error);
        return false;
    }

    if (FAILED(Result)) {
        ErrorHandler::ReportHRESULT(Result, "Shader::Initialize", "Failed to create the Direct3D shader.", ErrorHandler::EErrorLevel::Error);
        FShader::Reset();
        return false;
    }

    return true;
}

void FShader::Reset() {
    ByteCode.clear();

    VertexShader.Reset();
    PixelShader.Reset();
    GeometryShader.Reset();
    HullShader.Reset();
    DomainShader.Reset();
    ComputeShader.Reset();
}
