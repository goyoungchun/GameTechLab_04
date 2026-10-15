#include "EnginePCH.h"
#include "Component/PointLightComponent.h"

#include "Render/LineBatcher.h"
#include "Engine/PointLightSceneProxy.h"

namespace
{
	// 한 원의 분할 수
	constexpr int32 CIRCLE_SEGMENT_COUNT = 24;

	// Apex에서 축평면 방향으로 원 그리기 
	void AddWireCircle(
		FLineBatcher* LineBatcher,
		const FVector& Apex,
		const FVector& AxisA,
		const FVector& AxisB,
		float Range,
		const FVector4& Color)
	{
		// 밑면 원
		FVector PreviousPoint;
		FVector FirstPoint;

		for (int32 i = 0; i < CIRCLE_SEGMENT_COUNT; ++i)
		{
			const float Angle = (2.0f * PI * i) / CIRCLE_SEGMENT_COUNT;

			const FVector Point =
				Apex +
				AxisA * (std::cosf(Angle) * Range) +
				AxisB * (std::sinf(Angle) * Range);

			if (i == 0)
			{
				FirstPoint = Point;
			}
			else
			{
				LineBatcher->AddLine(PreviousPoint, Point, Color);
			}

			PreviousPoint = Point;
		}
		// 마지막 점과 첫 점을 이어 원을 닫는다
		LineBatcher->AddLine(PreviousPoint, FirstPoint, Color);
	}
}

void UPointLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (LineBatcher == nullptr)
	{
		return;
	}

	FTransform WorldTransform;
	WorldTransform.Location = GetWorldLocation();
	WorldTransform.Rotation = GetWorldRotation();

	FVector Apex = WorldTransform.Location;
	FVector Forward = WorldTransform.GetForward();

	TArray<FVector> Axis = { XAxisVector, YAxisVector, ZAxisVector };
	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		FVector u = Axis[(AxisIndex + 1) % 3];
		FVector v = Axis[(AxisIndex + 2) % 3];

		// 에디터에서 값을 직접 드래그하므로 범위를 신뢰할 수 없다.
		// tan(90도)는 무한대라 원뿔 정점이 NaN이 되므로 여기서 막는다.
		const float SafeRadius = (AttenuationRadius > 0.0f) ? AttenuationRadius : 0.0f;

		// 바깥 원뿔은 라이트 색 그대로, 안쪽은 구분되도록 흐리게
		AddWireCircle(LineBatcher, Apex, u, v, SafeRadius, LightColor);

		FVector4 InnerColor(LightColor.X * 0.5f, LightColor.Y * 0.5f, LightColor.Z * 0.5f, LightColor.W);
	}
}

FLightSceneProxy* UPointLightComponent::CreateSceneProxy() const
{


	return new FPointLightSceneProxy(this);
}
