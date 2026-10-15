#include "EnginePCH.h"
#include "Component/AmbientLightComponent.h"
#include "Engine/AmbientLightSceneProxy.h"

FLightSceneProxy* UAmbientLightComponent::CreateSceneProxy() const
{
	return new FAmbientLightSceneProxy(this);
}
