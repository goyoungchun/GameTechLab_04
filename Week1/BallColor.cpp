#include "BallColor.h"

FColor getColorFromEnum(BallColors color)
{
	switch (color)
	{
	case Red:
		return FColor{ 1.0f, 0.0f, 0.0f, 1.0f };
	case Blue:
		return FColor{ 0.0f, 0.0f, 1.0f, 1.0f };
	case Green:
		return FColor{ 0.0f, 1.0f, 0.0f, 1.0f };
	case Yellow:
		return FColor{ 1.0f, 1.0f, 0.0f, 1.0f };
	case Purple:
		return FColor{ 1.0f, 0.0f, 1.0f, 1.0f };
	case EmptyColor:
		return FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	case WallColor:
		return FColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	case Lightening:
		return FColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	case Bomb:
		return FColor{ 0.5f, 0.5f, 0.5f, 1.0f };
	}
}