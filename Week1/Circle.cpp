#include "Circle.h"

void Circle::Render(URenderer& renderer) {
	renderer.UpdateConstant(pos, radius, color);
	renderer.RenderPrimitive(renderer.VertexBufferSphere, renderer.NumVerticesSphere);
}