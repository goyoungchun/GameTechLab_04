#pragma once
#include "Text/TextRenderer.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Core/EngineString.h"
#include "Container/Array.h"

class FTexture2D;

enum class EUIDrawType : uint8 { Text, Rect, Bar, Image };

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
	FTexture2D*	Texture = nullptr;
	FVector2	UVMin = FVector2(0.0f, 0.0f);
	FVector2	UVMax = FVector2(1.0f, 1.0f);
	bool		bKeepAspect = false;
};


class FGameUIBuffer
{
public:
	void AddText(FVector2 InPosition, const FString& InText, float InFontSize, const FVector4& InColor, ETextAlign InAlign, const FString& InFontName = "");
	void AddRect(FVector2 InPosition, FVector2 InSize, const FVector4& InColor);
	void AddBar(FVector2 InPosition, FVector2 InSize, float InRatio, const FVector4& InColor, const FVector4& InBackgroundColor);
	void AddImage(FVector2 InPosition, FVector2 InSize, FTexture2D* InTexture, const FVector4& InColor,
		FVector2 InUVMin = FVector2(0.0f, 0.0f), FVector2 InUVMax = FVector2(1.0f, 1.0f), bool bInKeepAspect = false);
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
