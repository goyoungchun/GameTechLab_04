#include "EnginePCH.h"
#include "PhysicsSweep.h"
#include "Component/BoxComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/SphereComponent.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	bool IsFinite(const FVector& V)
	{
		return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z);
	}

	void SetHit(const FRay& Ray, float Distance, float Radius, const FVector& Normal,
		float Depth, FHitResult& Hit)
	{
		Hit.Distance = Distance;
		Hit.Normal = Normal;
		Hit.ImpactNormal = Normal;
		Hit.ImpactPoint = Ray.PointAt(Distance) - Normal * Radius;
		Hit.PenetrationDepth = std::max(0.0f, Depth);
		Hit.bStartPenetrating = Hit.PenetrationDepth > 1e-6f;
	}

	bool RaySphere(const FRay& Ray, const FVector& Center, float Radius, float Limit, float& OutDistance)
	{
		const FVector Offset = Ray.Origin - Center;
		const float A = Ray.Direction.Dot(Ray.Direction);
		const float B = Offset.Dot(Ray.Direction);
		const float C = Offset.Dot(Offset) - Radius * Radius;
		if (A <= 0.0f || B > 0.0f) return false;
		const float Discriminant = B * B - A * C;
		if (Discriminant < 0.0f) return false;
		const float Distance = (-B - std::sqrt(Discriminant)) / A;
		if (Distance < 0.0f || Distance > Limit) return false;
		OutDistance = Distance;
		return true;
	}

	// Capsule의 로컬 Z 중심 선분과 구 이동체의 Minkowski 합을 검사한다.
	bool SweepCapsule(const FRay& Ray, float Limit, float QueryRadius, float TargetRadius,
		float HalfSegment, FHitResult& Hit)
	{
		const float Radius = QueryRadius + TargetRadius;
		const FVector Closest(0.0f, 0.0f, std::clamp(Ray.Origin.Z, -HalfSegment, HalfSegment));
		const FVector Delta = Ray.Origin - Closest;
		const float DistanceSquared = Delta.Dot(Delta);
		if (DistanceSquared <= Radius * Radius)
		{
			const float Distance = std::sqrt(DistanceSquared);
			const FVector Normal = Distance > 1e-6f ? Delta / Distance : FVector(1.0f, 0.0f, 0.0f);
			SetHit(Ray, 0.0f, QueryRadius, Normal, Radius - Distance, Hit);
			return true;
		}
		if (Limit <= 0.0f) return false;

		float Nearest = Limit;
		bool bHit = false;
		for (float Z : { -HalfSegment, HalfSegment })
		{
			float Distance;
			if (RaySphere(Ray, FVector(0.0f, 0.0f, Z), Radius, Nearest, Distance))
			{
				Nearest = Distance;
				bHit = true;
			}
		}
		const float A = Ray.Direction.X * Ray.Direction.X + Ray.Direction.Y * Ray.Direction.Y;
		const float B = Ray.Origin.X * Ray.Direction.X + Ray.Origin.Y * Ray.Direction.Y;
		const float C = Ray.Origin.X * Ray.Origin.X + Ray.Origin.Y * Ray.Origin.Y - Radius * Radius;
		const float Discriminant = B * B - A * C;
		if (A > 1e-12f && Discriminant >= 0.0f)
		{
			const float Distance = (-B - std::sqrt(Discriminant)) / A;
			const float Z = Ray.Origin.Z + Ray.Direction.Z * Distance;
			if (Distance >= 0.0f && Distance <= Nearest && Z >= -HalfSegment && Z <= HalfSegment)
			{
				Nearest = Distance;
				bHit = true;
			}
		}
		if (!bHit) return false;
		const FVector Center = Ray.PointAt(Nearest);
		const FVector ContactDelta = Center - FVector(0.0f, 0.0f, std::clamp(Center.Z, -HalfSegment, HalfSegment));
		const float Length = ContactDelta.Length();
		SetHit(Ray, Nearest, QueryRadius, Length > 1e-6f ? ContactDelta / Length : -Ray.Direction, 0.0f, Hit);
		return true;
	}

	FVector ClosestOnBox(const FVector& P, const FVector& Extent)
	{
		return FVector(std::clamp(P.X, -Extent.X, Extent.X), std::clamp(P.Y, -Extent.Y, Extent.Y),
			std::clamp(P.Z, -Extent.Z, Extent.Z));
	}

	void BoxHit(const FRay& Ray, float Distance, float Radius, const FVector& Extent, FHitResult& Hit)
	{
		const FVector Center = Ray.PointAt(Distance);
		const FVector Delta = Center - ClosestOnBox(Center, Extent);
		const float Length = Delta.Length();
		FVector Normal;
		float Depth = 0.0f;
		if (Length > 1e-6f)
		{
			Normal = Delta / Length;
			if (Distance == 0.0f) Depth = Radius - Length;
		}
		else
		{
			const FVector FaceDistances = Extent - Center.GetAbs();
			int32 Axis = 0;
			if (FaceDistances.Y < FaceDistances[Axis]) Axis = 1;
			if (FaceDistances.Z < FaceDistances[Axis]) Axis = 2;
			Normal[Axis] = Center[Axis] >= 0.0f ? 1.0f : -1.0f;
			if (Distance == 0.0f) Depth = Radius + FaceDistances[Axis];
		}
		SetHit(Ray, Distance, Radius, Normal, Depth, Hit);
	}

	// 구 중심과 Box의 거리 제곱은 축별 경계 통과 사이에서 이차식이다.
	// 각 구간의 첫 근을 찾아 모서리 주변의 확장 AABB 오검출을 피한다.
	bool SweepBox(const FRay& Ray, float Limit, float Radius, const FVector& Extent, FHitResult& Hit)
	{
		const FVector InitialDelta = Ray.Origin - ClosestOnBox(Ray.Origin, Extent);
		if (InitialDelta.Dot(InitialDelta) <= Radius * Radius)
		{
			BoxHit(Ray, 0.0f, Radius, Extent, Hit);
			return true;
		}
		if (Limit <= 0.0f) return false;
		std::array<float, 8> Times;
		int32 Count = 0;
		Times[Count++] = 0.0f;
		Times[Count++] = Limit;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (std::abs(Ray.Direction[Axis]) <= 1e-12f) continue;
			for (float Side : { -1.0f, 1.0f })
			{
				const float T = (Side * Extent[Axis] - Ray.Origin[Axis]) / Ray.Direction[Axis];
				if (T > 0.0f && T < Limit) Times[Count++] = T;
			}
		}
		std::sort(Times.begin(), Times.begin() + Count);
		for (int32 Index = 0; Index + 1 < Count; ++Index)
		{
			const float Begin = Times[Index];
			const float End = Times[Index + 1];
			const FVector Mid = Ray.PointAt(Begin * 0.5f + End * 0.5f);
			float A = 0.0f, B = 0.0f, C = -Radius * Radius;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (Mid[Axis] >= -Extent[Axis] && Mid[Axis] <= Extent[Axis]) continue;
				const float Offset = Ray.Origin[Axis] - (Mid[Axis] > 0.0f ? Extent[Axis] : -Extent[Axis]);
				A += Ray.Direction[Axis] * Ray.Direction[Axis];
				B += Offset * Ray.Direction[Axis];
				C += Offset * Offset;
			}
			float Distance = Begin;
			if (A > 1e-12f)
			{
				const float Discriminant = B * B - A * C;
				if (Discriminant < 0.0f) continue;
				Distance = (-B - std::sqrt(Discriminant)) / A;
			}
			else if (C > 0.0f) continue;
			if (Distance < Begin - 1e-5f || Distance > End + 1e-5f) continue;
			BoxHit(Ray, std::clamp(Distance, Begin, End), Radius, Extent, Hit);
			return true;
		}
		return false;
	}
}

bool SweepSphereAgainstComponent(const FRay& Ray, float MaxDistance, float Radius,
	UPrimitiveComponent* Target, FHitResult& OutHit)
{
	if (!Target || !IsFinite(Ray.Origin) || !IsFinite(Ray.Direction)
		|| !std::isfinite(MaxDistance) || MaxDistance < 0.0f || !std::isfinite(Radius) || Radius < 0.0f)
		return false;
	const FQuat Rotation = Target->GetWorldRotation().Quaternion().Normalized();
	const FVector Location = Target->GetWorldLocation();
	const FVector Scale = Target->GetWorldScale3D().GetAbs();
	if (!IsFinite(Location) || !IsFinite(Scale)) return false;
	const FRay LocalRay{ Rotation.Inverse().RotateVector(Ray.Origin - Location), Rotation.Inverse().RotateVector(Ray.Direction) };
	FHitResult Hit(1.0f);
	bool bHit = false;
	bool bLocal = true;
	if (auto* Sphere = Cast<USphereComponent>(Target))
	{
		const float TargetRadius = Sphere->GetScaledSphereRadius(Scale);
		if (TargetRadius > 0.0f && std::isfinite(TargetRadius))
			bHit = SweepCapsule(LocalRay, MaxDistance, Radius, TargetRadius, 0.0f, Hit);
	}
	else if (auto* Capsule = Cast<UCapsuleComponent>(Target))
	{
		const float TargetRadius = Capsule->GetCapsuleRadius() * std::min({ Scale.X, Scale.Y, Scale.Z });
		const float HalfHeight = Capsule->GetCapsuleHalfHeight() * Scale.Z;
		if (TargetRadius > 0.0f && std::isfinite(TargetRadius) && std::isfinite(HalfHeight))
			bHit = SweepCapsule(LocalRay, MaxDistance, Radius, TargetRadius, std::max(0.0f, HalfHeight - TargetRadius), Hit);
	}
	else if (auto* Box = Cast<UBoxComponent>(Target))
	{
		const FVector Extent = Box->GetBoxExtent() * Scale;
		if (IsFinite(Extent) && Extent.X >= 0.0f && Extent.Y >= 0.0f && Extent.Z >= 0.0f)
			bHit = SweepBox(LocalRay, MaxDistance, Radius, Extent, Hit);
	}
	else if (Target->GetMeshData())
	{
		const FBox Bounds = Target->CalcBounds();
		const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
		const FVector Center = Bounds.Min * 0.5f + Bounds.Max * 0.5f;
		if (IsFinite(Extent) && IsFinite(Center) && Extent.X >= 0.0f && Extent.Y >= 0.0f && Extent.Z >= 0.0f)
			bHit = SweepBox(FRay{ Ray.Origin - Center, Ray.Direction }, MaxDistance, Radius, Extent, Hit);
		Hit.ImpactPoint += Center;
		bLocal = false;
	}
	if (!bHit || !std::isfinite(Hit.Distance) || !IsFinite(Hit.Normal)) return false;
	if (bLocal)
	{
		Hit.Normal = Rotation.RotateVector(Hit.Normal);
		Hit.ImpactNormal = Hit.Normal;
		Hit.ImpactPoint = Location + Rotation.RotateVector(Hit.ImpactPoint);
	}
	Hit.HitComponent = Target;
	Hit.Time = MaxDistance > 0.0f ? std::clamp(Hit.Distance / MaxDistance, 0.0f, 1.0f) : 0.0f;
	OutHit = Hit;
	return true;
}
