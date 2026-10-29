#pragma once

#include "Render/Buffer.h"
#include "Render/PipelineState.h"

struct FTextVertex;
class UFont;
class UCameraComponent;

struct TextTransformData
{
	// 텍스트 Draw에 필요한 월드 행렬과 ViewProjection 행렬을 담는다.
	FMatrix World;
	FMatrix ViewProj;
};


struct MSDFData
{
	// MSDF 텍스트의 화면 픽셀 크기와 정렬용 여유 값, Color값을 담는다.
	float ScreenPx;
	FVector Pad = FVector();
	FVector4 Color = FVector4(1, 1, 1, 1);
};

enum class ETextAlign { Left, Center, Right };

class FTextRenderer
{
public:
	void Init(uint32 InMaxCharacters = 256);

	// 컴포넌트 트랜스폼을 그대로 사용 (TextRenderComponent)
	void OnRender(const FString& Text, const FMatrix& WorldMatrix, float TextSize,
		const UFont& Atlas, UCameraComponent* CameraComponent);
	// View별 ViewProjection을 직접 사용해 텍스트를 해당 View에 렌더한다.
	void OnRender(const FString& Text, const FMatrix& WorldMatrix, float TextSize,
		const UFont& Atlas, const FMatrix& ViewProjection);

	void OnRenderScreen(const FString& Text,float PixelX, float PixelY, float PixelSize, const UFont& Atlas, 
		uint32 ScreenW, uint32 ScreenH, FVector4 Color, ETextAlign Align);

	// 항상 카메라를 향하게 그린다 (에디터 UUID 라벨 등)
	void OnRenderBillboard(const FString& Text, const FVector& WorldPos, float TextSize,
		const UFont& Atlas, UCameraComponent* CameraComponent);

	void BuildTextMesh(const FString& Text, float TextSize, const UFont& Atlas);

	static bool ComputeTextBounds(const FString& Text, float TextSize, const UFont& Atlas,
		float& OutMinY, float& OutMaxY, float& OutMinZ, float& OutMaxZ);

	static float MeasureTextWidth(const FString& Text, float TextSize, const UFont& Atlas);
private:
	void DrawTextRender(const FString& Text, const FMatrix& WorldMatrix, float TextSize,
		const UFont& Atlas, const FMatrix& ViewProjection, const FVector4 Color, const FPipelineState& State);

	ComPtr<ID3D11RasterizerState> RasterizerState;

	FShaderProgram* TextShader = nullptr;
	FPipelineState PipelineState;
	FPipelineState ScreenPipelineState;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;

	TArray<FTextVertex> Vertices;
	TArray<uint32> Indices;

	TUniquePtr<FConstantBuffer> MVP;
	TUniquePtr<FConstantBuffer> ScreenPx;
	uint32 MaxCharacters = 64; // 대략 UUID 문자열 길이(32~36자) 여유있게
	uint32 MaxVertices = 0;
	uint32 MaxIndices = 0;
};
