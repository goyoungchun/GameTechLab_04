#pragma once

#include "Shader.h"
#include "RenderUtil.h"

class FRenderResourceManager
{
public:
	static FRenderResourceManager& Get()
	{
		static FRenderResourceManager* Instance = new FRenderResourceManager();
		return *Instance;
	}

	static void Init()
	{
		Get().ScanShaders("Resources/Shader");
	}

	void ScanShaders(const fs::path& ShaderRoot);

	static void Shutdown();

	static FShaderProgram* GetShaderProgram(const FString& InPath);
	static FComputeShader* GetComputeShader(const FString& InPath, const TArray<FShaderDefine>& Defines = {});
	// 매크로 변형을 찾는다. ScanShaders는 기본 변형만 만들므로, 처음 요청받을 때 컴파일해 둔다.
	// 컴파일에 실패하면 DefaultShader로 대체하지 않고 nullptr을 돌려준다.
	static FShaderProgram* GetShaderProgram(const FString& InPath, const TArray<FShaderDefine>& Defines);
private:
	void LoadOrCompileShader(const FString& Path, const TArray<FShaderDefine>& Defines = {});
	// ShaderProgramMap 키. 매크로가 없으면 경로 그대로라서 기존 GetShaderProgram(Path)와 같은 키가 된다.
	static FString MakeProgramKey(const FString& Path, const TArray<FShaderDefine>& Defines);

	TMap<FString, TUniquePtr<FVertexShader>> VertexShaderMap;
	TMap<FString, TUniquePtr<FPixelShader>>  PixelShaderMap;
	TMap<FString, TUniquePtr<FComputeShader>>  ComputeShaderMap;
	TMap<FString, TUniquePtr<FShaderProgram>> ShaderProgramMap;
}; 