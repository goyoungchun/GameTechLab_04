#include "EnginePCH.h"
#include "SpotLightSceneProxy.h"
#include "Component/SpotLightComponent.h"


FSpotLightSceneProxy::FSpotLightSceneProxy(const USpotLightComponent* Component)
	: FLightSceneProxy(Component)
{
	FSpotLightSceneProxy::UpdateFromComponent(Component);
}

void FSpotLightSceneProxy::UpdateFromComponent(const ULightComponent* InComponent)
{
	FLightSceneProxy::UpdateFromComponent(InComponent);

	const USpotLightComponent* Component = static_cast<const USpotLightComponent*>(InComponent);
	Direction = Component->GetWorldRotation().Quaternion().RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized();
	InnerConeAngle = Component->GetInnerConeAngle();
	OuterConeAngle = Component->GetOuterConeAngle();
	AttenuationRadius = Component->GetAttenuationRadius();
	Falloff = Component->GetFalloff();
	Intensity = Component->GetIntensity();
	LightColor = Component->GetLightColor();
}
