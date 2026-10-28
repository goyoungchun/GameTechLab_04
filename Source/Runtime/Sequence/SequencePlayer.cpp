#include "EnginePCH.h"
#include "SequencePlayer.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

#include <type_traits>
#include <variant>
#include <algorithm>
#include <cmath>
#include <limits>

void USequencePlayer::SetSequence(const FSequence* InSequence)
{
	if (Sequence == InSequence)
		return;

	Sequence = InSequence;
	State = ESequencePlaybackState::Stopped;
	CurrentTime = 0.0;
	PreviousTime = 0.0;
	PlayRate = 1.0f;
	if (Sequence)
		SetPlayRate(Sequence->PlayRate);
}

void USequencePlayer::SetPlayRate(float InPlayRate)
{
	if (!std::isfinite(InPlayRate) || InPlayRate < 0.0f)
		return;

	PlayRate = InPlayRate;
}

void USequencePlayer::Update(double GameDeltaTime, double UnscaledDeltaTime)
{
	if (!Sequence)
		return;

	double SelectedDeltaTime = 0.0f;
	if (ClockSource == ESequenceClockSource::GameTime) SelectedDeltaTime = GameDeltaTime;
	if (ClockSource == ESequenceClockSource::UnscaledTime) SelectedDeltaTime = UnscaledDeltaTime;

	if (State == ESequencePlaybackState::Playing)
	{
		PreviousTime = CurrentTime;
		double CurTime = std::clamp(CurrentTime + SelectedDeltaTime * PlayRate, 0.0, Sequence->Length);
		if (Sequence->Length > 0.0 && CurTime >= Sequence->Length)
		{
			// 끝 키의 중지 이벤트까지 실행한 다음 다음 반복의 시작 키를 적용한다.
			CurrentTime = Sequence->Length;
			Apply(Sequence, true);
			PreviousTime = -std::numeric_limits<double>::epsilon();
			CurrentTime = 0.0;
			Apply(Sequence, true);
			PreviousTime = 0.0;
			return;
		}
		else
		{
			CurrentTime = CurTime;
		}
		Apply(Sequence, true);
	}
}

void USequencePlayer::Play(bool bFireInitialEvents)
{
	if (!Sequence)
		return;

	const bool bStartAtZero = bFireInitialEvents && State == ESequencePlaybackState::Stopped && CurrentTime == 0.0;
	State = ESequencePlaybackState::Playing;
	if (bStartAtZero)
	{
		PreviousTime = -std::numeric_limits<double>::epsilon();
		Apply(Sequence, true);
		PreviousTime = CurrentTime;
	}
}

void USequencePlayer::Stop()
{
	State = ESequencePlaybackState::Stopped;
	CurrentTime = 0.0;
	PreviousTime = 0.0;
	Apply(Sequence, false);
}

void USequencePlayer::Pause()
{
	State = ESequencePlaybackState::Paused;
}

void USequencePlayer::Seek(double Time, bool bFireEvents)
{
	if (!Sequence)
		return;

	const double OldTime = CurrentTime;
	CurrentTime = std::clamp(Time, 0.0, Sequence->Length);

	// 현재 이벤트 평가는 정방향 이동만 지원한다.
	const bool bEvaluateEvents = bFireEvents && CurrentTime > OldTime;
	PreviousTime = bEvaluateEvents ? OldTime : CurrentTime;

	Apply(Sequence, bEvaluateEvents);

	PreviousTime = CurrentTime;
}

void USequencePlayer::Apply(const FSequence* InSequence, bool bFireEvents)
{
	if (!InSequence)
		return;

	struct FPendingTransform
	{
		USceneComponent* Component;
		FTransform WorldTransform;
		uint32 AttachmentDepth;
	};
	TArray<FPendingTransform> PendingTransforms;
	TArray<FSequenceEventRequest> EventRequests;

	for (const FSequenceTrack& Track : InSequence->Tracks)
	{
		std::visit([this, &Track, bFireEvents, &PendingTransforms, &EventRequests](const auto& TypedTrack)
			{
				using TTrack = std::decay_t<decltype(TypedTrack)>; // 타입 자체를 비교하기 위함
				if constexpr (std::is_same_v<TTrack, FFloatTrack> || std::is_same_v<TTrack, FTransformTrack>)
				{
					if (!TypedTrack.HasCompatibleValueType() || !TypedTrack.Binding)
					{
						return;
					}
					const FKeyFrameValueType Result = Evaluate(Track, CurrentTime);

					// 값 종류에 따른 다른 적용
					if (UCameraComponent* Camera = Cast<UCameraComponent>(TypedTrack.Binding))
					{
						if(const float* Value = std::get_if<float>(&Result))
						{
							if (TypedTrack.ValueType == ESequenceValueType::FOV)
							{
								Camera->SetFieldOfView(*Value);
							}
							if (TypedTrack.ValueType == ESequenceValueType::OrthoWidth)
							{
								Camera->SetOrthoWidth(*Value);
							}
						}
					}
					if (USceneComponent* Component = Cast<USceneComponent>(TypedTrack.Binding))
					{
						if (const FTransform* Value = std::get_if<FTransform>(&Result))
						{
							if (TypedTrack.ValueType == ESequenceValueType::Transform)
							{
								uint32 Depth = 0;
								for (const USceneComponent* Parent = Component->GetAttachParent();
									Parent; Parent = Parent->GetAttachParent())
								{
									++Depth;
								}
								PendingTransforms.Add({ Component, *Value, Depth });
							}
						}
					}
				}

				else if constexpr (std::is_same_v<TTrack, FEventTrack>)
				{
					if (!bFireEvents || !TypedTrack.HasCompatibleValueType())
					{
						return;
					}
					EventRequests.Append(Evaluate(TypedTrack, PreviousTime, CurrentTime));

				}
			}, Track);
	}

	// 자식이 먼저 등록되어 있어도 부모의 이번 프레임 변환을 기준으로 상대값을 계산한다.
	std::stable_sort(PendingTransforms.begin(), PendingTransforms.end(),
		[](const FPendingTransform& A, const FPendingTransform& B)
		{
			return A.AttachmentDepth < B.AttachmentDepth;
		});
	for (const FPendingTransform& Pending : PendingTransforms)
	{
		Pending.Component->SetWorldTransform(Pending.WorldTransform);
	}

	// 같은 시각은 기존 트랙 순서와 키 배열 순서를 유지한다.
	std::stable_sort(EventRequests.begin(), EventRequests.end(),
		[](const FSequenceEventRequest& A, const FSequenceEventRequest& B)
		{
			return A.Time < B.Time;
		});
	for (const FSequenceEventRequest& Request : EventRequests)
	{
		switch (Request.EventType)
		{
		case ESequenceEventType::HitStop:
			if (const auto* Params = std::get_if<FHitStopParams>(&Request.Params))
			{
				if (PlaybackWorld)
					PlaybackWorld->GetTimeController().RequestHitStop(*Params);
			}
			break;
		case ESequenceEventType::Slomo:
			if (const auto* Params = std::get_if<FSlomoParams>(&Request.Params))
			{
				if (PlaybackWorld)
					PlaybackWorld->GetTimeController().RequestSlomo(*Params);
			}
			break;
		case ESequenceEventType::CameraShake:
		case ESequenceEventType::CameraFade:
		case ESequenceEventType::StopCameraShake:
		case ESequenceEventType::StopCameraFade:
		case ESequenceEventType::Letterbox:
			if (PlaybackWorld)
				for (uint32 Index = 0; Index < PlaybackWorld->GetPlayerCount(); ++Index)
				{
					APlayerController* PC = PlaybackWorld->GetPlayerController(Index);
					APlayerCameraManager* Camera = PC ? PC->GetCameraManager() : nullptr;
					if (!Camera) continue;
					if (const auto* Shake = std::get_if<FCameraShakeParams>(&Request.Params))
						Camera->StartCameraShake(*Shake);
					else if (const auto* Fade = std::get_if<FCameraFadeParams>(&Request.Params))
						Camera->StartCameraFade(Fade->FromAlpha, Fade->ToAlpha, Fade->Duration, Fade->Color, Fade->bHoldWhenFinished, Fade->Ease);
					else if (const auto* Bars = std::get_if<FLetterboxParams>(&Request.Params))
						Camera->SetLetterbox(Bars->bEnable, Bars->Ratio);
					else if (Request.EventType == ESequenceEventType::StopCameraShake) Camera->StopAllCameraShakes();
					else if (Request.EventType == ESequenceEventType::StopCameraFade) Camera->StopCameraFade();
				}
			break;
		default:
			break;
		}
	}
}

FKeyFrameValueType USequencePlayer::Evaluate(const FSequenceTrack& Track, double Time) const
{
	return std::visit(
		[Time](const auto& TypedTrack) -> FKeyFrameValueType
		{
			using TTrack = std::decay_t<decltype(TypedTrack)>;

			if constexpr (std::is_same_v<TTrack, FFloatTrack>)
			{
				const FFloatKeyFrame* Left = nullptr;
				const FFloatKeyFrame* Right = nullptr;

				for (const FFloatKeyFrame& Key : TypedTrack.KeyFrames)
				{
					if (Key.Time <= Time)
					{
						// 현재 시간 이하인 키 중 가장 늦은 키
						if (!Left || Key.Time > Left->Time)
							Left = &Key;
					}
					else
					{
						// 현재 시간보다 뒤인 키 중 가장 이른 키
						if (!Right || Key.Time < Right->Time)
							Right = &Key;
					}
				}

				// 키가 없음
				if (!Left && !Right)
					return std::monostate{};
				// 첫 키 이전
				if (!Left)
					return Right->Value;
				// 마지막 키 이후 또는 키가 하나뿐인 경우
				if (!Right)
					return Left->Value;

				if (Left->Interpolation == ESequenceInterpolation::Constant)
					return Left->Value;

				if (Left->Interpolation == ESequenceInterpolation::Linear || Left->Interpolation == ESequenceInterpolation::Bezier)
				{
					const float Fraction = static_cast<float>((Time - Left->Time) / (Right->Time - Left->Time));
					const float Alpha = Left->Interpolation == ESequenceInterpolation::Bezier ? Left->Ease.Evaluate(Fraction) : Fraction;
					return Left->Value + (Right->Value - Left->Value) * Alpha;
				}
			}
			else if constexpr (std::is_same_v<TTrack, FTransformTrack>)
			{
				// TypedTrack은 const FTransformTrack&
				const auto& Keys = TypedTrack.KeyFrames;

				const FTransformKeyFrame* Left = nullptr;
				const FTransformKeyFrame* Right = nullptr;

				for (const FTransformKeyFrame& Key : TypedTrack.KeyFrames)
				{
					if (Key.Time <= Time)
					{
						// 현재 시간 이하인 키 중 가장 늦은 키
						if (!Left || Key.Time > Left->Time)
							Left = &Key;
					}
					else
					{
						// 현재 시간보다 뒤인 키 중 가장 이른 키
						if (!Right || Key.Time < Right->Time)
							Right = &Key;
					}
				}

				// 키가 없음
				if (!Left && !Right)
					return std::monostate{};
				// 첫 키 이전
				if (!Left)
					return Right->Value;
				// 마지막 키 이후 또는 키가 하나뿐인 경우
				if (!Right)
					return Left->Value;

				if (Left->Interpolation == ESequenceInterpolation::Constant)
					return Left->Value;

				if (Left->Interpolation == ESequenceInterpolation::Linear || Left->Interpolation == ESequenceInterpolation::Bezier)
				{
					const float Fraction = static_cast<float>((Time - Left->Time) / (Right->Time - Left->Time));
					const float Alpha = Left->Interpolation == ESequenceInterpolation::Bezier ? Left->Ease.Evaluate(Fraction) : Fraction;

					const FTransform& A = Left->Value;
					const FTransform& B = Right->Value;

					FTransform Result;

					Result.Location = A.Location + (B.Location - A.Location) * Alpha;
					Result.Scale = A.Scale + (B.Scale - A.Scale) * Alpha;
					Result.Rotation = FQuat::Slerp(A.GetOrientation(), B.GetOrientation(), Alpha).ToFRotator();

					return Result;
				}
			}

			// monostate 또는 값 평가 대상이 아닌 이벤트 트랙
			return std::monostate{};
		},
		Track);
}

TArray<FSequenceEventRequest> USequencePlayer::Evaluate(const FEventTrack& Track, double PreviousTime, double CurrentTime) const
{
	TArray<FSequenceEventRequest> Result;

	for (const auto& Event : Track.KeyFrames)
	{
		if (Event.IsValid() && PreviousTime < Event.Time && Event.Time <= CurrentTime)
		{
			Result.Add(FSequenceEventRequest::FromKeyFrame(Event, Track.Binding));
		}
	}

	return Result;
}
