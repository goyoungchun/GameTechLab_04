#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

#include "RenderUtil.h"



void FRenderResourceManager::ScanShaders(const fs::path& ShaderRoot)
{
	std::error_code ErrorCode;

	if (!fs::exists(ShaderRoot, ErrorCode))
	{
		HTR_LOG(Error, "[Shader] Scan Root Not Found : {}", ShaderRoot.generic_string());
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(ShaderRoot))
	{
		if (!Entry.is_regular_file()) continue;
		if (Entry.path().extension() != ".hlsl") continue;

		const FString Path = Entry.path().generic_string();
		if (Path.find("/Compute/") != FString::npos)
		{
			GetComputeShader(Path);
			continue;
		}

		LoadOrCompileShader(Path);
	}
}

void FRenderResourceManager::Shutdown()
{
	Get().ShaderProgramMap.Empty();
	Get().VertexShaderMap.Empty();
	Get().PixelShaderMap.Empty();
	Get().ComputeShaderMap.Empty();
}

FComputeShader* FRenderResourceManager::GetComputeShader(const FString& InPath, const TArray<FShaderDefine>& Defines)
{
	const FString Key = MakeProgramKey(InPath, Defines);
	auto& Manager = Get();
	if (auto* Found = Manager.ComputeShaderMap.Find(Key))
		return Found->get();

	FString CsoPath;
	FShaderByteCode Code = RenderUtil::GetOrCompile(InPath, "mainCS", EShaderType::Compute, CsoPath, Defines);
	if (!Code.IsValid())
	{
		HTR_LOG(Error, "[Shader] compute compile failed: {}", Key);
		return nullptr;
	}

	TUniquePtr<FComputeShader> Shader = RenderCommand::CreateComputeShader(Code);
	if (!Shader || !Shader->IsValid())
	{
		HTR_LOG(Error, "[Shader] compute create failed: {}", Key);
		return nullptr;
	}

	// 매니저가 소유하고, 호출자는 재사용할 포인터만 보관한다.
	FComputeShader* RawShader = Shader.get();
	Manager.ComputeShaderMap[Key] = std::move(Shader);
	HTR_LOG(Info, "[Shader] compute loaded: {}", Key);
	return RawShader;
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(const FString& InPath)
{
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(InPath))
		return Found->get();

	HTR_LOG(Error, "[Shader] not found: {}", InPath);

	if (TUniquePtr<FShaderProgram>* Fallback =
		Get().ShaderProgramMap.Find("Resources/Shader/DefaultShader.hlsl"))
		return Fallback->get();

	return nullptr;
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(const FString& InPath, const TArray<FShaderDefine>& Defines)
{
	//경로 + | + Define 으로 키를 만듬 
	const FString Key = MakeProgramKey(InPath, Defines);
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(Key))
		return Found->get();

	//위에서 찾아봤는데 없으면 LoadOrCompileShader로 만들기
	Get().LoadOrCompileShader(InPath, Defines);

	//만들었으니까 다시 탐색
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(Key))
		return Found->get();
	return nullptr;
}

FString FRenderResourceManager::MakeProgramKey(const FString& Path, const TArray<FShaderDefine>& Defines)
{
	const FString DefinesKey = RenderUtil::MakeDefinesKey(Defines);
	return DefinesKey.empty() ? Path : Path + "|" + DefinesKey;
}

void FRenderResourceManager::LoadOrCompileShader(const FString& Path, const TArray<FShaderDefine>& Defines)
{
	const FString Key = MakeProgramKey(Path, Defines);

	FString VSCSOPath;
	FString PSCSOPath;
	FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, VSCSOPath, Defines);
	FShaderByteCode PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath, Defines);

	if (!VSCode.IsValid() || !PSCode.IsValid())
	{
		HTR_LOG(Error, "[Shader] compile failed: {}", Key);
		return;
	}

	TUniquePtr<FVertexShader> Vs = RenderCommand::CreateVertexShader(VSCode);
	TUniquePtr<FPixelShader>  Ps = RenderCommand::CreatePixelShader(PSCode);

	if (!Vs || !Vs->IsValid() || !Ps || !Ps->IsValid())
	{
		HTR_LOG(Error, "[Shader] device create failed: {}", Key);
		return;
	}

	FVertexShader* VsRaw = Vs.get();
	FPixelShader* PsRaw = Ps.get();

	// .cso 경로에 변형 이름이 들어 있어 VS/PS 맵 키도 변형끼리 겹치지 않는다
	VertexShaderMap[VSCSOPath] = std::move(Vs);
	PixelShaderMap[PSCSOPath] = std::move(Ps);
	ShaderProgramMap[Key] = MakeUnique<FShaderProgram>(VsRaw, PsRaw);

	HTR_LOG(Info, "[Shader] loaded: {}", Key);

}
