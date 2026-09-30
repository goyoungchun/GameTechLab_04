#pragma once

#include "../Core/Core.h"
#include "../Core/enum.h"
#include <array>

inline constexpr uint8 invalidViewportId = 0;
inline constexpr uint8 maxViewportCount = 4;

// 화면상의 한 점
struct FPoint
{
	float X = 0.0f;
	float Y = 0.0f;
};
// 한 화면 사각형의 크기
struct FRect
{
	float Left = 0.0f;
	float Top = 0.0f;
	float Right = 0.0f;
	float Bottom = 0.0f;

	float getWidth() const { return Right - Left; }
	float getHeight() const { return Bottom - Top; }
	bool contains(const FPoint& point) const
	{
		return point.X >= Left && point.X <= Right && point.Y >= Top && point.Y <= Bottom;
	}
};
// 카메라 시점 종류
enum class EViewportType : uint8_t
{
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};
// 레이아웃 개수 모드
enum class EViewportLayoutMode : uint8_t
{
	SinglePane = 1,
	TwoPane,
	ThreePane,
	FourPane
};
// Viewport들이 공유하는 정보들
struct FViewportSharedSettings
{
	inline static constexpr std::array<float, 7> snapPresets{ 0.1, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0 };
	uint8_t snapPresetIndex = 1;
	float cameraSpeed = 5.0f;
	float getSnapSize() const { return snapPresets[snapPresetIndex]; }
};
// Viewport 하나가 현재 Editor UI에서 어떤 상태인가
struct FViewportWindowState
{
	FRect panelRect;
	FRect imageRect;
	bool bImageHovered = false;
	bool bFocused = false;
};

struct FViewportRenderSettings
{
	EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;

	uint32 ShowFlags =
		static_cast<uint32>(EEngineShowFlags::SF_Primitives) |
		static_cast<uint32>(EEngineShowFlags::SF_BillboardText) |
		static_cast<uint32>(EEngineShowFlags::SF_WorldAxis) |
		static_cast<uint32>(EEngineShowFlags::SF_BoundingBox) |
		static_cast<uint32>(EEngineShowFlags::SF_Grid);

	bool HasShowFlag(EEngineShowFlags flag) const
	{
		return (ShowFlags & static_cast<uint32>(flag)) != 0;
	}

	void SetShowFlag(EEngineShowFlags flag,bool bEnabled)
	{
		const uint32 value = static_cast<uint32>(flag);

		if (bEnabled)
		{
			ShowFlags |= value;
		}
		else
		{
			ShowFlags &= ~value;
		}
	}
};
