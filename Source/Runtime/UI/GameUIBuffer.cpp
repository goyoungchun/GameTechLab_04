#include "EnginePCH.h"
#include "GameUIBuffer.h"

void FGameUIBuffer::AddText(FVector2 InPosition, const FString& InText, float InFontSize, const FVector4& InColor, ETextAlign InAlign, const FString& InFontName)
{
	FUIDrawCmd AddCommand;
	AddCommand.Type = EUIDrawType::Text;
	AddCommand.Position = InPosition;
	AddCommand.Text = InText;
	AddCommand.FontSize = InFontSize;
	AddCommand.Color = InColor;
	AddCommand.Align = InAlign;
	AddCommand.FontName = InFontName;
	
	Push(std::move(AddCommand));
}

void FGameUIBuffer::AddRect(FVector2 InPosition, FVector2 InSize, const FVector4& InColor)
{
	FUIDrawCmd AddCommand;
	AddCommand.Type = EUIDrawType::Rect;
	AddCommand.Position = InPosition;
	AddCommand.Size = InSize;
	AddCommand.Color = InColor;
	
	Push(std::move(AddCommand));
}

void FGameUIBuffer::AddBar(FVector2 InPosition, FVector2 InSize, float InRatio, const FVector4& InColor, const FVector4& InBackgroundColor)
{
	FUIDrawCmd AddCommand;
	AddCommand.Type = EUIDrawType::Bar;
	AddCommand.Position = InPosition;
	AddCommand.Size = InSize;
	AddCommand.Ratio = std::clamp(InRatio, 0.0f, 1.0f);
	AddCommand.Color = InColor;
	AddCommand.BackgroundColor = InBackgroundColor;
	
	Push(std::move(AddCommand));
}

void FGameUIBuffer::AddImage(FVector2 InPosition, FVector2 InSize, FTexture2D* InTexture, const FVector4& InColor,
	FVector2 InUVMin, FVector2 InUVMax, bool bInKeepAspect)
{
	if (!InTexture)
		return;

	FUIDrawCmd AddCommand;
	AddCommand.Type = EUIDrawType::Image;
	AddCommand.Position = InPosition;
	AddCommand.Size = InSize;
	AddCommand.Texture = InTexture;
	AddCommand.Color = InColor;
	AddCommand.UVMin = InUVMin;
	AddCommand.UVMax = InUVMax;
	AddCommand.bKeepAspect = bInKeepAspect;

	Push(std::move(AddCommand));
}

void FGameUIBuffer::BeginPlayer(int32 InPlayerIndex)
{
	CurrentPlayer = InPlayerIndex;
}

void FGameUIBuffer::EndPlayer()
{
	CurrentPlayer = 0;
}

void FGameUIBuffer::Clear()
{
	Commands.Reset();
	CurrentPlayer = 0;
	bOverflowWarned = false;
}

const TArray<FUIDrawCmd>& FGameUIBuffer::GetCommands() const
{
	return (Commands);
}

void FGameUIBuffer::Push(FUIDrawCmd&& InCommand)
{
	if (Commands.Num() >= MaxCommands)
	{
		if (!bOverflowWarned)
		{
			HTR_LOG(Warning, "[UI] Too many UI commands this frame (max {})", MaxCommands);
			bOverflowWarned = true;
		}
		return;
	}

	InCommand.PlayerIndex = CurrentPlayer;
	Commands.Add(std::move(InCommand));
}