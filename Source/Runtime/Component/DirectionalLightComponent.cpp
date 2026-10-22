#include "EnginePCH.h"
#include "Component/DirectionalLightComponent.h"
#include "UObject/UObjectIterator.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
	ShadowMapResolution = SharedShadowMapResolution;
}

void UDirectionalLightComponent::OnPropertyChanged(const FString& PropertyName)
{
	Super::OnPropertyChanged(PropertyName);

	if (PropertyName == "ShadowMapResolution")
	{
		SharedShadowMapResolution = ShadowMapResolution;
		// 모든 월드의 Directional Light가 같은 그림자 텍스처를 공유한다.
		for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
		{
			It->ShadowMapResolution = SharedShadowMapResolution;
		}
	}
}

void UDirectionalLightComponent::SetCascadeCount(uint32 Count)
{
	CascadeCount = Count;
	OnPropertyChanged("CascadeCount");
}

void UDirectionalLightComponent::SetCascadeExponential(uint32 Exponential)
{
	CascadeExponential = Exponential;
	OnPropertyChanged("CascadeExponential");
}

void UDirectionalLightComponent::SetCascadeBlendRange(float BlendRange)
{
	CascadeBlendRange = BlendRange;
	OnPropertyChanged("CascadeBlendRange");
}

void UDirectionalLightComponent::SetCascadeDebug(bool Debug)
{
	bCascadeDebug = Debug;
	OnPropertyChanged("bCascadeDebug");
}
