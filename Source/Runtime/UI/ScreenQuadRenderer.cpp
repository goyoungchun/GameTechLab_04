#include "EnginePCH.h"
#include "ScreenQuadRenderer.h"

#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Render/Texture/Texture2D.h"

void FScreenQuadRenderer::Init(uint32 InMaxQuads)
{
	MaxVertices = InMaxQuads * 6; // 인덱스 없이 사각형당 삼각형 2개(6정점)

	QuadShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ScreenQuad.hlsl");

	PipelineState.Shader = QuadShader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
	PipelineState.BlendState = EBlendState::AlphaBlend;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;

	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));
	ProjectionCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	Vertices.Reserve(MaxVertices);
}

void FScreenQuadRenderer::BeginFrame(uint32 ScreenW, uint32 ScreenH)
{
	Vertices.Reset();

	// OnRenderScreen과 같은 행렬
	ScreenProjection = FMatrix::MakeOrthographic(0.0f, static_cast<float>(ScreenW),
		static_cast<float>(ScreenH), 0.0f, -1.0f, 1.0f);
}

void FScreenQuadRenderer::AddRect(float X, float Y, float W, float H, const FVector4& Color)
{
	AddQuad(X, Y, W, H, FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), Color);
}

void FScreenQuadRenderer::AddQuad(float X, float Y, float W, float H, const FVector2& UVMin, const FVector2& UVMax, const FVector4& Color)
{
	if (Vertices.Num() + 6 > static_cast<int32>(MaxVertices))
		Flush();

	const FVertex TopLeft     = { FVector(0.0f, X, Y),     FVector2(UVMin.X, UVMin.Y), Color };
	const FVertex TopRight    = { FVector(0.0f, X + W, Y),     FVector2(UVMax.X, UVMin.Y), Color };
	const FVertex BottomRight = { FVector(0.0f, X + W, Y + H), FVector2(UVMax.X, UVMax.Y), Color };
	const FVertex BottomLeft  = { FVector(0.0f, X, Y + H), FVector2(UVMin.X, UVMax.Y), Color };

	Vertices.Add(TopLeft);
	Vertices.Add(TopRight);
	Vertices.Add(BottomRight);

	Vertices.Add(TopLeft);
	Vertices.Add(BottomRight);
	Vertices.Add(BottomLeft);
}

void FScreenQuadRenderer::Flush()
{
	if (Vertices.IsEmpty())
		return;

	if (!WhiteTexture)
	{
		WhiteTexture = UAssetManager::GetAssetByPath<UTexture2D>("WhiteTexture");
		if (!WhiteTexture)
			return;
	}

	RenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), sizeof(FVertex) * Vertices.Num());
	RenderCommand::UpdateBufferData(ProjectionCB.get(), &ScreenProjection, sizeof(FMatrix));

	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindShaderResource(0, WhiteTexture, EShaderBindFlagBits::Pixel);
	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
	RenderCommand::BindConstantBuffer(0, ProjectionCB.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindVertexBuffer(VertexBuffer.get());

	RenderCommand::Draw(static_cast<uint32>(Vertices.Num()), 0);

	Vertices.Reset();
}
