#include "EnginePCH.h"
#include "ScreenUIRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/Texture/Texture2D.h"

void FScreenUIRenderer::Init()
{
	QuadRenderer.Init(256);
	TextRenderer.Init(512);
}

// PlayerRects는 0~1 비율 영역. 비어 있으면 플레이어 명령도 전체 화면 기준으로 그린다.
void FScreenUIRenderer::Render(const FGameUIBuffer& Buffer, uint32 ScreenW, uint32 ScreenH, const TArray<FRect>& PlayerRects)
{
	const TArray<FUIDrawCmd>& Commands = Buffer.GetCommands();
	if (Commands.IsEmpty() || ScreenW == 0 || ScreenH == 0)
		return;

	ScreenWidth = ScreenW;
	ScreenHeight = ScreenH;
	CurrentPlayerRects = &PlayerRects;
	CurrentArea = -1;

	for (const FUIDrawCmd& Cmd : Commands)
	{
		BeginArea(Cmd.PlayerIndex);
		switch (Cmd.Type)
		{
		case EUIDrawType::Rect: DrawRect(Cmd); break;
		case EUIDrawType::Bar:  DrawBar(Cmd);  break;
		case EUIDrawType::Text: DrawTextRender(Cmd); break;
		case EUIDrawType::Image: DrawImage(Cmd); break;
		}
	}

	QuadRenderer.Flush();
	RenderCommand::SetViewport(0, 0, ScreenW, ScreenH);
	CurrentPlayerRects = nullptr;
}

// 명령의 PlayerIndex에 맞는 영역으로 뷰포트, 직교 행렬, 배율을 바꾼다.
void FScreenUIRenderer::BeginArea(int32 PlayerIndex)
{
	// 플레이어 영역이 없으면 전체 화면으로 취급한다.
	const bool bHasPlayerRect = PlayerIndex > 0 && CurrentPlayerRects && PlayerIndex <= CurrentPlayerRects->Num();
	const int32 Area = bHasPlayerRect ? PlayerIndex : 0;
	if (Area == CurrentArea)
		return;

	QuadRenderer.Flush();
	CurrentArea = Area;

	if (Area == 0)
	{
		AreaX = 0.0f;
		AreaY = 0.0f;
		AreaWidth = static_cast<float>(ScreenWidth);
		AreaHeight = static_cast<float>(ScreenHeight);
	}
	else
	{
		const FRect& Rect = (*CurrentPlayerRects)[Area - 1];
		AreaX = std::round(Rect.X * ScreenWidth);
		AreaY = std::round(Rect.Y * ScreenHeight);
		AreaWidth = std::round((Rect.X + Rect.Width) * ScreenWidth) - AreaX;
		AreaHeight = std::round((Rect.Y + Rect.Height) * ScreenHeight) - AreaY;
	}

	RenderCommand::SetViewport(static_cast<uint32>(AreaX), static_cast<uint32>(AreaY),
		static_cast<uint32>(AreaWidth), static_cast<uint32>(AreaHeight));
	QuadRenderer.BeginFrame(static_cast<uint32>(AreaWidth), static_cast<uint32>(AreaHeight));
	Scale = AreaHeight / ReferenceHeight;
}

void FScreenUIRenderer::DrawRect(const FUIDrawCmd& Cmd) 
{
	float PixelX = Cmd.Position.X * AreaWidth;
	float PixelY = Cmd.Position.Y * AreaHeight;
	float Width = Cmd.Size.X * AreaWidth;
	float Height = Cmd.Size.Y * AreaHeight;

	QuadRenderer.AddRect(PixelX, PixelY, Width, Height, Cmd.Color);
}

void FScreenUIRenderer::DrawBar(const FUIDrawCmd& Cmd)
{
	float PixelX = Cmd.Position.X * AreaWidth;
	float PixelY = Cmd.Position.Y * AreaHeight;
	float Width = Cmd.Size.X * AreaWidth;
	float Height = Cmd.Size.Y * AreaHeight;

	QuadRenderer.AddRect(PixelX, PixelY, Width, Height, Cmd.BackgroundColor);
	if (Cmd.Ratio > 0.0f)
	{
		QuadRenderer.AddRect(PixelX, PixelY, Width * Cmd.Ratio, Height, Cmd.Color);
	}
	

}

void FScreenUIRenderer::DrawTextRender(const FUIDrawCmd& Cmd)
{
	QuadRenderer.Flush();

	float PixelX = Cmd.Position.X * AreaWidth;
	float PixelY = Cmd.Position.Y * AreaHeight;
	float Fontsize = Cmd.FontSize * Scale;

	UFont* Font = FindFont(Cmd.FontName);
	if (!Font)
		return;

	TextRenderer.OnRenderScreen(Cmd.Text, PixelX, PixelY, Fontsize, *Font,
		static_cast<uint32>(AreaWidth), static_cast<uint32>(AreaHeight), Cmd.Color, Cmd.Align);
}

void FScreenUIRenderer::DrawImage(const FUIDrawCmd& Cmd)
{
	if (!Cmd.Texture)
		return;

	float PixelX = Cmd.Position.X * AreaWidth;
	float PixelY = Cmd.Position.Y * AreaHeight;
	float Width = Cmd.Size.X * AreaWidth;
	float Height = Cmd.Size.Y * AreaHeight;

	// 원본 픽셀 비율을 유지한다. 화면 비율이 바뀌어도 이미지가 찌그러지지 않는다.
	if (Cmd.bKeepAspect)
	{
		const float SourceWidth = Cmd.Texture->GetWidth() * (Cmd.UVMax.X - Cmd.UVMin.X);
		const float SourceHeight = Cmd.Texture->GetHeight() * (Cmd.UVMax.Y - Cmd.UVMin.Y);
		if (SourceHeight > 0.0f)
		{
			Width = Height * SourceWidth / SourceHeight;
		}
	}

	QuadRenderer.AddImage(PixelX, PixelY, Width, Height, Cmd.Texture, Cmd.Color, Cmd.UVMin, Cmd.UVMax);
}

UFont* FScreenUIRenderer::FindFont(const FString& FontName)
{
	if (!DefaultFont)
	{
		DefaultFont = UAssetManager::GetAssetByPath<UFont>("Assets/Fonts/Pretendard.json");
	}
	if (FontName.empty())
	{
		return (DefaultFont);
	}

	auto FoundFont = FontCache.Find(FontName);
	if (FoundFont)
	{
		return *FoundFont;
	}

	UFont* Font = UAssetManager::GetAssetByPath<UFont>("Assets/Fonts/" + FontName + ".json");
	if (Font == nullptr)
	{
		return (DefaultFont);
	}
	FontCache.Add(FontName, Font);
	return (Font);
}