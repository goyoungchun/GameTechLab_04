#include "Renderable.h"


std::vector<Renderable*> Renderable::renderables;

Renderable::Renderable() {
	int slot = getFirstEmpty();
	if (slot == -1) {
		slotNo = renderables.size();
		renderables.push_back(this);
	}
	else {
		slotNo = slot;
		renderables[slot] = this;
	}
}

Renderable::Renderable(FVector newPos,
	FColor newColor,
	float newRadius)
{
	pos.x = newPos.x;
	pos.y = newPos.y;
	pos.z = newPos.z;
	color.R = newColor.R;
	color.G = newColor.G;
	color.B = newColor.B;
	color.A = newColor.A;
	radius = newRadius;
	int slot = getFirstEmpty();
	if (slot == -1) {
		slotNo = renderables.size();
		renderables.push_back(this);
	}
	else {
		slotNo = slot;
		renderables[slot] = this;
	}
}

Renderable::~Renderable() {
	renderables[slotNo] = nullptr;
}

int Renderable::getFirstEmpty() {
	for (int i = 0; i < renderables.size(); i++) {
		if (renderables[i] == nullptr)
			return i;
	}
	return -1;
}

void Renderable::RenderAll(URenderer& renderer) {
	for (int i = 0; i < renderables.size(); i++) {
		if (renderables[i] != nullptr)
		{
			renderables[i]->Render(renderer);	
		}
	}
}

void Renderable::setPos(FVector newPos) {
	pos.x = newPos.x;
	pos.y = newPos.y;
	pos.z = newPos.z;
}

void Renderable::setColor(FColor newColor) {
	color.R = newColor.R;
	color.G = newColor.G;
	color.B = newColor.B;
	color.A = newColor.A;
}

void Renderable::setRadius(float newRadius) {
	radius = newRadius;
}

FVector Renderable::getPos() {
	return pos;
}

FColor Renderable::getColor() {
	return color;
}