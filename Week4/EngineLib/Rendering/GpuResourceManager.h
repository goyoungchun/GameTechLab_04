// EngineLib/Rendering/GpuResourceManager.h

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "Core/enum.h"
#include "Core/Name.h"
#include "Core/AssetManager.h"
#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/VertexType.h"

//namespace BuiltInGpuResource
//{
//	inline const FName CubeBufferName{ "CubeBuffer" };
//	inline const FName SphereBufferName{ "SphereBuffer" };
//	inline const FName GizmoArrowBufferName{ "GizmoArrowBuffer" };
//	inline const FName CircleBufferName{ "CircleBuffer" };
//	inline const FName ParticleBufferName{ "ParticleBuffer" };
//}

inline constexpr uint32 InitialMaxFontCount = 16;
inline constexpr uint32 InitialMaxUnicodeFontCount = 16;
inline constexpr uint32 InitialMaxLineVertexCapacity = 8192;
inline constexpr uint32 InitialMaxLineIndexCapacity = 16384;
inline constexpr uint32 InitialMaxInstanceCount = 16;

struct FBuffer
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D11Buffer> Buffer;
	uint32 SourceNum = 0;
	FBoundingBox LocalBounds;

	ComPtr<ID3D11Buffer> TexturedBuffer = nullptr;
	ComPtr<ID3D11Buffer> IndexBuffer = nullptr;
	uint32 IndexCount = 0;
};

class FGpuResourceManager
{
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	FGpuResourceManager();
	void Initialize(FAssetManager& assetManagerRef, ID3D11Device& deviceRef);
	void SetDistanceRange(float distanceRange);

	/* Getters */
	const FBuffer* FindImmutableBufferOrAdd(FName bufferName);
	ID3D11ShaderResourceView* FindTextureOrAdd(FName texturePath);

	ID3D11Buffer& GetConstantBuffer(EContantBufferType bufferType) const;
	ID3D11BlendState& GetBlendState(EBlendStateType stateType) const;
	ID3D11InputLayout& GetInputLayout(EInputLayoutType layoutType) const;
	ID3D11SamplerState& GetSamplerState(ESamplerStateType stateType) const;
	ID3D11RasterizerState& GetRasterizerState(ERasterizerStateType stateType) const;
	ID3D11DepthStencilState& GetDepthStencilState(EDepthStencilStateType stateType) const;

	const FBuffer& GetFontBuffer() const;
	const FBuffer& GetUnicodeFontBuffer() const;
	const FBuffer& GetLineBuffer() const;
	const FBuffer& GetInstanceBuffer() const;

	ID3D11VertexShader& GetVertexShader(EVertexShaderType shaderType) const;
	ID3D11PixelShader& GetPixelShader(EPixelShaderType shaderType) const;

	/* Setters */
	void CreateBuffer(FName bufferName, const TArray<FVertexSimple>& vertices, const TArray<uint32>& indices = {});
	void CreateBuffer(FName bufferName, const TArray<FVertexTextured>& vertices, const TArray<uint32>& indices = {});
	void CreateBuffer(FName bufferName, const TArray<FNormalVertex>& vertices, const TArray<uint32>& indices = {});

	void CreateTextureFromDDS(FName texturePath);
	void CreateUnicodeFontTexture(FName texturePath);
	void CreateTextureFromWIC(FName texturePath);

	/* Ensure Dynamic Buffers Capacity */
	bool EnsureFontBuffer(uint32 fontCount);
	bool EnsureUnicodeFontBuffer(uint32 quadCount);
	bool EnsureLineBuffer(uint32 vertexCount, uint32 indexCount);
	bool EnsureInstanceCapacity(uint32 count);

	uint32 GetImmutableBufferCount() const { return mImmutableBufferMap.Num(); }
	uint32 GetTextureCount() const { return mTextureMap.Num(); }

private:
	/* References */
	ID3D11Device* mDeviceRef = nullptr;
	FAssetManager* mAssetManagerRef;

	/* Render States */
	ComPtr<ID3D11BlendState> mBlendState[BST_Count];
	ComPtr<ID3D11SamplerState> mSamplerState[SST_Count];
	ComPtr<ID3D11RasterizerState> mRasterizerState[RST_Count];
	ComPtr<ID3D11DepthStencilState> mDepthStencilState[DSS_Count];

	/* Buffers */
	// Immutable buffers
	TMap<FName, FBuffer> mImmutableBufferMap;

	// Mutable buffers
	ComPtr<ID3D11Buffer> mConstantBuffer[CBT_Count];
	FBuffer mFontBuffer;
	uint32  mMaxFontCount = InitialMaxFontCount;
	FBuffer mUnicodeFontBuffer;
	uint32  mMaxUnicodeFontCount = InitialMaxUnicodeFontCount;

	FBuffer mLineBuffer;
	uint32  mMaxLineVertexCapacity = InitialMaxLineVertexCapacity;
	uint32  mMaxLineIndexCapacity = InitialMaxLineIndexCapacity;

	FBuffer mInstanceBuffer;
	uint32  mMaxInstanceCount = InitialMaxInstanceCount;

	/* Shaders */
	ComPtr<ID3D11VertexShader> mVertexShader[VST_Count];
	ComPtr<ID3D11PixelShader> mPixelShader[PST_Count];
	ComPtr<ID3D11InputLayout> mInputLayout[ILT_Count];

	/* Textures */
	TMap<FName, ComPtr<ID3D11ShaderResourceView>> mTextureMap;

	/* Create */

	// Create general resources
	void create(float distanceRange);

	/* Create */

	void createBuiltinBuffers();
	void createBuiltinTextures();

	void createDefaultWhiteTexture();

	void createSamplerStates();
	void createRasterizerStates();
	void createDepthStencilStates();
	void createBlendStates();

	void createShadersAndInputLayout();

	// Mutable buffers. Should be called when the capacity exceeds
	void createLineVertexBuffer(uint32 maxVertices);
	void createLineIndexBuffer(uint32 maxIndices);
	void createFontBuffer(uint32 numQuad);
	void createUnicodeFontBuffer(uint32 numQuad);
	void createInstanceBuffer(uint32 maxInstances);

	void createConstantBuffers(float distanceRange);

	// Helper function to compile shader from file
	ComPtr<ID3DBlob> createVertexShaderFromFile(
		const wchar_t* filename, const char* entryPoint, const char* shaderModel,
		EVertexShaderType vertexShaderType);
	void createPixelShaderFromFile(
		const wchar_t* filename, const char* entryPoint, const char* shaderModel,
		EPixelShaderType pixelShaderType);

	/* Dynamic Buffer Resizing */

};
