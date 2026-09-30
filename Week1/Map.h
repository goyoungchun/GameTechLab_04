#pragma once
#include "Ball.h"
#include <stdlib.h>
#include <queue>
#include <vector>

#define GameRow 14
#define GameCol 12

using std::queue;
using std::vector;

class UMap
{
public:
	int Lines;
	bool bGameOver;
	UBall* Balls[GameRow][GameCol] = {};
	vector<UBall*> DropBalls = {};
public:
	UMap()
	{
		Lines = 0;
		bGameOver = false;
	}
	
	~UMap()
	{
		UMapClear();
	}

	enum
	{
		MOVE_DIRECTIONS_COUNT = 6,
		BREAK_BALLS_COUNT = 3
	};

	void addBallandPop(int ix, int iy, BallColors color); // 발사한 공 구조체로 파라미터 변경해야함
	void randMapGenerator();
	void addLine();
	void UMapClear();
	void renderMap();
	int getMaxRow();
	int getLines(); // return Lines
	bool isGameOver(); // return bGameOver
	UBall* (*GetBalls())[GameCol];

	void DropBallUpdate(float dt);

private:
	// Todo: Fixed
	bool isBallDataOnBound(int x, int y);
	void updateBallData(int* outX, int* outY, int moveIndex, int currentY);
	void popBalls(int x, int y);

	bool checkAndExplodeBombs(int x, int y);

private:
	const int DX_12[MOVE_DIRECTIONS_COUNT] = { 1, -1, 0, 0, -1, -1 };
	const int DY_12[MOVE_DIRECTIONS_COUNT] = { 0, 0, 1, -1, 1, -1 };
	const int DX_11[MOVE_DIRECTIONS_COUNT] = { 1, -1, 0, 0, 1, 1 };
	const int DY_11[MOVE_DIRECTIONS_COUNT] = { 0, 0, 1, -1, 1, -1 };

};