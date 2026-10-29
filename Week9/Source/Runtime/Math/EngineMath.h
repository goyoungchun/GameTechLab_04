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

	static inline float Lerp(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	static inline FVector Lerp(const FVector& A, const FVector& B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	static inline FRotator Slerp(const FRotator& A, const FRotator& B, float Alpha)
	{
		return FQuat::Slerp(A.Quaternion(), B.Quaternion(), Alpha).Normalized().ToFRotator();
	}

	static inline float CubicInterpolation(const float& P0, const float& T0, const float& P1, const float& T1, const float& A)
	{
		const float A2 = A * A;
		const float A3 = A2 * A;

		return (((2 * A3) - (3 * A2) + 1) * P0) + ((A3 - (2 * A2) + A) * T0) + ((A3 - A2) * T1) + (((-2 * A3) + (3 * A2)) * P1);
	}

	static inline float InterpEaseIn(const float& A, const float& B, float Alpha, float Exp)
	{
		float const ModifiedAlpha = std::pow(Alpha, Exp);
		return Lerp(A, B, ModifiedAlpha);
	}

	static inline float InterpEaseOut(const float& A, const float& B, float Alpha, float Exp)
	{
		float const ModifiedAlpha = 1.f - std::pow(1.f - Alpha, Exp);
		return Lerp(A, B, ModifiedAlpha);
	}

	static inline float InterpEaseInOut(const float& A, const float& B, float Alpha, float Exp)
	{
		return Lerp(A, B, (Alpha < 0.5f) ?
			InterpEaseIn(0.f, 1.f, Alpha * 2.f, Exp) * 0.5f :
			InterpEaseOut(0.f, 1.f, Alpha * 2.f - 1.f, Exp) * 0.5f + 0.5f);
	}
}