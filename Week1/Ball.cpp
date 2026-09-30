#include "Ball.h"

float UBall::GetRadius() 
{
	return Radius;
}

float UBall::GetMass() 
{
	return Mass;
}

FVector UBall::GetLocation() 
{
	return Location;
}

FVector UBall::GetVelocity() 
{
	return Velocity;
}

void UBall::SetLocation(FVector vector) 
{
	Location = vector;
}

void UBall::SetVelocity(FVector vector) 
{
	Velocity = vector;
}

void UBall::Update() 
{
	Location.x += Velocity.x;
	Location.y += Velocity.y;
}

bool UBall::CheckCollision(UPrimitive* targetball) 
{
	float TotalRadius = Radius + targetball->GetRadius();

	if (TotalRadius >= GetDistance(targetball)) return true;
	else return false;
}

void UBall::ResolveCollision(UPrimitive* targetball) 
{
	FVector TargetLocation = targetball->GetLocation();
	FVector TargetVelocity = targetball->GetVelocity();
	float TargetRadius = targetball->GetRadius();
	float TargetMass = targetball->GetMass();

	float dx = Location.x - TargetLocation.x;
	float dy = Location.y - TargetLocation.y;

	float distance = sqrt(dx * dx + dy * dy);

	FVector Normal(dx / distance, dy / distance, 0);

	float overlap = Radius + TargetRadius - distance;

	if (overlap > 0.0f)
	{
		Location.x += Normal.x * (overlap * 0.5f);
		Location.y += Normal.y * (overlap * 0.5f);

		TargetLocation.x -= Normal.x * (overlap * 0.5f);
		TargetLocation.y -= Normal.y * (overlap * 0.5f);

		SetLocation(Location);
		targetball->SetLocation(TargetLocation);
	}

	float dvx = Velocity.x - TargetVelocity.x;
	float dvy = Velocity.y - TargetVelocity.y;

	FVector dVelocity(dvx, dvy, 0);

	float j = -2 * (Mass * TargetMass) / (Mass + TargetMass) * (Dot(Normal, dVelocity));

	FVector J(Normal.x * j / Mass, Normal.y * j / Mass, 0);
	FVector TargetJ(Normal.x * j / TargetMass, Normal.y * j / TargetMass, 0);

	Velocity.x = Velocity.x + J.x;
	Velocity.y = Velocity.y + J.y;

	TargetVelocity.x = TargetVelocity.x - TargetJ.x;
	TargetVelocity.y = TargetVelocity.y - TargetJ.y;

	targetball->SetVelocity(TargetVelocity);
}

float UBall::GetDistance(UPrimitive* targetball)
{
	FVector TargetLocation = targetball->GetLocation();

	float dx = Location.x - TargetLocation.x;
	float dy = Location.y - TargetLocation.y;

	float distance = sqrt(dx * dx + dy * dy);

	return distance;
}

void UBall::Gravity(float dt)
{
	float G = -9.8f;

	if (bGravity) {
		Velocity.y = Velocity.y + G * dt;
	}

}

float UBall::Dot(FVector v1, FVector v2)
{
	float result = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	return result;
}