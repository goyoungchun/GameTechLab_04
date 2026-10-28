#include "EnginePCH.h"
#include "TimeController.h"

#include <algorithm>

void FTimeController::Update(double UnscaledDeltaTime)
{
	if (!std::isfinite(UnscaledDeltaTime) || UnscaledDeltaTime <= 0.0)
		return;


	UpdateHitStop(UnscaledDeltaTime);
	UpdateSlomo(UnscaledDeltaTime);
}

void FTimeController::SetTimeDilation(float InTimeDilation)
{
	if (!std::isfinite(InTimeDilation) || InTimeDilation < 0.0f)
		return;

	BaseTimeDilation = InTimeDilation;

	if (!bSlomoActive)
		CurrentTimeDilation = BaseTimeDilation;
}
float FTimeController::GetEffectiveTimeDilation() const
{
	return HitStopTimeRemaining > 0.0 ? 0.0f: CurrentTimeDilation;
}
double FTimeController::GetGameDeltaTime(double UnscaledDeltaTime) const
{
	return UnscaledDeltaTime * GetEffectiveTimeDilation();
}


void FTimeController::RequestHitStop(const FHitStopParams& Params)
{
	if (!Params.IsValid())
		return;
	HitStopTimeRemaining = std::max(HitStopTimeRemaining, static_cast<double>(Params.Duration));
}
void FTimeController::StopHitStop()
{
	HitStopTimeRemaining = 0.0;
}
bool FTimeController::IsHitStopped() const
{
	return GetHitStopTimeRemaining() > 0.0;
}
double FTimeController::GetHitStopTimeRemaining() const
{
	return HitStopTimeRemaining;
}


void FTimeController::RequestSlomo(const FSlomoParams& Params, bool bHoldAtEnd)
{
	if (!Params.IsValid() || Params.Duration <= 0.0f)
		return;
	SlomoStartTimeDilation = CurrentTimeDilation;
	ActiveSlomoParams = Params;
	SlomoElapsedTime = 0.0;
	bSlomoActive = true;
	bHoldSlomoAtEnd = bHoldAtEnd;

	if (Params.BlendInTime <= 0.0f) // 진입 시간이 없으면 배율 즉시 적용
		CurrentTimeDilation = Params.TargetScale;
}
void FTimeController::StopSlomo()
{
	bSlomoActive = false;
	bHoldSlomoAtEnd = false;
	ActiveSlomoParams = {};
	SlomoElapsedTime = 0.0;

	CurrentTimeDilation = BaseTimeDilation;
	SlomoStartTimeDilation = BaseTimeDilation;
}
bool FTimeController::IsSlomoActive() const
{
	return bSlomoActive;
}
double FTimeController::GetSlomoElapsedTime() const
{
	return SlomoElapsedTime;
}


void FTimeController::Reset()
{
	BaseTimeDilation = 1.0f;
	CurrentTimeDilation = 1.0f; 
	HitStopTimeRemaining = 0.0;
	bSlomoActive = false;
	bHoldSlomoAtEnd = false;
	ActiveSlomoParams = {};
	SlomoElapsedTime = 0.0;
	SlomoStartTimeDilation = 1.0f;
}

void FTimeController::UpdateHitStop(double UnscaledDeltaTime)
{
	if (HitStopTimeRemaining < 0.0)
		StopHitStop();

	HitStopTimeRemaining = std::max(0.0, HitStopTimeRemaining - UnscaledDeltaTime);
}
void FTimeController::UpdateSlomo(double UnscaledDeltaTime)
{
	if (!IsSlomoActive())
		return;

	SlomoElapsedTime += UnscaledDeltaTime;
	const FSlomoParams& P = ActiveSlomoParams;

	// 게임 연출은 목표 배율을 유지할 수 있다. 기존 시퀀서 이벤트는 자동 복구한다.
	if (SlomoElapsedTime >= P.Duration)
	{
		if (bHoldSlomoAtEnd)
		{
			SlomoElapsedTime = P.Duration;
			CurrentTimeDilation = P.TargetScale;
		}
		else
			StopSlomo();
		return;
	}

	const double BlendOutStart = P.Duration - P.BlendOutTime;

	if (P.BlendInTime > 0.0 && SlomoElapsedTime <= P.BlendInTime) // slow 진입점
	{
		const float Alpha = SlomoElapsedTime / P.BlendInTime;
		CurrentTimeDilation = SlomoStartTimeDilation + (P.TargetScale - SlomoStartTimeDilation) * Alpha;
	}
	else if (!bHoldSlomoAtEnd && P.BlendOutTime > 0.0 && SlomoElapsedTime > BlendOutStart) // slow 종료점
	{
		const float Alpha = (SlomoElapsedTime - BlendOutStart) / P.BlendOutTime;
		CurrentTimeDilation = P.TargetScale + (BaseTimeDilation - P.TargetScale) * Alpha;
	}
	else // slow 중
	{
		CurrentTimeDilation = P.TargetScale;
	}
}
