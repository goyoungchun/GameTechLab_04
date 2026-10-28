#pragma once

#include "ObjectSystem/Class.h"
#include "SequenceData.h"
#include "ObjectSystem/Object.h"

class UWorld;

enum class ESequencePlaybackState : uint8
{
	Stopped,
	Playing,
	Paused
};

enum class ESequenceClockSource : uint8
{
	GameTime, // 게임 시간
	UnscaledTime // 엔진 시간
};

class USequencePlayer : public UObject
{
	DECLARE_CLASS(USequencePlayer, UObject)
	// TODO : SceneJSON 연결
	REFLECT_START(ClassName)
	REFLECT_END()

public:
	USequencePlayer() = default;
	~USequencePlayer() override = default;

	// 모두 비소유 참조. 데이터·World의 소유자가 Player보다 오래 살아 있어야 한다.
	// 다른 Sequence를 등록하면 정지·초기화하고 기본 배율을 복사한다. 같은 Sequence는 유지한다.
	void SetSequence(const FSequence* InSequence);
	void SetPlaybackWorld(UWorld* InWorld) { PlaybackWorld = InWorld; }

	// 시간 갱신 -> Apply의 트랙 순회 -> Evaluate -> 결과 적용 순서
	// SetSequence로 등록한 시퀀스만 재생한다.
	// Update는 World 시간 배율을 다시 곱하지 않고, 확정된 두 DeltaTime을 받는다.
	void Update(double GameDeltaTime, double UnscaledDeltaTime);

	// 런타임 시퀀스는 시작 시각 0의 이벤트도 한 번 실행할 수 있다.
	void Play(bool bFireInitialEvents = false);
	// 재생 위치를 0으로 돌린다. World에 요청한 시간 효과는 각 지속 시간에 따라 종료된다.
	void Stop();
	void Pause();
	// 기본 Seek는 값만 적용한다. 이벤트 실행은 명시적으로 요청한 경우에만 허용한다.
	void Seek(double Time, bool bFireEvents);
	// Player의 재생 배율만 변경한다. 0을 허용하며 역재생·유효하지 않은 값은 무시한다.
	void SetPlayRate(float InPlayRate);
	void SetClockSource(ESequenceClockSource InClockSource) { ClockSource = InClockSource; }

	const FSequence* GetSequence() const { return Sequence; }
	UWorld* GetPlaybackWorld() const { return PlaybackWorld; }
	ESequencePlaybackState GetPlaybackState() const { return State; }
	ESequenceClockSource GetClockSource() const { return ClockSource; }
	double GetCurrentTime() const { return CurrentTime; }
	double GetPreviousTime() const { return PreviousTime; }
	float GetPlayRate() const { return PlayRate; }

private:
	// 값을 평가하고 부모부터 월드 Transform을 적용한 뒤 전체 이벤트를 시간순으로 실행한다.
	// 재생 시간은 여기서 변경하지 않는다.
	// World 이벤트는 PlaybackWorld, 컴포넌트 이벤트는 Binding을 대상으로 전달한다.
	void Apply(const FSequence* InSequence, bool bFireEvents);

	// Time 시점의 값만 계산한다. 대상 객체를 수정하거나 이벤트를 실행하지 않는다.
	FKeyFrameValueType Evaluate(const FSequenceTrack& Track, double Time) const;
	// 정방향 (PreviousTime, CurrentTime] 구간의 요청을 반환한다. 전체 시간순 정렬은 Apply에서 수행한다.
	// Play의 시작 시점 키와 반복 경계는 호출부에서 별도로 처리한다.
	TArray<FSequenceEventRequest> Evaluate(const FEventTrack& Track, double PreviousTime, double CurrentTime) const;

private:
	const FSequence* Sequence = nullptr;
	UWorld* PlaybackWorld = nullptr;
	ESequencePlaybackState State = ESequencePlaybackState::Stopped;
	ESequenceClockSource ClockSource = ESequenceClockSource::GameTime;
	double PreviousTime = 0.0;
	double CurrentTime = 0.0;
	float PlayRate = 1.0f; // 이 Player의 실제 재생 속도. Sequence의 기본값과 독립적이다.
};
