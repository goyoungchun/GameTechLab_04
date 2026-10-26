#pragma once

#include "Render/Buffer.h"
#include "Render/PipelineState.h"
#include "Render/Vertex.h"
#include "Math/Matrix.h"

class UTexture2D;

class FScreenQuadRenderer
{
public:
	FScreenQuadRenderer() = default;
	~FScreenQuadRenderer() = default;

	void Init(uint32 InMaxQuads = 1024);
	void BeginFrame(uint32 ScreenW, uint32 ScreenH);
	void AddRect(float X, float Y, float W, float H, const FVector4& Color);
	void Flush();

private:
	void AddQuad(float X, float Y, float W, float H, const FVector2& UVMin, const FVector2& UVMax, const FVector4& Color);

	FShaderProgram* QuadShader = nullptr;
	FPipelineState PipelineState;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FConstantBuffer> ProjectionCB;

	TArray<FVertex> Vertices;
	uint32 MaxVertices = 0;
	FMatrix ScreenProjection;
	UTexture2D* WhiteTexture = nullptr;
};
