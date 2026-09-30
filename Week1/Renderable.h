#pragma once

#include <vector>

#include "Renderer.h"
#include "Math.h"

class Renderable {
private:
	static std::vector<Renderable*> renderables;
	int slotNo;
protected:
	FVector pos;
	FColor color;
	float radius;

public:
	Renderable();
	~Renderable();

	Renderable(FVector newPos,
		FColor newColor,
		float newRadius);

	static void RenderAll(URenderer& renderer);

	void setPos(FVector newPos);
	void setColor(FColor newColor);
	void setRadius(float newRadius);
	FVector getPos();
	FColor getColor();
	virtual void Render(URenderer& renderer) = 0;
private:
	int getFirstEmpty();
};