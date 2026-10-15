#include "EnginePCH.h"
#include "AmbientLightSceneProxy.h"
#include "Component/AmbientLightComponent.h"


FAmbientLightSceneProxy::FAmbientLightSceneProxy(const UAmbientLightComponent* Component)
	: FLightSceneProxy(Component)
{
	FAmbientLightSceneProxy::UpdateFromComponent(Component);
}

void FAmbientLightSceneProxy::UpdateFromComponent(const ULightComponent* InComponent)
{
	FLightSceneProxy::UpdateFromComponent(InComponent);

	const UAmbientLightComponent* Component = static_cast<const UAmbientLightComponent*>(InComponent);
	Intensity = Component->GetIntensity();
	LightColor = Component->GetLightColor();
}
