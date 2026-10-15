#include "EnginePCH.h"
#include "Component/DirectionalLightComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/World.h"
#include "Engine/DirectionalLightSceneProxy.h"

UDirectionalLightComponent::~UDirectionalLightComponent()
{
	if (AActor* Owner = GetOwner())
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetScene().UnregisterDirectLight(this);
		}
	}
}

void UDirectionalLightComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (AActor* Owner = GetOwner())
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetScene().RegisterDirectLight(this);
		}
	}
}

void UDirectionalLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
	return new FDirectionalLightSceneProxy(this);
}
