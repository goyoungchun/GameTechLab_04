#include "EnginePCH.h"
#include "LightSceneProxy.h"
#include "Component/LightComponent.h"


FLightSceneProxy::FLightSceneProxy(const ULightComponent* Component)
{
	FLightSceneProxy::UpdateFromComponent(Component);
}

void FLightSceneProxy::UpdateFromComponent(const ULightComponent* InComponent)
{
	Position = InComponent->GetWorldLocation();
}
