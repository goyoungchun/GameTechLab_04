#pragma once
#include "ViewportTypes.h"
#include "Viewport.h"
#include <memory>

class SSplitter;

class SWindow
{
public:
	virtual ~SWindow() = default;
	virtual void Arrange(const FRect& rect) = 0;
	virtual SSplitter* findSplitter(const FPoint& point);
	const FRect& getRect() const;
protected:
	FRect Rect;
};

class SViewportPanel : public SWindow
{
public :
	explicit SViewportPanel(FViewport& viewport);
	void Arrange(const FRect& rect) override;
private:
	FViewport* Viewport = nullptr;
	float menuBarSize = 30.0f;
};

class SSplitter : public SWindow
{
public:
	SSplitter(std::unique_ptr<SWindow> sideLT, std::unique_ptr<SWindow> sideRB, float splitRatio = 0.5f);
	SSplitter* findSplitter(const FPoint& point) override;
	virtual void updateSplitFromPoint(const FPoint& point) = 0;

	const FRect& getHandleRect() const;
	float getSplitRatio() const;

	void linkSplitRatio(SSplitter& other);

protected:
	float clampSplitRatio(float ratio, float usableExtent) const;

	std::unique_ptr<SWindow> SideLT;
	std::unique_ptr<SWindow> SideRB;

	void setSplitRatio(float ratio);
	SSplitter* linkedSplitter = nullptr;

	FRect handleRect;
	float splitRatio = 0.5f;
	float handleThickness = 6.0f;
	float minimumPanelSize = 100.0f;
};

class SSplitterV : public SSplitter
{
public:
	using SSplitter::SSplitter;

	void Arrange(const FRect& rect) override;
	void updateSplitFromPoint(const FPoint& point) override;
};

class SSplitterH : public SSplitter
{
public:
	using SSplitter::SSplitter;

	void Arrange(const FRect& rect) override;
	void updateSplitFromPoint(const FPoint& point) override;
};
