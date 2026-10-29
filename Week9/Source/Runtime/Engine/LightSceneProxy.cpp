#include "EnginePCH.h"
#include "Engine/LightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/SpotLightComponent.h"

#include <algorithm>
#include <cmath>

#include "Component/AmbientLightComponent.h"

static uint32 GNextLightId = 1;

void FLightSceneProxy::Update()
{
	if (!Component)
	{
		bEnabled = false;
		return;
	}

	ShadowMapInfo.Location = Component->GetWorldLocation();

	// 모든 광원에 공통으로 필요한 값을 먼저 갱신한다.
	LightType = Component->GetLightType();

	bEnabled = Component->IsEnabled() && Component->GetIntensity() > 0.0f;
	bVisible = Component->IsVisible();
	bHiddenInGame = Component->IsHiddenInGame();
	ShadowMapResolution = Component->GetShadowMapResolution();

	switch (LightType)
	{
	case ELightType::Ambient:
	{
		RenderData = MakeAmbientLightData(CastChecked<UAmbientLightComponent>(Component));
		break;
	}
	case ELightType::Directional:
	{
		RenderData = MakeDirectionalLightData(CastChecked<UDirectionalLightComponent>(Component));
		break;
	}
	case ELightType::Point:
	{
		auto* Light = CastChecked<UPointLightComponent>(Component);
		bEnabled &= Light->GetAttenuationRadius() > 0.0f;

		RenderData = MakePointLightData(Light);
		break;
	}
	case ELightType::Spot:
	{
		auto* Light = CastChecked<USpotLightComponent>(Component);

		bEnabled &= Light->GetAttenuationRadius() > 0.0f;
		RenderData = MakeSpotLightData(Light);
		break;
	}
	}
}

void FLightSceneProxy::UpdateSpotSphere(float Radius, float OuterAngle)
{
	if (Radius == CachedSpotRadius && OuterAngle == CachedSpotOuterAngle)
		return;

	CachedSpotRadius = Radius;
	CachedSpotOuterAngle = OuterAngle;
	const float CosOuter = std::cos(OuterAngle);
	if (OuterAngle <= PI / 4.0f)
	{
		SpotSphereOffset = Radius / (2.0f * CosOuter);
		CullingSphereRadius = SpotSphereOffset;
	}
	else
	{
		SpotSphereOffset = Radius * CosOuter;
		CullingSphereRadius = Radius * std::sin(OuterAngle);
	}
}

FLightSceneProxy::FLightSceneProxy(ULightComponent* InComponent)
	: Component(InComponent), LightId(GNextLightId++)
{
	// Light 종류별 Shadow Map의 면 수만큼 행렬 배열 설정
	uint32 Count = 1;

	if (Component->GetLightType() == ELightType::Ambient)
	{
		Count = 0;
	}

	if (Component->GetLightType() == ELightType::Point)
	{
		Count = 6;
	}

	if (Component->GetLightType() == ELightType::Directional)
	{
		// Update에서 갱신
		Count = 0;
	}

	ShadowMapInfo.ShadowViews.SetNum(Count);

	Update();
}

FAmbientLightRenderData FLightSceneProxy::MakeAmbientLightData(const UAmbientLightComponent* LightComponent)
{
	return {
		.Color = LightComponent->GetLightColor(),
		.Intensity = LightComponent->GetIntensity(),
	};
}

FDirectionalLightRenderData FLightSceneProxy::MakeDirectionalLightData(const UDirectionalLightComponent* LightComponent)
{
	// ShadowMap 갯수를 Cascade 갯수로 지정
	// 굳이 capacity를 줄이지는 않음
	uint32 CascadeCount = LightComponent->GetCascadeCount();
	ShadowMapInfo.ShadowViews.SetNum(CascadeCount, false);

	for (int i = 0; i < CascadeCount; ++i)
	{
		FShadowView& ShadowView = ShadowMapInfo.ShadowViews[i];
		ShadowView.Forward = LightComponent->GetLightDirection();
		ShadowView.Near = 0.0f;
		ShadowView.Far = 0.0f;
		ShadowView.bIsOrthographic = true;
		ShadowView.ShadowBias = LightComponent->GetShadowBias();
		ShadowView.ShadowSlopeBias = LightComponent->GetShadowSlopeBias();
	}

	return {
		.Color = LightComponent->GetLightColor(),
		.Intensity = LightComponent->GetIntensity(),
		.Direction = LightComponent->GetLightDirection(),
		.ShadowAngularRadiusTan = std::tan(FMath::DegreesToRadians(LightComponent->GetShadowAngularDiameter()) * 0.5f),
		.CascadeCount = CascadeCount,
		.CascadeExponential = LightComponent->GetCascadeExponential(),
		.CascadeBlendRange = LightComponent->GetCascadeBlendRange(),
		.bCascadeDebug = LightComponent->GetCascadeDebug() ? 1.0f : 0.0f,
	};
}

FPointLightRenderData FLightSceneProxy::MakePointLightData(const UPointLightComponent* LightComponent)
{
	const float Radius = LightComponent->GetAttenuationRadius();

	CullingSphereCenter = LightComponent->GetWorldLocation();
	CullingSphereRadius = Radius;

	FPointLightRenderData Data = {
		.Color = LightComponent->GetLightColor(),
		.Intensity = LightComponent->GetIntensity(),
		.Location = LightComponent->GetWorldLocation(),
		.AttenuationRadiusSquaredInversed = 1.0f / (Radius * Radius),
		.ShadowSourceRadius = LightComponent->GetShadowSourceRadius(),
		.ShadowFar = Radius,
	};

	constexpr float x[] = { -1, 1, 0, 0, 0, 0 };
	constexpr float y[] = { 0, 0, -1, 1, 0, 0 };
	constexpr float z[] = { 0, 0, 0, 0, -1, 1 };

	for (int i = 0; i < 6; ++i)
	{
		FShadowView& ShadowView = ShadowMapInfo.ShadowViews[i];
		// TODO: 이 값들도 각 라이트가 알아서 적절하게 조절해야함
		float Near = 0.5f;
		ShadowView.Near = Near;
		ShadowView.bIsOrthographic = false;

		// TODO: Cube Seam
		const FMatrix Projection = FMatrix::MakePerspective(91.0f, 1.0f, Near, Radius);
		float ProjectionScale = Projection.M[2][1]; // Y Scale

		// z -1, 1 에서는 up vector로 외적이 불가능
		// 대신, forward vector를 전달해서 해결
		const FVector WorldUpVector = i < 4 ? UpVector : ForwardVector;
		FMatrix View = FMatrix::MakeLookDirection(ShadowMapInfo.Location, FVector{ x[i], y[i], z[i] }, WorldUpVector);

		Data.ViewProjection[i] = View * Projection;
		Data.ShadowNear = Near;
		Data.ShadowProjectionScale = ProjectionScale,

		ShadowView.Forward = FVector{ View.M[0][0], View.M[1][0], View.M[2][0] };
		ShadowView.View = View;
		ShadowView.Projection = Projection;
		ShadowView.ViewProjection = Data.ViewProjection[i];
		ShadowView.ShadowBias = LightComponent->GetShadowBias();
		ShadowView.ShadowSlopeBias = LightComponent->GetShadowSlopeBias();
	}

	return Data;
}                                                             

FSpotLightRenderData FLightSceneProxy::MakeSpotLightData(const USpotLightComponent* LightComponent)
{
	FShadowView& ShadowView = ShadowMapInfo.ShadowViews[0];
	// TODO: 이 값들도 각 라이트가 알아서 적절하게 조절해야함
	float Near = 0.5f;
	ShadowView.Near = Near;
	ShadowView.bIsOrthographic = false;

	const FVector Location = LightComponent->GetWorldLocation();

	const float OuterDegrees = std::clamp(LightComponent->GetOuterConeAngle(), 0.0f, 89.0f);
	const float OuterAngle = OuterDegrees * PI / 180.0f;
	const float Radius = std::max(0.0f, LightComponent->GetAttenuationRadius());
	const FVector Direction = LightComponent->GetLightDirection();

	const FVector WorldUpVector = FMatrix::CanMakeLookDirection(Direction, UpVector) ? UpVector : ForwardVector;

	const FMatrix View = FMatrix::MakeLookDirection(ShadowMapInfo.Location, Direction, WorldUpVector);
	const FMatrix Projection = FMatrix::MakePerspective(OuterDegrees * 2.0f, 1.0f, Near, Radius);

	ShadowView.Forward = FVector{ View.M[0][0], View.M[1][0], View.M[2][0] };

	ShadowView.View = View;
	ShadowView.Projection = Projection;
	ShadowView.ViewProjection = View * Projection;
	ShadowView.ShadowBias = LightComponent->GetShadowBias();
	ShadowView.ShadowSlopeBias = LightComponent->GetShadowSlopeBias();

	float ProjectionScale = Projection.M[2][1]; // Y Scale

	UpdateSpotSphere(Radius, OuterAngle);
	CullingSphereCenter = Location + Direction * SpotSphereOffset;

	return {
		.Color = LightComponent->GetLightColor(),
		.Intensity = LightComponent->GetIntensity(),
		.Location = Location,
		.AttenuationRadiusSquaredInversed = 1.0f / (Radius * Radius),
		.Direction = Direction,
		.CosInner = std::cos(LightComponent->GetInnerConeAngle() * PI / 180.0f),
		.CosOuter = std::cos(OuterAngle),
		.ViewProjection = View * Projection,
		.ShadowSourceRadius = LightComponent->GetShadowSourceRadius(),
		.ShadowNear = Near,
		.ShadowFar = Radius,
		.ShadowProjectionScale = ProjectionScale,
	};
}