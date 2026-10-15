#pragma once

#include "RenderEnums.h"

struct FShaderByteCode
{
	ComPtr<ID3DBlob> Blob;

	const void* GetData() const { return Blob ? Blob->GetBufferPointer() : nullptr; }
	SIZE_T      GetSize() const { return Blob ? Blob->GetBufferSize() : 0; }
	bool        IsValid() const { return Blob != nullptr; }
};

// 셰이더 변형을 만드는 매크로 하나 (#define Name Value)
struct FShaderDefine
{
	FString Name;
	FString Value = "1";
};

class RenderUtil
{
public:
	static FShaderByteCode CompileShader(FString Path, const char* EntryPoint, EShaderType ShaderType, const TArray<FShaderDefine>& Defines = {});
	static void SaveByteCode(FString TargetPath, const FShaderByteCode& ByteCode);
	static FShaderByteCode LoadByteCode(const FString& CsoPath);

	// 변형마다 .cso가 겹치지 않도록 매크로 조합을 파일 이름에 붙인다 (매크로가 없으면 기존 이름 그대로)
	static FString GetCsoPath(const FString& ShaderPath, EShaderType ShaderType, const TArray<FShaderDefine>& Defines = {});
	static bool IsCsoUpToDate(const FString& ShaderPath, const FString& CsoPath);
	static FShaderByteCode GetOrCompile(const FString& ShaderPath, const char* EntryPoint, EShaderType ShaderType, FString& OutCSOPath, const TArray<FShaderDefine>& Defines = {});

	// 셰이더 맵 키에 붙일 문자열. 예: "LIGHTING_MODEL_PHONG=1". 매크로가 없으면 빈 문자열.
	static FString MakeDefinesKey(const TArray<FShaderDefine>& Defines);
};