#include "Map.h"

#include <iostream>
#include <cassert>

using namespace std;

//#include "GameManager.h"

void UMap::addBallandPop(int ix, int iy, BallColors color)  // Player가 위치 좌표 및 색상 정보를 담은 구조체를 입력하는걸로 변환 해야함
{

	//gameOver 밑에 초과해서 게임 오버 되는 경우에는 어디서 처리하게 할까?
	/*
	if (getLines() + getMaxRow() >= GameRow)
	{
		bGameOver = true;
		//GameManager::GameOver()
		UMapClear();
		return;
	}
	*/

	int AddedScore = 0;
	int PopMul = 100;
	int DropMul = 500;

	int PopVisited[GameRow][GameCol] = { 0, };
	PopVisited[iy][ix] = 1;

	//addBall
	Balls[iy][ix]->Color = color;
	Balls[iy][ix]->CircleRenderer->setColor(getColorFromEnum(color));

	queue<BallData> q; // 위치 좌표 및 색상 정보를 담은 구조체로 변환 해야함
	queue<BallData> breakable; // 파괴할 공을 담는 큐 이것 또한 위치 좌표 및 색상 정보를 담은 구조체로 변환 해야함

	BallData RootBall;
	RootBall.x = ix;
	RootBall.y = iy;
	RootBall.color = color;

	q.push(RootBall); // Player가 새로 추가한 공을 root
	breakable.push(RootBall);

	// Lightning, Bomb effect
	for (int i = 0; i < MOVE_DIRECTIONS_COUNT; i++)
	{
		int nx = ix;
		int ny = iy;

		if (isBallDataOnBound(nx, ny) == false)
		{
			continue;
		}

		updateBallData(&nx, &ny, i, iy);

		if (Balls[ny][nx] == nullptr)
		{
			continue;
		}

		if (Balls[ny][nx]->Color == Lightening)
		{
			for (int k = 0; k < GameCol; k++)
			{
				if (Balls[ny][k] == nullptr)
				{
					continue;
				}
				
				popBalls(k, ny);
			}
		} 
		else if (Balls[ny][nx]->Color == Bomb)
		{
			for (int k = 0; k < MOVE_DIRECTIONS_COUNT; k++)
			{
				int bx = nx;
				int by = ny;

				updateBallData(&bx, &by, k, by);
				if (checkAndExplodeBombs(bx, by) == false)
				{
					continue;
				}

				for (int j = 0; j < MOVE_DIRECTIONS_COUNT; j++)
				{
					int mx = bx;
					int my = by;

					updateBallData(&mx, &my, j, my);
					if (checkAndExplodeBombs(mx, my) == false)
					{
						continue;
					}
				}
			}
		}
	}


	//Pop
	for (int i = 0; i < 50; i++) //BFS를 통해 같은 색의 공을 탐색하고, 3개 이상이면 Pop
	{
		BallData currentBall = q.front(); //현재공 좌표 업데이트, 공에서 색 추출 필요

		for (int k = 0; k < MOVE_DIRECTIONS_COUNT; k++)
		{
			int nx = currentBall.x;
			int ny = currentBall.y;

			updateBallData(&nx, &ny, k, currentBall.y);

			if (isBallDataOnBound(nx, ny) == false)
			{
				continue;
			}

			if (Balls[ny][nx] == nullptr || Balls[ny][nx]->Color == EmptyColor)
			{
				continue;
			}

			assert(Balls[ny][nx] != nullptr);
			if (Balls[ny][nx]->Color == currentBall.color && !PopVisited[ny][nx])
			{
				PopVisited[ny][nx] = 1;

				BallData newBallData;
				newBallData.x = nx;
				newBallData.y = ny;
				newBallData.color = Balls[ny][nx]->Color;

				q.push(newBallData);
				breakable.push(newBallData);
			}
		}

		q.pop();

		if (q.empty())
		{
			break;
		}
	}
	

	if (breakable.size() >= BREAK_BALLS_COUNT) // 3개 이상이면 Pop
	{
		AddedScore += breakable.size() * PopMul; // pop 점수 계산 추가
		while (!breakable.empty())
		{
			BallData currentBall = breakable.front(); 

			popBalls(currentBall.x, currentBall.y);
			breakable.pop();
		}
	}


	//Drop
	queue<BallData> dq;
	int DropVisited[GameRow][GameCol] = { 0, };

	for(int i = 0; i < GameCol; i++)
	{
		if (Balls[Lines][i]->Color == EmptyColor || Balls[Lines][i] == nullptr)
		{
			continue;
		}
		
		BallData RootBall;
		RootBall.x = i;
		RootBall.y = Lines;
		RootBall.color = Balls[Lines][i]->Color;

		dq.push(RootBall);
		DropVisited[Lines][i] = 1;

		for (int k = 0; k < 50; k++)
		{
			BallData currentBall = dq.front();

			for (int j = 0; j < MOVE_DIRECTIONS_COUNT; j++)
			{
				int nx = currentBall.x;
				int ny = currentBall.y;

				updateBallData(&nx, &ny, j, currentBall.y);

				if (isBallDataOnBound(nx, ny) == false)
				{
					continue;
				}

				if (Balls[ny][nx] != nullptr && Balls[ny][nx]->Color != EmptyColor && !DropVisited[ny][nx])
				{
					DropVisited[ny][nx] = 1;

					BallData newBallData;
					newBallData.x = nx;
					newBallData.y = ny;
					newBallData.color = Balls[ny][nx]->Color;

					dq.push(newBallData);
				}
			}

			dq.pop();

			if (dq.empty())
			{
				break;
			}
		}
	}

	for (int i = 0; i < GameRow; i++)
	{
		for (int j = 0; j < GameCol; j++)
		{
			if (Balls[i][j] != nullptr && Balls[i][j]->Color != EmptyColor && !DropVisited[i][j])
			{
				AddedScore += DropMul; // Drop 점수 추가

				popBalls(i, j);
			}
		}
	}
	
	//GameManager에 addedScore 전달
	//AddedScore가 이번 발사 단계에 대한 최종 추가 점수
	

	//Clear
	for (int i = 0; i < GameCol; i++)
	{
		if (Balls[Lines][i]->Color != EmptyColor)
		{
			break;
		}

		//UMapClear();
		//GameManager::Clear
		return;
	}
}



void UMap::randMapGenerator() // 현재 3*GameCol 사이즈의 랜덤 맵만 생성하게 만들어져있음
{
	int colorCount = 5;
	int rowCount = 6; // 랜덤으로 생성할 행 개수 -> 레벨 스테이지 도입 시 수정 필요
	
	float BasicRadius = 0.07f;
	FColor BasicColor = { 1.0f, 1.0f, 0.0f, 1.0f };

	for (int i = 0; i < GameRow - 1; i++)
	{
		for (int j = 0; j < GameCol; j++)
		{
			UBall* Ball = new UBall();
			if (i < rowCount) {
			
				int randomColor = rand() % colorCount; // 랜덤 색상 생성
				Ball->CircleRenderer->setColor(getColorFromEnum(static_cast<BallColors>(randomColor)));
				Ball->Color = static_cast<BallColors>(randomColor);
				Ball->CircleRenderer->setRadius(BasicRadius);
			}	
			else 
			{
				Ball->CircleRenderer->setColor(getColorFromEnum(EmptyColor));
				Ball->Color = EmptyColor;
				Ball->CircleRenderer->setRadius(BasicRadius);
			}
			Balls[i][j] = Ball;
		}
	}
}
void UMap::renderMap()
{
	float BasicRadius = 0.07f;
	FVector BasicLocation = { -0.83f , 0.9f, 0.0f };
	FColor BasicColor = { 1.0f, 1.0f, 0.0f, 1.0f };

	for (int i = 0; i < GameRow -1; i++)
	{
		for (int j = 0; j < GameCol; j++)
		{
			FVector AddLocation = { BasicRadius * 2.1f * j, BasicRadius * 1.8f * i, 0.0f };
			FVector FinalLocation = { BasicLocation.x + AddLocation.x, BasicLocation.y - AddLocation.y , BasicLocation.z + AddLocation.z };
			if(Balls[i][j] != nullptr)
				Balls[i][j]->CircleRenderer->setPos(FinalLocation);
			if ((i + Lines) % 2 != 0)
			{
				if (j == GameCol - 1) // 홀수행의 마지막 공은 생성하지 않음
				{
					delete Balls[i][j];
					Balls[i][j] = nullptr;
				}
				if (Balls[i][j] != nullptr)
					Balls[i][j]->CircleRenderer->setPos(FVector{ FinalLocation.x + BasicRadius, FinalLocation.y, FinalLocation.z });
			}
		}
	}
}
void UMap::addLine() 
{
	Lines++;
	float BasicRadius = 0.07f;
	
	for (int i = 0; i < GameCol; i++) // 초반에 게임오버 여부 판단하고 라인추가 실행 후 게임오버 전달
	{
		if (Balls[GameRow - 2][i] != nullptr && Balls[GameRow - 2][i]->Color != EmptyColor)
		{
			bGameOver = true; 
		}
	}

	for (int i = 0; i < GameCol; i++)
	{
		if(Balls[GameRow - 2][i] != nullptr)
		{
			delete Balls[GameRow-2][i];
			Balls[GameRow-2][i] = nullptr;
		}
	}
	for (int i = GameRow - 2; i > 0; i--)
	{
		for (int j = 0; j < GameCol; j++)
		{
			{
				Balls[i][j] = Balls[i - 1][j];
			}
		}
	}

	for (int i = 0; i < GameCol; i++)
	{
		if (Balls[0][i] != nullptr)
		{
			Balls[0][i] = nullptr;
		}
		UBall* Ball = new UBall();
		Ball->CircleRenderer->setColor(getColorFromEnum(WallColor));
		Ball->CircleRenderer->setRadius(BasicRadius);
		Ball->Color = WallColor;
		Balls[0][i] = Ball;
	}
	if (bGameOver)
	{
		//GameManager::GameOver
	}
}
void UMap::UMapClear()
{
	for (int i = 0; i < GameRow; i++)
	{
		for (int j = 0; j < GameCol; j++)
		{
			if (Balls[i][j] != nullptr)
			{
				delete Balls[i][j];
				Balls[i][j] = nullptr;
				DropBalls.clear();
			}
		}
	}
}
void UMap::DropBallUpdate(float dt)
{
	if (!DropBalls.empty())
	{
		for (int i = 0; i < DropBalls.size(); i++)
		{
			int MaxCount = 1;
			FVector DropBallPos = DropBalls[i]->CircleRenderer->getPos();
			DropBalls[i]->Velocity.y = DropBalls[i]->Velocity.y - 0.1f * dt;
			DropBallPos.y += DropBalls[i]->Velocity.y;
			DropBallPos.x += DropBalls[i]->Velocity.x;
			DropBalls[i]->CircleRenderer->setPos(DropBallPos);
			if (DropBalls[i]->CircleRenderer->getPos().y < -1.05f)
			{
				if (DropBalls[i]->BounceCnt >= MaxCount)
				{
					delete DropBalls[i];
					DropBalls.erase(DropBalls.begin() + i);
					continue;
				}
				else
				{
					DropBalls[i]->Velocity.y = DropBalls[i]->Velocity.y * -0.4f;
					DropBalls[i]->BounceCnt += 1;
				}
			}
		}
	}
}
int UMap::getLines()
{
	return Lines;
}
int UMap::getMaxRow() // 필요 없어짐 
{
	return 3;
}
bool UMap::isGameOver() //필요 없어짐 
{
	return bGameOver;
}
UBall* (*UMap::GetBalls())[GameCol]
{
	return Balls;
}

// Todo: Fixed 
bool UMap::isBallDataOnBound(int x, int y)
{
	if (x >= 0 && x < GameCol && y >= 0 && y < GameRow - 1)
	{
		return true;
	}

	return false;
}

void UMap::updateBallData(int* outX, int* outY, int moveIndex, int currentY)
{
	/*
	if ((currentY + Lines) % 2)
	{
		*outX += DX_11[moveIndex];
		*outY += DY_11[moveIndex];
	}
	else
	{
		*outX += DX_12[moveIndex];
		*outY += DY_12[moveIndex];
	}
	*/

	if (((currentY + Lines) % 2) == 0)
	{
		*outX += DX_12[moveIndex];
		*outY += DY_12[moveIndex];
	}
	else
	{
		*outX += DX_11[moveIndex];
		*outY += DY_11[moveIndex];
	}
}

void UMap::popBalls(int ballsX, int ballsY)
{
	UBall* PopBall = new UBall;
	PopBall->CircleRenderer->setColor(Balls[ballsY][ballsX]->CircleRenderer->getColor());
	PopBall->CircleRenderer->setPos(Balls[ballsY][ballsX]->CircleRenderer->getPos());
	PopBall->CircleRenderer->setRadius(0.07f);

	float x = (float)rand() / RAND_MAX * 0.01f;
	if (rand() % 2) x *= -1;
	PopBall->SetVelocity({ x,0.01f,0.0f });
	DropBalls.push_back(PopBall);

	Balls[ballsY][ballsX]->CircleRenderer->setColor(getColorFromEnum(EmptyColor)); // Pop
	Balls[ballsY][ballsX]->Color = EmptyColor;
}

bool UMap::checkAndExplodeBombs(int x, int y)
{
	if (isBallDataOnBound(x, y) == false)
	{
		return false;
	}

	if (Balls[y][x] == nullptr || Balls[y][x]->Color == EmptyColor)
	{
		return false;
	}

	popBalls(x, y);

	return true;
}