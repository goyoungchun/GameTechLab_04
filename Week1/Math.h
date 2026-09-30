#pragma once
#include <cmath>

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};

struct FColor
{
	float R;
	float G;
	float B;
	float A;
};