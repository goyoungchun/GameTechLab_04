#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>

// 차량과 공이 공유하는 짧은 외형 Scale 애니메이션. 충돌체나 시퀀스 데이터는 변경하지 않는다.
class FHitSquashAnimation
{
public:
	void Play(float InStartScale = 1.0f)
	{
		StartScale = std::isfinite(InStartScale) && InStartScale > 0.0f ? InStartScale : 1.0f;
		Scale = StartScale;
		ElapsedTime = 0.0f;
		bActive = true;
	}

	void Reset()
	{
		bActive = false;
		ElapsedTime = 0.0f;
		StartScale = Scale = 1.0f;
	}

	bool IsActive() const { return bActive; }

	float Update(float DeltaTime)
	{
		if (!bActive || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f) return Scale;
		struct FScaleKey { float Time; float Scale; };
		static constexpr FScaleKey Keys[] = {
			{ 0.0f, 1.0f }, { 0.05f, 0.90f }, { 0.12f, 1.06f },
			{ 0.19f, 0.98f }, { 0.26f, 1.0f }
		};
		ElapsedTime += DeltaTime;
		if (ElapsedTime >= Keys[std::size(Keys) - 1].Time)
		{
			Reset();
			return Scale;
		}
		for (size_t Index = 1; Index < std::size(Keys); ++Index)
		{
			if (ElapsedTime > Keys[Index].Time) continue;
			const FScaleKey& Left = Keys[Index - 1];
			const FScaleKey& Right = Keys[Index];
			const float Alpha = std::clamp((ElapsedTime - Left.Time) / (Right.Time - Left.Time), 0.0f, 1.0f);
			const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
			const float FromScale = Index == 1 ? StartScale : Left.Scale;
			Scale = FromScale + (Right.Scale - FromScale) * SmoothAlpha;
			break;
		}
		return Scale;
	}

private:
	bool bActive = false;
	float ElapsedTime = 0.0f;
	float StartScale = 1.0f;
	float Scale = 1.0f;
};
