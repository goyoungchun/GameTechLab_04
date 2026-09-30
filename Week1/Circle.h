#pragma once

#include "Math.h"
#include "Renderable.h"

class Circle : public Renderable {
public:
	void Render(URenderer& renderer) override;
};