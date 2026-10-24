#include "EnginePCH.h"
#include "LineBatcher.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"

#include "RenderCommand.h"
#include "RenderResourceManager.h"

#include "UObject/UObjectIterator.h"

#include <cmath>

FLineBatcher::~FLineBatcher()
{
	delete[] VertexBufferBase;
}

bool FLineBatcher::Init(FRenderer* InRenderer, UWorld* InWorld)
{
	Renderer = InRenderer;
	World = InWorld;

	LineShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/BoundingBoxShader.hlsl");

	PipelineState.Shader = LineShader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

	MaxVertices = 65536;
	VertexBufferBase = new FVertex[MaxVertices];

	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));

	CB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	return true;
}

void FLineBatcher::BuildVertexBuffer()
{
	for (TObjectIterator<UPrimitiveComponent> Itr; Itr; ++Itr)
	{
		if (!Itr->IsRegistered() || Itr->GetWorld() != World || !Itr->IsVisible())
			continue;
		// 로컬 AABB와 월드 행렬을 그대로 넘기고 변환은 AddOrientedBox가 한다.
		// (월드 행렬이어야 부모에 붙은 컴포넌트도 제자리에 그려진다)
		AddOrientedBox(
			Itr->CalcLocalBounds(),
			Itr->GetWorldMatrix(),
			FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void FLineBatcher::BuildShapeVertexBuffer()
{
	for (TObjectIterator<UShapeComponent> Itr; Itr; ++Itr)
	{
		if (!Itr->IsRegistered() || Itr->GetWorld() != World || !Itr->IsVisible() || !Itr->GetDrawShape())
			continue;
		Itr->DrawShape(*this);
	}
}

void FLineBatcher::BeginFrame()
{
	VertexBufferPtr = VertexBufferBase;
	VertexCount = 0;
}

void FLineBatcher::AddLine(const FVector& A, const FVector& B, const FVector4& Color)
{
	if (VertexCount + 2 > MaxVertices)
		return;   // 넘치면 버림

	*VertexBufferPtr++ = { A, FVector2(), Color };
	*VertexBufferPtr++ = { B, FVector2(), Color };

	VertexCount += 2;
}

namespace
{
	// 박스 모서리 12개. 코너 배열의 인덱스 쌍이며 AABB/OBB가 같은 순서를 쓴다.
	const int BOX_EDGES[12][2] = {
		{0,1},{1,2},{2,3},{3,0},      // 아랫면
		{4,5},{5,6},{6,7},{7,4},      // 윗면
		{0,4},{1,5},{2,6},{3,7},      // 기둥
	};

	// Min/Max로 코너 8개를 만든다. BOX_EDGES가 기대하는 순서다.
	void MakeCorners(const FVector& Min, const FVector& Max, FVector* OutCorners)
	{
		OutCorners[0] = { Min.X, Min.Y, Min.Z };
		OutCorners[1] = { Max.X, Min.Y, Min.Z };
		OutCorners[2] = { Max.X, Max.Y, Min.Z };
		OutCorners[3] = { Min.X, Max.Y, Min.Z };
		OutCorners[4] = { Min.X, Min.Y, Max.Z };
		OutCorners[5] = { Max.X, Min.Y, Max.Z };
		OutCorners[6] = { Max.X, Max.Y, Max.Z };
		OutCorners[7] = { Min.X, Max.Y, Max.Z };
	}
}

void FLineBatcher::AddBox(FBox BoundingBox, FVector4 Color)
{
	FVector C[8];
	MakeCorners(BoundingBox.Min, BoundingBox.Max, C);

	for (const auto& Edge : BOX_EDGES)
	{
		AddLine(C[Edge[0]], C[Edge[1]], Color);
	}
}

void FLineBatcher::AddOrientedBox(const FBox& LocalBox, const FMatrix& WorldMatrix, const FVector4& Color)
{
	FVector C[8];
	MakeCorners(LocalBox.Min, LocalBox.Max, C);

	// 변환 후에 Min/Max를 다시 재면 회전이 뭉개지므로,
	// 코너를 하나씩 옮긴 뒤 원래 순서 그대로 잇는다.
	for (FVector& Corner : C)
	{
		const FVector4 WorldCorner = FVector4(Corner, 1.0f) * WorldMatrix;
		Corner = FVector(WorldCorner.X, WorldCorner.Y, WorldCorner.Z);
	}

	for (const auto& Edge : BOX_EDGES)
	{
		AddLine(C[Edge[0]], C[Edge[1]], Color);
	}

}
void FLineBatcher::AddSphere(float Radius, const FMatrix& WorldMatrix, const FVector4& Color)
{
	if (!(Radius > 0.0f))
		return;

	constexpr int32 SegmentCount = 32;
	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const float AngleA = 2.0f * PI * Segment / SegmentCount;
		const float AngleB = 2.0f * PI * (Segment + 1) / SegmentCount;
		const float CosA = std::cos(AngleA) * Radius;
		const float SinA = std::sin(AngleA) * Radius;
		const float CosB = std::cos(AngleB) * Radius;
		const float SinB = std::sin(AngleB) * Radius;
		AddLine(WorldMatrix.TransformPosition(FVector(CosA, SinA, 0.0f)),
			WorldMatrix.TransformPosition(FVector(CosB, SinB, 0.0f)), Color);
		AddLine(WorldMatrix.TransformPosition(FVector(CosA, 0.0f, SinA)),
			WorldMatrix.TransformPosition(FVector(CosB, 0.0f, SinB)), Color);
		AddLine(WorldMatrix.TransformPosition(FVector(0.0f, CosA, SinA)),
			WorldMatrix.TransformPosition(FVector(0.0f, CosB, SinB)), Color);
	}
}

void FLineBatcher::AddCapsule(float Radius, float HalfHeight, const FMatrix& WorldMatrix, const FVector4& Color)
{
	if (!(Radius > 0.0f))
		return;

	const float CylinderHalfHeight = std::max(0.0f, HalfHeight - Radius);
	const FVector TopCenter(0.0f, 0.0f, CylinderHalfHeight);
	const FVector BottomCenter(0.0f, 0.0f, -CylinderHalfHeight);
	const FVector Up(0.0f, 0.0f, 1.0f);
	constexpr int32 SegmentCount = 32;
	constexpr int32 ArcSegmentCount = 8;

	// 원통 양 끝의 원.
	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const float AngleA = 2.0f * PI * Segment / SegmentCount;
		const float AngleB = 2.0f * PI * (Segment + 1) / SegmentCount;
		const FVector A(std::cos(AngleA) * Radius, std::sin(AngleA) * Radius, 0.0f);
		const FVector B(std::cos(AngleB) * Radius, std::sin(AngleB) * Radius, 0.0f);
		AddLine(WorldMatrix.TransformPosition(TopCenter + A), WorldMatrix.TransformPosition(TopCenter + B), Color);
		AddLine(WorldMatrix.TransformPosition(BottomCenter + A), WorldMatrix.TransformPosition(BottomCenter + B), Color);
	}

	// 네 방향의 옆선과 양 끝 반구의 호.
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const float SideAngle = 0.5f * PI * Side;
		const FVector Radial(std::cos(SideAngle), std::sin(SideAngle), 0.0f);
		AddLine(WorldMatrix.TransformPosition(TopCenter + Radial * Radius),
			WorldMatrix.TransformPosition(BottomCenter + Radial * Radius), Color);
		for (int32 Segment = 0; Segment < ArcSegmentCount; ++Segment)
		{
			const float AngleA = 0.5f * PI * Segment / ArcSegmentCount;
			const float AngleB = 0.5f * PI * (Segment + 1) / ArcSegmentCount;
			const FVector RadialA = Radial * (std::cos(AngleA) * Radius);
			const FVector RadialB = Radial * (std::cos(AngleB) * Radius);
			const FVector UpA = Up * (std::sin(AngleA) * Radius);
			const FVector UpB = Up * (std::sin(AngleB) * Radius);
			AddLine(WorldMatrix.TransformPosition(TopCenter + RadialA + UpA),
				WorldMatrix.TransformPosition(TopCenter + RadialB + UpB), Color);
			AddLine(WorldMatrix.TransformPosition(BottomCenter + RadialA - UpA),
				WorldMatrix.TransformPosition(BottomCenter + RadialB - UpB), Color);
		}
	}
}

void FLineBatcher::AddPath(const TArray<FVector>& Points, const FVector4& Color)
{
	if (Points.IsEmpty())
	{
		return;
	}
	for (size_t i = 0; i < Points.size() - 1; ++i) {
		AddLine(Points[i], Points[i + 1], Color);
	}
}

void FLineBatcher::Flush()
{
	RenderCommand::BindPipelineState(PipelineState);
	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);

	RenderCommand::Draw(VertexCount, 0);
}

void FLineBatcher::OnRender(const FMatrix& ViewProjection)
{
	//BuildVertexBuffer();

	FMatrix VP = ViewProjection;
	RenderCommand::UpdateBufferData(VertexBuffer.get(), VertexBufferBase, sizeof(FVertex) * VertexCount);
	RenderCommand::UpdateBufferData(CB.get(), &VP, sizeof(FMatrix));

	Flush();
}

