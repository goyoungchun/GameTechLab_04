#pragma once
#include "Primitive.h"
#include "Circle.h"
#include "Math.h"
#include "BallColor.h"
#define BALLSPEED 0.01f

class UBall : public UPrimitive
{
public:
	UBall()
	{
		TotalNumBalls += 1;

		Location.x = ((float)(rand() % 161 - 80) / 100.0f);
		Location.y = ((float)(rand() % 161 - 80) / 100.0f);

		Velocity.x = ((float)(rand() % 100 - 50)) * BALLSPEED;
		Velocity.y = ((float)(rand() % 100 - 50)) * BALLSPEED;

		Radius = 0.01f + ((float)rand() / RAND_MAX) * 0.1f;
		Mass = Radius * 10.0f;

		CircleRenderer = new Circle;
	}

	~UBall() override
	{
		TotalNumBalls -= 1;
		delete CircleRenderer;
	}

	float GetRadius() override;
	float GetMass() override;
	FVector GetLocation() override;
	FVector GetVelocity() override;
	void SetLocation(FVector vector) override;
	void SetVelocity(FVector vector) override;
	void Update() override;
	bool CheckCollision(UPrimitive* targetball) override;
	void ResolveCollision(UPrimitive* targetball) override;
	float GetDistance(UPrimitive* targetball);
	void Gravity(float dt) override;
	float Dot(FVector v1, FVector v2);

public:
	FVector Location;
	FVector Velocity;
	float Radius;
	float Mass;
	static int TotalNumBalls;
	static bool bGravity;

	
	Circle* CircleRenderer;
	BallColors Color;
	int BounceCnt = 0;
	
};