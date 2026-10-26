#pragma once
#include "Text/TextRenderer.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Core/EngineString.h"
#include "Container/Array.h"

enum class EUIDrawType : uint8 { Text, Rect, Bar };

struct FUIDrawCmd
{
	EUIDrawType Type = EUIDrawType::Rect;
	int32		PlayerIndex = 0;
	float		Ratio = 1.0f;
	float		FontSize = 32.0f;
	FVector2	Position;
	FVector2	Size;
	FVector4	Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4	BackgroundColor = FVector4(0.0f, 0.0f, 0.0f, 0.5f);
	FString		Text;	
	FString		FontName;
	ETextAlign	Align = ETextAlign::Left;
};


class FGameUIBuffer
{
public:
	void AddText(FVector2 InPosition, const FString& InText, float InFontSize, const FVector4& InColor, ETextAlign InAlign, const FString& InFontName = "");
	void AddRect(FVector2 InPosition, FVector2 InSize, const FVector4& InColor);
	void AddBar(FVector2 InPosition, FVector2 InSize, float InRatio, const FVector4& InColor, const FVector4& InBackgroundColor);
	void BeginPlayer(int32 InPlayerIndex);
	void EndPlayer();
	void Clear();
	const TArray<FUIDrawCmd>& GetCommands() const;

private:
	void Push(FUIDrawCmd&& InCommand);
	TArray<FUIDrawCmd> Commands;
	int32 CurrentPlayer = 0;
	bool bOverflowWarned = false;
	static constexpr int32 MaxCommands = 2048;
};
