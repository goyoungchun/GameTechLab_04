#include "EnginePCH.h"

#include "BoxComponent.h"
#include "Render/LineBatcher.h"

#include <algorithm>

void UBoxComponent::OnPropertyChanged(const FString& PropertyName)
{
	if (PropertyName == "BoxExtent")
	{
		BoxExtent.X = std::max(0.0f, BoxExtent.X);
		BoxExtent.Y = std::max(0.0f, BoxExtent.Y);
		BoxExtent.Z = std::max(0.0f, BoxExtent.Z);
	}
	Super::OnPropertyChanged(PropertyName);
}

FBox UBoxComponent::CalcLocalBounds() const
{
	return FBox{ -BoxExtent, BoxExtent };
}

void UBoxComponent::DrawShape(FLineBatcher& LineBatcher) const
{
	LineBatcher.AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), GetShapeColor());
}
