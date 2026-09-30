#include "Triangle.h"

Triangle::Triangle(FVector newPos,
	FColor newColor,
	float newRadius):Renderable(newPos, newColor, newRadius) {

}

void Triangle::Render(URenderer& renderer) {
	renderer.UpdateConstant(pos, radius, color);
	renderer.RenderPrimitive(renderer.VertexBufferTriangle, renderer.NumVerticesTriangle);
}