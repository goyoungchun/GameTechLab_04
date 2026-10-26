#pragma once

#include "UI/GameUIBuffer.h"
#include "UI/ScreenQuadRenderer.h"
#include "Text/TextRenderer.h"
#include "Math/Vector2.h"

class UFont;

class FScreenUIRenderer
{
public:
	FScreenUIRenderer() = default;
	~FScreenUIRenderer() = default;

	void Init();
	void Render(const FGameUIBuffer& Buffer, uint32 ScreenW, uint32 ScreenH, const TArray<FRect>& PlayerRects);

private:
	void BeginArea(int32 PlayerIndex);

	void DrawRect(const FUIDrawCmd& Cmd);
	void DrawBar(const FUIDrawCmd& Cmd);
	void DrawTextRender(const FUIDrawCmd& Cmd);
	void DrawImage(const FUIDrawCmd& Cmd);

	UFont* FindFont(const FString& FontName);

	FScreenQuadRenderer QuadRenderer;
	FTextRenderer		TextRenderer;

	UFont* DefaultFont = nullptr;
	TMap<FString, UFont*> FontCache;

	// Render 동안만 쓰는 현재 상태
	uint32	ScreenWidth = 0;
	uint32	ScreenHeight = 0;
	int32	CurrentArea = -1;
	float	AreaX = 0.0f, AreaY = 0.0f, AreaWidth = 0.0f, AreaHeight = 0.0f;
	float	Scale = 1.0f;
	const TArray<FRect>* CurrentPlayerRects = nullptr;
	

	static constexpr float ReferenceHeight = 1080.0f;
};
