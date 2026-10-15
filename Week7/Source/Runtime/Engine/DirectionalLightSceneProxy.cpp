#include "EnginePCH.h"
#include "DirectionalLightSceneProxy.h"
#include "Component/DirectionalLightComponent.h"


FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(const UDirectionalLightComponent* Component)
	: FLightSceneProxy(Component)
{
	FDirectionalLightSceneProxy::UpdateFromComponent(Component);
}

void FDirectionalLightSceneProxy::UpdateFromComponent(const ULightComponent* InComponent)
{
	FLightSceneProxy::UpdateFromComponent(InComponent);

	const UDirectionalLightComponent* Component = static_cast<const UDirectionalLightComponent*>(InComponent);
	Direction = Component->GetWorldRotation().Quaternion().RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized();
	Intensity = Component->GetIntensity();
	LightColor = Component->GetLightColor();
}
