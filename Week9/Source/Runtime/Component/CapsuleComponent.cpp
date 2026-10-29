#include "EnginePCH.h"

#include "CapsuleComponent.h"
#include "Render/LineBatcher.h"

#include <algorithm>

void UCapsuleComponent::OnPropertyChanged(const FString& PropertyName)
{
	if (PropertyName == "CapsuleRadius" || PropertyName == "CapsuleHalfHeight" || PropertyName == "CapsuleSize")
	{
		CapsuleRadius = std::max(0.0f, CapsuleRadius);
		CapsuleHalfHeight = std::max(CapsuleRadius, CapsuleHalfHeight);
	}
	Super::OnPropertyChanged(PropertyName);
}

FBox UCapsuleComponent::CalcLocalBounds() const
{
	const FVector Extent(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight);
	return FBox{ -Extent, Extent };
}

void UCapsuleComponent::DrawShape(FLineBatcher& LineBatcher) const
{
	LineBatcher.AddCapsule(CapsuleRadius, CapsuleHalfHeight, GetWorldMatrix(), GetShapeColor());
}
