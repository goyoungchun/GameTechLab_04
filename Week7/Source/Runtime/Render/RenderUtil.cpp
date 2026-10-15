#include "EnginePCH.h"
#include "RenderUtil.h"

#include <algorithm>

FShaderByteCode RenderUtil::CompileShader(FString Path, const char* EntryPoint, EShaderType ShaderType, const TArray<FShaderDefine>& Defines)
{
	const char* Target;
	switch (ShaderType)
	{
		case EShaderType::Vertex:
			Target = "vs_5_0";
			break;
		case EShaderType::Pixel:
			Target = "ps_5_0";
			break;
		case EShaderType::Compute:
			Target = "cs_5_0";
			break;
	}

	uint32 Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	FShaderByteCode ByteCode;

	std::wstring WPath(Path.begin(), Path.end());

	// D3D_SHADER_MACRO는 문자열 포인터만 들고 있으므로 Defines가 컴파일이 끝날 때까지 살아 있어야 한다. 끝은 {nullptr, nullptr}.
	std::vector<D3D_SHADER_MACRO> Macros;
	Macros.reserve(static_cast<size_t>(Defines.Num()) + 1);
	for (const FShaderDefine& Define : Defines)
		Macros.push_back({ Define.Name.c_str(), Define.Value.c_str() });
	Macros.push_back({ nullptr, nullptr });

	ComPtr<ID3DBlob> ErrorBlob;
	HRESULT hr = D3DCompileFromFile(WPath.c_str(), Macros.data(), nullptr, EntryPoint, Target, 0, 0, ByteCode.Blob.GetAddressOf(), ErrorBlob.GetAddressOf());

	if (FAILED(hr))
	{
		if (ErrorBlob)
			OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		else
			OutputDebugStringA(("[Shader] file not found: " + Path + "\n").c_str());

		ByteCode.Blob.Reset();
	}
	return ByteCode;
}

void RenderUtil::SaveByteCode(FString CsoPath, const FShaderByteCode& ByteCode)
{
	fs::create_directories(fs::path(CsoPath).parent_path());
	std::ofstream Out(CsoPath, std::ios::binary);
	if (!Out) { fs::remove(CsoPath); return; }
	Out.write(reinterpret_cast<const char*>(ByteCode.GetData()), ByteCode.GetSize());
}

FShaderByteCode RenderUtil::LoadByteCode(const FString& CsoPath)
{
	FShaderByteCode Result;

	std::ifstream In(CsoPath, std::ios::binary | std::ios::ate);
	if (!In) return Result;

	const std::streamsize Size = In.tellg();
	In.seekg(0);
	if (Size <= 0) return Result;

	ComPtr<ID3DBlob> Blob;
	if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(Size), &Blob))) return Result;

	In.read(reinterpret_cast<char*>(Blob->GetBufferPointer()), Size);
	if (In) Result.Blob = std::move(Blob);
	return Result;
}

FString RenderUtil::GetCsoPath(const FString& ShaderPath, EShaderType ShaderType, const TArray<FShaderDefine>& Defines)
{
	const fs::path Rel = fs::relative(ShaderPath, "Resources/Shader");
	const char* Suffix;
	switch (ShaderType)
	{
	case EShaderType::Vertex:
		Suffix = "_VS.cso";
		break;
	case EShaderType::Pixel:
		Suffix = "_PS.cso";
		break;
	case EShaderType::Compute:
		Suffix = "_CS.cso";
		break;
	}
	// 예: UberLit__LIGHTING_MODEL_PHONG_VS.cso (값이 "1"이 아니면 _값까지 붙인다)
	FString VariantName;
	for (const FShaderDefine& Define : Defines)
	{
		VariantName += "__" + Define.Name;
		if (Define.Value != "1")
			VariantName += "_" + Define.Value;
	}

	fs::path Out = fs::path("Intermediate/Shaders") / Rel.parent_path();
	Out /= Rel.stem().generic_string() + VariantName + Suffix;
	return Out.generic_string();
}

FString RenderUtil::MakeDefinesKey(const TArray<FShaderDefine>& Defines)
{
	FString Key;
	for (const FShaderDefine& Define : Defines)
	{
		if (!Key.empty())
			Key += ";";
		Key += Define.Name + "=" + Define.Value;
	}
	return Key;
}

// .cso 파일이 최신 컴파일인지 비교한다.
bool RenderUtil::IsCsoUpToDate(const FString& ShaderPath, const FString& CsoPath)
{
	std::error_code Ec;  
	if (!fs::exists(CsoPath, Ec)) return false;

	const auto SrcTime = fs::last_write_time(ShaderPath, Ec);
	if (Ec) return false;
	const auto CsoTime = fs::last_write_time(CsoPath, Ec);
	if (Ec) return false;

	return CsoTime > SrcTime;
}

FShaderByteCode RenderUtil::GetOrCompile(const FString& ShaderPath, const char* EntryPoint, EShaderType ShaderType, FString& OutCSOPath, const TArray<FShaderDefine>& Defines)
{
	OutCSOPath = GetCsoPath(ShaderPath, ShaderType, Defines);

	if (IsCsoUpToDate(ShaderPath, OutCSOPath))
	{
		FShaderByteCode Cached = LoadByteCode(OutCSOPath);
		if (Cached.IsValid()) return Cached;
	}

	FShaderByteCode Compiled = CompileShader(ShaderPath, EntryPoint, ShaderType, Defines);
	if (Compiled.IsValid())
		SaveByteCode(OutCSOPath, Compiled);

	return Compiled;

}
