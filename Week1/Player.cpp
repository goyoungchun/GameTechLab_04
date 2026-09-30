#include "Player.h"
#include <iostream>

#include "InputManager.h"
#include <random>
#include <ctime>
#include <cmath>

UPlayer::UPlayer()
{
	GenerateNewBall();

	for (int i = 0; i < 5; i++)
	{

		directer[i].CircleRenderer->setColor(getColorFromEnum(Red));
		directer[i].CircleRenderer->setPos(MyPos);
		directer[i].CircleRenderer->setRadius(0.01);

	}
}

UPlayer::~UPlayer()
{
	if (ball) delete ball;
	if (ball) delete map;
}

void UPlayer::SetMap(UMap* newmap)
{
	map = newmap;
}

void UPlayer::GenerateNewBall()
{
	if (ball) delete ball;

	pos = BallPosition;
	vel = FVector(0.0f, 0.0f, 0.0f);



	ball = new UBall;
	BallColors newColor =static_cast<BallColors>(GenerateBallColor());
	ball->Color = newColor;
	ball->CircleRenderer->setColor(getColorFromEnum(newColor));
	
	ball->CircleRenderer->setPos(BallPosition);
	ball->CircleRenderer->setRadius(radius);
}

void UPlayer::Update()
{
	if (InputManager::GetInstance()->GetSTATUS()[SPACE] && bcanShoot)
	{
		Shoot();
		bcanShoot = false;
	}

	// 정해지는 것이 아니라 더해지게

	pos.x += vel.x;
	pos.y += vel.y;
	pos.z += vel.z;

	ball->CircleRenderer->setPos(pos);

	//ball->CircleRenderer->setPos(FVector{ 0.0f + vel.x, -0.8f + vel.y , 0.0f + vel.z});

	Collision(map->GetBalls(), map->getLines());
	WallCollision();

	if (InputManager::GetInstance()->GetSTATUS()[LEFT])
	{
		OutputDebugStringA("!!! My Position is getting Bigger !!!");
		theta += ANGLESPEED;
	}
	if (InputManager::GetInstance()->GetSTATUS()[RIGHT])
	{
		OutputDebugStringA("!!! My Position is getting Smaller !!!");
		theta -= ANGLESPEED;
	}

	if (theta > MAXANGLE) theta = MAXANGLE;
	if (theta < MINANGLE) theta = MINANGLE;

	radian = theta * PI / 180.0f;

	MyPos.x = ANGLERADIUS[0] * cos(radian);
	MyPos.y = ANGLERADIUS[0] * sin(radian) + BallPosition.y;

	for (int i = 0; i < 5; i++)
	{
		float x = ANGLERADIUS[i] * cos(radian);
		float y = ANGLERADIUS[i] * sin(radian) + BallPosition.y;

		directer[i].CircleRenderer->setPos(FVector{x,y,0});
	}
	

}

void UPlayer::Shoot()
{

	OutputDebugStringA("!!! Shooter !!!");
	FVector vector;
	float dx = MyPos.x - BallPosition.x;
	float dy = MyPos.y - BallPosition.y;

	float dv = sqrt(dx * dx + dy * dy);

	vector.x = dx / dv; 
	vector.y = dy / dv;
	
	vector.x *= ballSpeed; 
	vector.y *= ballSpeed;

	vel = vector;
}

int UPlayer::GenerateBallColor()
{
	srand(static_cast<unsigned int>(time(NULL)));
	//return getColorFromEnum(static_cast<BallColors>(rand() % 5));

	return rand() % 5;
}

void UPlayer::Collision(UBall* balls[GameRow][GameCol], int lines)
{
	for (int i = 0; i < GameRow; i++) {
		for (int j = 0; j < GameCol; j++) {
			// nullptr 이면, Empty 컬러면 
			if(balls[i][j] == nullptr || balls[i][j]->Color == EmptyColor)
			{
				continue;
			}

			FVector ballPos = balls[i][j]->CircleRenderer->getPos();
			float dis = GetDistance(ballPos, pos);

			if (dis <= 2 * radius)
			{
				OutputDebugStringA("!!! Collision Detection !!!\n");

				float minDistance = inf;
				int minR= 0;
				int minC = 0;

				int r = i, c = j;

				// if 충돌 라인 r이 11일 경우 == r가 홀수, line이 짝수 or r이 짝수 line이 홀수
				if ((r % 2 != 0 && lines % 2 == 0) || (r % 2 == 0 && lines % 2 != 0))
				{
					for (auto& k : v11)
					{
						int newR = r + k[0];
						int newC = c + k[1];

						if (newR >= 12) newR = 12;
						if (newC >= 11) newC = 11;
						if (newR <= 0) newR = 0;
						if (newC <= 0) newC = 0;

						if (newR == r && newC >= 11) newC = 10;


						// balls[i]->CircleRenderer->getPos();
						float dis = GetDistance(pos, balls[newR][newC]->CircleRenderer->getPos());

						if (!balls[newR][newC]) continue;

						if (minDistance > dis && balls[newR][newC]->Color == EmptyColor)
						{
							minDistance = dis;
							minR = newR;
							minC = newC;
						}
					}

					// 공 인덱스 전달
					if (minDistance < 10.0f)
					{
						map->addBallandPop(minC, minR, ball->Color);
						GenerateNewBall();
						return;
					}
				}


				// else 충돌 라인이 r이 12인 경우 r이 짝수, line이 짝수 or r이 홀수 line이 홀수
				else if ((r % 2 == 0 && lines % 2 == 0) || (r % 2 != 0 && lines % 2 != 0))
				{
					for (auto& k : v12)
					{
						int newR = r + k[0];
						int newC = c + k[1];

						if (newR >= 12) newR = 12;
						if (newC >= 11) newC = 11;
						if (newR <= 0) newR = 0;
						if (newC <= 0) newC = 0;

						if (newR == r - 1 && newC >= 11) newC = 10;
						if (newR == r + 1 && newC >= 11) newC = 10;

						// balls[i]->CircleRenderer->getPos();
						float dis = GetDistance(pos, balls[newR][newC]->CircleRenderer->getPos());

						if (!balls[newR][newC]) continue;

						if (minDistance > dis && balls[newR][newC]->Color == EmptyColor )
						{
							minDistance = dis;
							minR = newR;
							minC = newC;
						}
					}

					// 공 인덱스 전달
					if (minDistance < 10.0f)
					{
						map->addBallandPop(minC, minR, ball->Color);
						GenerateNewBall();
						return;
					}
				}

			}
			bcanShoot = true;
		}
	}
}

FVector UPlayer::UVToPos(FVector& zeropos, int u, int v)
{
	FVector anspos;
	anspos.x = zeropos.x + 2 * radius * v;
	anspos.y = zeropos.y + sqrt(3) * radius * u;
	return anspos;
}

void UPlayer::IdxToUV(int& u, int& v, int idx)
{
	// u = idx / 12; v = idx % 12;

	u = idx / 23;
	v = idx % 23;

	//// lines 홀수 이냐 짝수이냐 에 따라서 12 또는 11로 변경한다.

	if (v > 12) 
	{
		u++;
		v = u + v - 12 ;
	}
	else if (v <= 12)
	{
		v += u;
	}

}

int UPlayer::UVToIdx(int u, int v)
{
	return u * 12 + v;
}

float UPlayer::GetDistance(FVector v1, FVector v2)
{
	float dx = v1.x - v2.x;
	float dy = v1.y - v2.y;

	return sqrt(dx * dx + dy * dy);
}

void UPlayer::WallCollision()
{
	FVector offset = pos;

	if (offset.x < leftBorder + radius)
	{
		offset.x = leftBorder + radius;
		ball->CircleRenderer->setPos(offset);
		vel.x *= -1.0f;
	}
	if (offset.x > rightBorder - radius)
	{
		offset.x = rightBorder - radius;
		ball->CircleRenderer->setPos(offset);
		vel.x *= -1.0f;
	}
	if (offset.y < topBorder + radius)
	{
		/*offset.y = topBorder + radius;
		ball->CircleRenderer->setPos(offset);
		vel.y *= -1.0f;*/
		GenerateNewBall();
	}
	if (offset.y > bottomBorder - radius)
	{
		offset.y = bottomBorder - radius;
		ball->CircleRenderer->setPos(offset);
		vel.y *= -1.0f;
	}

	
}