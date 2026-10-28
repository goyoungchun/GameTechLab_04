#include "EnginePCH.h"
#include "SequencePlayer.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

#include <type_traits>
#include <variant>
#include <algorithm>

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
		double CurTime = std::clamp(CurrentTime + SelectedDeltaTime * Sequence->PlayRate, 0.0, Sequence->Length);
		if (CurTime >= Sequence->Length)
		{
			CurrentTime = 0.0;
		}
		else
		{
			CurrentTime = CurTime;
		}
		Apply(Sequence, true);
	}
}

void USequencePlayer::Play()
{
	State = ESequencePlaybackState::Playing;
}

void USequencePlayer::Stop()
{
	State = ESequencePlaybackState::Stopped;
	CurrentTime = 0.0;
	PreviousTime = 0.0;
	Apply(Sequence, false);
	PlaybackWorld->GetTimeController().Reset();
}

void USequencePlayer::Pause()
{
	State = ESequencePlaybackState::Paused;
}

void USequencePlayer::Seek(double Time, bool bFireEvents)
{
	CurrentTime = std::clamp(Time, 0.0, Sequence->Length);
	Apply(Sequence, bFireEvents);
}

void USequencePlayer::Apply(const FSequence* InSequence, bool bFireEvents)
{
	if (!InSequence)
		return;

	for (const FSequenceTrack& Track : InSequence->Tracks)
	{
		std::visit([this, &Track, bFireEvents](const auto& TypedTrack)
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
						if (const FTransform* Value = std::get_if<FTransform>(&Result))
						{
							if (TypedTrack.ValueType == ESequenceValueType::Transform)
							{
								Camera->SetTransform(*Value);
							}
						}
					}
					if (USceneComponent* Component = Cast<USceneComponent>(TypedTrack.Binding))
					{
						if (const FTransform* Value = std::get_if<FTransform>(&Result))
						{
							if (TypedTrack.ValueType == ESequenceValueType::Transform)
							{
								Component->SetTransform(*Value);
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

					const auto Requests = Evaluate(TypedTrack, PreviousTime, CurrentTime);

					for (const FSequenceEventRequest& Request : Requests)
					{
						switch (Request.EventType)
						{
						case ESequenceEventType::HitStop:
							if (const auto* Params = std::get_if<FHitStopParams>(&Request.Params))
							{
								if (PlaybackWorld)
								{
									PlaybackWorld->GetTimeController().RequestHitStop(*Params);
								}
							}
							break;

						case ESequenceEventType::Slomo:
							if (const auto* Params = std::get_if<FSlomoParams>(&Request.Params))
							{
								PlaybackWorld->GetTimeController().RequestSlomo(*Params);
							}
							break;

						case ESequenceEventType::CameraShake:
							if (const auto* Params = std::get_if<FCameraShakeParams>(&Request.Params))
							{
								// Camera Shake 처리
							}
							break;

						default:
							break;
						}
					}
				}
			}, Track);
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

				if (Left->Interpolation == ESequenceInterpolation::Linear)
				{
					const float Alpha = static_cast<float>((Time - Left->Time) / (Right->Time - Left->Time));
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

				if (Left->Interpolation == ESequenceInterpolation::Linear)
				{
					const float Alpha = static_cast<float>( (Time - Left->Time) / (Right->Time - Left->Time));

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
		if (PreviousTime <= Event.Time && Event.Time <= CurrentTime)
		{
			Result.Add(FSequenceEventRequest::FromKeyFrame(Event, Track.Binding));
		}
	}

	return Result;
}
