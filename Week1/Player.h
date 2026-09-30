#pragma once
#include <vector>
#include "Triangle.h"
#include "Ball.h"
#include "Math.h"
#include "Map.h"

const std::vector<FColor> color = {
{1.0f, 0.0f, 0.0f, 1.0f},
{0.0f, 1.0f, 0.0f, 1.0f},
{0.0f, 0.0f, 1.0f, 1.0f},
{1.0f, 1.0f, 0.0f, 1.0f},
{1.0f, 0.0f, 1.0f, 1.0f}
};

class UPlayer {
public:
	UPlayer();
	~UPlayer();

private:
	UBall* ball;		// FVector Pos
	UBall directer[5];
	UBall* Balls[GameRow][GameCol] = {};
	Triangle* triangle;  // FVector Pos
	bool bisCollision = false;
	FVector vel = { 0.0, 0.0f, 0.0f };
	FVector MyPos = { 0.0f, -0.5f , 0.0f };
	FVector pos = { 0.0f, -0.8f, 0.0f };
	UMap* map;
	bool bcanShoot = true;
	float theta = 0.0f;
	float radian = 0.0f;
public:
	void SetMap(UMap* newmap);
	void Update();
	void Shoot();
	void Collision(UBall* balls[GameRow][GameCol], int lines);
	void GenerateNewBall();
	void WallCollision();
	int GenerateBallColor();
	FVector UVToPos(FVector& zeropos, int u, int v);
	void IdxToUV(int& u, int& v, int idx);
	int UVToIdx(int u, int v);
	float GetDistance(FVector v1, FVector v2);

private:
	const float ballSpeed = 0.07f;
	const FVector BallPosition = { 0.0f, -0.8f , 0.0f };
	const float radius = 0.07f;
	const float leftBorder = -0.8f;
	const float rightBorder = 0.8f;
	const float topBorder = -1.0f;
	const float bottomBorder = 1.0f;
	const float inf = 1000.0f;
	const float MAXANGLE = 165.0f;
	const float MINANGLE = 15.0f;
	const float PI = 3.141592;
	const float ANGLESPEED =5.0f;
	const float ANGLERADIUS[5] = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
	const std::vector<std::vector<int>> v11 = { { -1, 0 },{ -1, +1 }, { 0, -1 }, { 0, +1 }, { +1, 0 }, { +1, +1 } };
	const std::vector<std::vector<int>> v12 = { { -1, -1 },{ -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 } };
};