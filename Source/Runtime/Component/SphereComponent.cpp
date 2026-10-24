#include "EnginePCH.h"

#include "SphereComponent.h"
#include "Render/LineBatcher.h"

#include <algorithm>

void USphereComponent::OnPropertyChanged(const FString& PropertyName)
{
	if (PropertyName == "SphereRadius")
		SphereRadius = std::max(0.0f, SphereRadius);
	Super::OnPropertyChanged(PropertyName);
}

FBox USphereComponent::CalcLocalBounds() const
{
	const FVector Extent(SphereRadius);
	return FBox{ -Extent, Extent };
}

void USphereComponent::DrawShape(FLineBatcher& LineBatcher) const
{
	const FTransform SpherePose(GetWorldRotation(), GetWorldLocation());
	LineBatcher.AddSphere(GetScaledSphereRadius(GetWorldScale3D()), SpherePose.GetLocalMatrix(), GetShapeColor());
}
