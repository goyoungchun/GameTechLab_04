#include "ViewportLayout.h"
#include <algorithm>
#include <cassert>
#include <utility>

// 주어진 마우스 좌표에 존재하는 Splitter를 찾는 함수
// SWindow 및 SViewportPanel 자체에는 Splitter 막대기가 없으므로 기본적으로 nullptr
SSplitter* SWindow::findSplitter(const FPoint& point)
{
	return nullptr;
}
// 현재 Window가 차지하는 화면 좌표
const FRect& SWindow::getRect() const
{
	return Rect;
}
// Layout의 말단 노드와 실제 FViewport를 연결
SViewportPanel::SViewportPanel(FViewport& viewport)
	:Viewport(&viewport)
{
}
// Splitter 계산으로 결정된 최종 패널 영역을 저장
void SViewportPanel::Arrange(const FRect& rect)
{
	Rect = rect;
	if (Viewport == nullptr)
	{
		return;
	}
	FRect IRect = Rect;
	IRect.Top += menuBarSize;
	FViewportWindowState& windowState = Viewport->getWindowState();
	windowState.panelRect = Rect;
	windowState.imageRect = IRect; // Todo : 메뉴바 높이 고려 추가
}
// Splitter의 두 자식을 소유
SSplitter::SSplitter(std::unique_ptr<SWindow> sideLT, std::unique_ptr<SWindow> sideRB, float initialSplitRatio)
	: SideLT(std::move(sideLT)), SideRB(std::move(sideRB)), splitRatio(std::clamp(initialSplitRatio, 0.0f, 1.0f))
{
	// Splitter는 항상 두개의 자식을 가져야한다
	assert(SideLT != nullptr);
	assert(SideRB != nullptr);
}
// 현재 Splitter와 그 자식 중 마우스 좌표에 해당하는 Splitter를 찾음
SSplitter* SSplitter::findSplitter(const FPoint& point)
{
	// Splitter 전체 영역 밖이면 탐색 X
	if (!Rect.contains(point))
	{
		return nullptr;
	}
	// 현재 Splitter의 드래그 막대를 먼저 검사
	if (handleRect.contains(point))
	{
		return this;
	}
	if (SideLT != nullptr)
	{
		if (SSplitter* foundSplitter = SideLT->findSplitter(point))
		{
			return foundSplitter;
		}
	}
	if (SideRB != nullptr)
	{
		if (SSplitter* foundSplitter = SideRB->findSplitter(point))
		{
			return foundSplitter;
		}
	}
	return nullptr;
}
// 사용자가 드래그할 수 있는 Splitter 막대의 영역을 반환
const FRect& SSplitter::getHandleRect() const
{
	return handleRect;
}
float SSplitter::getSplitRatio() const
{
	return splitRatio;
}
// 패널이 너무 작아지지 않도록 분할 비율을 제한
float SSplitter::clampSplitRatio(float ratio, float usableExtent) const
{
	if (usableExtent <= 0.0f)
	{
		return 0.5f;
	}
	const float minimumExtent = (std::min)(minimumPanelSize, usableExtent * 0.5f);
	const float minimumRatio = minimumExtent / usableExtent;
	const float maximumRatio = 1.0f - minimumRatio;

	return std::clamp(ratio, minimumRatio, maximumRatio);
}
// 세로 막대를 기준으로 전체 영역을 세부분으로 분할 ( Left Handle Right )
void SSplitterV::Arrange(const FRect& rect)
{
	Rect = rect;
	const float totalWidth = (std::max)(Rect.getWidth(), 0.0f);
	const float actualHandleThickness = (std::min)(handleThickness, totalWidth);
	const float usableWidth = totalWidth - actualHandleThickness;

	splitRatio = clampSplitRatio(splitRatio, usableWidth);

	const float leftWidth = usableWidth * splitRatio;
	const float handleLeft = Rect.Left + leftWidth;

	handleRect = { handleLeft, Rect.Top, handleLeft + actualHandleThickness, Rect.Bottom };

	const FRect leftRect = { Rect.Left, Rect.Top, handleRect.Left, Rect.Bottom };
	const FRect rightRect = { handleRect.Right, Rect.Top, Rect.Right, Rect.Bottom };



	if (SideLT != nullptr)
	{
		SideLT->Arrange(leftRect);
	}

	if (SideRB != nullptr)
	{
		SideRB->Arrange(rightRect);
	}
}
// 마우스 X 좌표를 새로운 좌우 분할 비율로 변환
void SSplitterV::updateSplitFromPoint(const FPoint& point)
{
	const float totalWidth = (std::max)(Rect.getWidth(), 0.0f);
	const float actualHandleThickness = (std::min)(handleThickness, totalWidth);
	const float usableWidth = totalWidth - actualHandleThickness;

	if (usableWidth <= 0.0f)
	{
		return;
	}

	// 마우스가 Splitter 막대의 중앙에 위치한다고 보고 계산한다.
	const float desiredLeftWidth = point.X - Rect.Left - actualHandleThickness * 0.5f;
	const float desiredRatio = desiredLeftWidth / usableWidth;

	setSplitRatio(clampSplitRatio(desiredRatio, usableWidth));

	// 변경된 비율로 현재 Splitter의 자식들을 다시 배치한다.
	Arrange(Rect);
	if (linkedSplitter != nullptr)
	{
		const FRect linkedRect = linkedSplitter->getRect();
		linkedSplitter->Arrange(linkedRect);
	}
}
// 가로 막대를 기준으로 위 아래 나눔
void SSplitterH::Arrange(const FRect& rect)
{
	Rect = rect;

	const float totalHeight = (std::max)(Rect.getHeight(), 0.0f);
	const float actualHandleThickness = (std::min)(handleThickness, totalHeight);
	const float usableHeight = totalHeight - actualHandleThickness;

	splitRatio = clampSplitRatio(splitRatio,usableHeight);

	const float topHeight = usableHeight * splitRatio;
	const float handleTop = Rect.Top + topHeight;

	handleRect = {Rect.Left, handleTop, Rect.Right, handleTop + actualHandleThickness};

	const FRect topRect = {Rect.Left, Rect.Top, Rect.Right, handleRect.Top};
	const FRect bottomRect = {Rect.Left,handleRect.Bottom,Rect.Right,Rect.Bottom};

	if (SideLT != nullptr)
	{
		SideLT->Arrange(topRect);
	}

	if (SideRB != nullptr)
	{
		SideRB->Arrange(bottomRect);
	}
}
// 마우스의 Y 좌표를 상하 분할 비율로 변환하고 패널을 다시 배치
void SSplitterH::updateSplitFromPoint(const FPoint& point)
{
	const float totalHeight = (std::max)(Rect.getHeight(), 0.0f);
	const float actualHandleThickness = (std::min)(handleThickness, totalHeight);
	const float usableHeight = totalHeight - actualHandleThickness;

	if (usableHeight <= 0.0f)
	{
		return;
	}

	const float desiredTopHeight = point.Y - Rect.Top - actualHandleThickness * 0.5f;
	const float desiredRatio = desiredTopHeight / usableHeight;

	setSplitRatio(clampSplitRatio(desiredRatio, usableHeight));

	Arrange(Rect);
	if (linkedSplitter != nullptr)
	{
		const FRect linkedRect = linkedSplitter->getRect();
		linkedSplitter->Arrange(linkedRect);
	}
}

void SSplitter::linkSplitRatio(SSplitter& other)
{
	linkedSplitter = &other;
	other.linkedSplitter = this;
}

void SSplitter::setSplitRatio(float ratio)
{
	splitRatio = ratio;

	if (linkedSplitter != nullptr)
	{
		linkedSplitter->splitRatio = ratio;
	}
}
