#pragma once
#include "Renderable.h"
#include "Renderer.h"
#include "Math.h"
class Triangle : Renderable {
public:
	Triangle(FVector newPos,
		FColor newColor,
		float newRadius);
	void Render(URenderer& renderer) override;
};