#pragma once

#include "Core/Core.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Color.h"

struct FVertexSimple
{
	float x, y, z;    // Position
	float r, g, b, a; // Color

	FVector GetPosition() const { return FVector(x, y, z); }
};

struct FVertexTextured
{
	float x, y, z;
	float u, v;

	FVector GetPosition() const { return FVector(x, y, z); }
};

struct FNormalVertex
{
	FVector pos;		// xyz
	FVector normal;		// normal
	FLinearColor color;	// RGBA
	FVector2 tex;		// uv
};
