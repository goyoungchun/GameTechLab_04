#include "EnginePCH.h"
#include "PointLightSceneProxy.h"
#include "Component/PointLightComponent.h"


FPointLightSceneProxy::FPointLightSceneProxy(const UPointLightComponent* Component)
	: FLightSceneProxy(Component)
{
	FPointLightSceneProxy::UpdateFromComponent(Component);
}

void FPointLightSceneProxy::UpdateFromComponent(const ULightComponent* InComponent)
{
	FLightSceneProxy::UpdateFromComponent(InComponent);

	const UPointLightComponent* Component = static_cast<const UPointLightComponent*>(InComponent);
	AttenuationRadius = Component->GetAttenuationRadius();
	Falloff = Component->GetFalloff();
	Intensity = Component->GetIntensity();
	LightColor = Component->GetLightColor();
}
