#pragma once

#include "Sequence/SequenceData.h"

// UWorld가 값으로 소유한다. 시간 배율 적용 전 DeltaTime으로 효과를 갱신한다.
class FTimeController
{
public:
	void Update(double UnscaledDeltaTime);

	void SetTimeDilation(float InTimeDilation); 
	float GetEffectiveTimeDilation() const; // 실제 적용할 최종 배율
	double GetGameDeltaTime(double UnscaledDeltaTime) const; // 엔진의 원본시간 * 최종 배율

	void RequestHitStop(const FHitStopParams& Params);
	void StopHitStop();
	bool IsHitStopped() const;
	double GetHitStopTimeRemaining() const;

	void RequestSlomo(const FSlomoParams& Params);
	void StopSlomo();
	bool IsSlomoActive() const;
	double GetSlomoElapsedTime() const;

	void Reset();

private:
	void UpdateHitStop(double UnscaledDeltaTime);
	void UpdateSlomo(double UnscaledDeltaTime);

private:
	float BaseTimeDilation = 1.0f; // 효과가 없을 때 사용하는 기본 배율
	float CurrentTimeDilation = 1.0f; // Slomo 반영 배율. Hit Stop 배율은 별도로 합성한다.
	double HitStopTimeRemaining = 0.0;

	bool bSlomoActive = false;
	FSlomoParams ActiveSlomoParams{};
	double SlomoElapsedTime = 0.0;
	float SlomoStartTimeDilation = 1.0f;
};
