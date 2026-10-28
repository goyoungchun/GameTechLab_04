#pragma once
#include <cmath>
#include "Vector.h"
#include "Vector2.h"
#include "Vector4.h"
//#include "TVector.h"
#include "Matrix.h"
#include "Quat.h"
#include "Rotator.h"
#include "Collision/Ray.h"
#include "VectorRegister.h"
#include "MatrixRegister.h"

#define PI 3.141592f
#define INDEX_NONE -1

namespace FMath
{
	//static float PI = std::acosf(-1);

	static inline bool IsNearlyZero(float Value, float ErrorTolerance = 1e-20f)
	{
		return std::abs(Value) <= ErrorTolerance;
	}

	static inline bool IsNearlyEqual(float Value1, float Value2, float ErrorTolerance = 1e-4f)
	{
		return std::abs(Value1 - Value2) <= ErrorTolerance;
	}

	static inline float Clamp(float Value, float Min, float Max)
	{
		if (Value < Min) return Min;
		if (Value > Max) return Max;
		return Value;
	}

	static inline uint32 Clamp(uint32 Value, uint32 Min, uint32 Max)
	{
		if (Value < Min) return Min;
		if (Value > Max) return Max;
		return Value;
	}

	static inline float RadiansToDegrees(float Radian)
	{
		return Radian * (180.0f / PI);
	}

	static inline float DegreesToRadians(float Degree)
	{
		return Degree * (PI / 180.0f);
	}

	static FQuat QuatInterpolationTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpolationSpeed)
	{
		if (InterpolationSpeed <= 0.f)
		{
			return Target;
		}

		return FQuat::Slerp(Current, Target, FMath::Clamp(InterpolationSpeed * DeltaTime, 0.f, 1.f));
	}

	static FVector VectorInterpoationTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpolationSpeed)
	{
		if (InterpolationSpeed <= 0.f)
		{
			return Target;
		}

		const FVector Dist = Target - Current;

		const FVector DeltaMove = Dist * FMath::Clamp(DeltaTime * InterpolationSpeed, 0.f, 1.f);
		return Current + DeltaMove;
	}
}