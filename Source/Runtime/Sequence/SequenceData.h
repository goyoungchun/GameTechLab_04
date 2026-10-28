#pragma once

#include <cmath>
#include <algorithm>
#include <type_traits>
#include <variant>

#include "Container/Array.h"
#include "Core/EngineString.h"
#include "Math/Transform.h"

class UActorComponent;

enum class ESequenceEventType : uint8
{
	None,
	HitStop,
	Slomo,
	CameraShake,
	CameraFade,
	StopCameraShake,
	StopCameraFade,
	Letterbox,
	Count
};

// 적용할 속성. 자료형은 GetSequencePropertyDataType에서 구분한다.
// Transform은 첫 구현에서 월드 좌표계를 사용한다.
enum class ESequenceValueType : uint8
{
	None,
	FOV,
	Transform,
	OrthoWidth,
	LightIntensity,
	Count
};

enum class ESequenceDataType : uint8
{
	None,
	Float,
	Transform
};

// 새 속성을 추가하면 필요한 자료형을 이곳에 등록한다.
constexpr ESequenceDataType GetSequencePropertyDataType(ESequenceValueType ValueType)
{
	switch (ValueType)
	{
	case ESequenceValueType::FOV:
	case ESequenceValueType::OrthoWidth:
	case ESequenceValueType::LightIntensity:
		return ESequenceDataType::Float;
	case ESequenceValueType::Transform:
		return ESequenceDataType::Transform;
	default:
		return ESequenceDataType::None;
	}
}

enum class ESequenceInterpolation : uint8
{
	Constant,
	Linear,
	Bezier,
	Count
};

// X는 정규화된 시간, Y는 보간 비율. 기본 제어점은 선형 곡선이다.
struct Bezier
{
	float X1 = 0.0f;
	float Y1 = 0.0f;
	float X2 = 1.0f;
	float Y2 = 1.0f;

	float Evaluate(float Alpha) const
	{
		if (!std::isfinite(Alpha)) return 0.0f;
		Alpha = std::clamp(Alpha, 0.0f, 1.0f);
		if (!IsValid() || Alpha == 0.0f || Alpha == 1.0f) return Alpha;
		auto Sample = [](float T, float A, float B)
		{
			const float S = 1.0f - T;
			return 3.0f*S*S*T*A + 3.0f*S*T*T*B + T*T*T;
		};
		// 시간은 곡선의 X 축이다. X(t)=Alpha를 풀고 같은 t의 Y를 평가한다.
		float Low = 0.0f, High = 1.0f;
		for (int Iteration = 0; Iteration < 24; ++Iteration)
		{
			const float Mid = (Low + High) * 0.5f;
			if (Sample(Mid, X1, X2) < Alpha) Low = Mid;
			else High = Mid;
		}
		return Sample((Low + High) * 0.5f, Y1, Y2);
	}

	bool IsValid() const
	{
		return std::isfinite(X1) && std::isfinite(Y1)
			&& std::isfinite(X2) && std::isfinite(Y2)
			&& X1 >= 0.0f && X1 <= 1.0f
			&& X2 >= 0.0f && X2 <= 1.0f;
	}
};

struct FHitStopParams
{
	float Duration = 0.1f; // 시간 배율 적용 전 초 단위

	bool IsValid() const
	{
		return std::isfinite(Duration) && Duration >= 0.0f;
	}
};

struct FSlomoParams
{
	float TargetScale = 0.5f;
	float Duration = 1.0f; // 진입·복귀를 포함한 전체 시간, Unscaled 기준
	float BlendInTime = 0.1f;
	float BlendOutTime = 0.1f;
	Bezier Ease;

	bool IsValid() const
	{
		return std::isfinite(TargetScale) && TargetScale >= 0.0f
			&& std::isfinite(Duration) && Duration >= 0.0f
			&& std::isfinite(BlendInTime) && BlendInTime >= 0.0f
			&& std::isfinite(BlendOutTime) && BlendOutTime >= 0.0f
			&& static_cast<double>(BlendInTime) + BlendOutTime
				<= static_cast<double>(Duration) + 1.0e-6
			&& Ease.IsValid();
	}
};

// 카메라 흔들림 파형
enum class ECameraShakeType : uint8
{
	Sine,	// 규칙적인 흔들림 (진동, 반동)
	Perlin,	// 불규칙한 흔들림 (폭발, 피격)
};

struct FCameraShakeParams
{
	ECameraShakeType Type = ECameraShakeType::Perlin;
	float Duration = 0.3f;
	float Strength = 1.0f;
	float Frequency = 20.f;
	Bezier Ease; // 카메라 효과가 경과 시간에 따른 강도 감쇠에 사용

	bool IsValid() const
	{
		return 
			std::isfinite(Duration) && Duration >= 0.0f
			&& std::isfinite(Strength) && Strength >= 0.0f
			&& std::isfinite(Frequency) && Frequency >= 0.0f
			&& Ease.IsValid();
	}
};

struct FCameraFadeParams
{
	float FromAlpha = 0.0f;
	float ToAlpha = 1.0f;
	float Duration = 0.3f;
	FVector Color = FVector::ZeroVector;
	bool bHoldWhenFinished = false;
	Bezier Ease;
	bool IsValid() const
	{
		return std::isfinite(FromAlpha) && FromAlpha >= 0 && FromAlpha <= 1
			&& std::isfinite(ToAlpha) && ToAlpha >= 0 && ToAlpha <= 1
			&& std::isfinite(Duration) && Duration >= 0
			&& std::isfinite(Color.X) && std::isfinite(Color.Y) && std::isfinite(Color.Z) && Ease.IsValid();
	}
};

struct FLetterboxParams
{
	bool bEnable = true;
	float Ratio = 0.12f;
	bool IsValid() const { return std::isfinite(Ratio) && Ratio >= 0 && Ratio <= 0.5f; }
};

using FSequenceEventParams = std::variant<
	std::monostate,
	FHitStopParams,
	FSlomoParams,
	FCameraShakeParams,
	FCameraFadeParams,
	FLetterboxParams
>;

// None·Count와 종류가 맞지 않는 Params는 실행할 수 없다.
inline bool IsValidSequenceEventParams(
	ESequenceEventType EventType, const FSequenceEventParams& Params)
{
	switch (EventType)
	{
	case ESequenceEventType::CameraFade:
		if (const auto* Value = std::get_if<FCameraFadeParams>(&Params)) return Value->IsValid();
		break;
	case ESequenceEventType::Letterbox:
		if (const auto* Value = std::get_if<FLetterboxParams>(&Params)) return Value->IsValid();
		break;
	case ESequenceEventType::StopCameraShake:
	case ESequenceEventType::StopCameraFade:
		return std::holds_alternative<std::monostate>(Params);
	case ESequenceEventType::HitStop:
		if (const auto* Value = std::get_if<FHitStopParams>(&Params))
			return Value->IsValid();
		break;
	case ESequenceEventType::Slomo:
		if (const auto* Value = std::get_if<FSlomoParams>(&Params))
			return Value->IsValid();
		break;
	case ESequenceEventType::CameraShake:
		if (const auto* Value = std::get_if<FCameraShakeParams>(&Params))
			return Value->IsValid();
		break;
	default:
		break;
	}
	return false;
}

template<typename TFrame>
struct TTrackKeyFrame
{
	double Time = 0.0;
	TFrame Value{};
	Bezier Ease; // 이 키에서 다음 키까지의 보간 비율을 조절
	ESequenceInterpolation Interpolation = ESequenceInterpolation::Linear;
};

using FFloatKeyFrame = TTrackKeyFrame<float>;
using FTransformKeyFrame = TTrackKeyFrame<FTransform>;

// 발동 시점만 저장한다. 효과의 지속 시간과 곡선은 Params에 들어간다.
struct FEventKeyFrame
{
	double Time = 0.0;
	ESequenceEventType EventType = ESequenceEventType::None;
	FSequenceEventParams Params;

	bool IsValid() const
	{
		return std::isfinite(Time) && Time >= 0.0
			&& IsValidSequenceEventParams(EventType, Params);
	}
};

struct FSequenceEventRequest
{
	double Time = 0.0; // 여러 트랙의 요청을 시간순으로 정렬할 때 사용
	// 비소유 참조. World 단위 이벤트는 nullptr을 허용하고 Player의 World를 사용한다.
	UActorComponent* Binding = nullptr;
	ESequenceEventType EventType = ESequenceEventType::None;
	FSequenceEventParams Params;

	static FSequenceEventRequest FromKeyFrame(
		const FEventKeyFrame& Key, UActorComponent* InBinding = nullptr)
	{
		return { Key.Time, InBinding, Key.EventType, Key.Params };
	}

	bool IsValid() const
	{
		return std::isfinite(Time) && Time >= 0.0
			&& IsValidSequenceEventParams(EventType, Params);
	}
};

// 값 평가 결과와 이벤트 요청 목록은 별도로 반환한다.
using FKeyFrameValueType = std::variant<std::monostate, float, FTransform>;

template<typename TKeyFrame>
struct TSequenceTrack
{
	FString Name;
	UActorComponent* Binding = nullptr; // 외부에서 연결하는 비소유 런타임 참조
	ESequenceValueType ValueType = ESequenceValueType::None;
	TArray<TKeyFrame> KeyFrames; // 트랙이 키 데이터를 값으로 소유

	bool HasCompatibleValueType() const
	{
		if constexpr (std::is_same_v<TKeyFrame, FFloatKeyFrame>)
			return GetSequencePropertyDataType(ValueType) == ESequenceDataType::Float;
		else if constexpr (std::is_same_v<TKeyFrame, FTransformKeyFrame>)
			return GetSequencePropertyDataType(ValueType) == ESequenceDataType::Transform;
		else if constexpr (std::is_same_v<TKeyFrame, FEventKeyFrame>)
			return ValueType == ESequenceValueType::None;
		else
			return false;
	}
};

using FFloatTrack = TSequenceTrack<FFloatKeyFrame>;
using FTransformTrack = TSequenceTrack<FTransformKeyFrame>;
using FEventTrack = TSequenceTrack<FEventKeyFrame>;

using FSequenceTrack = std::variant<
	std::monostate,
	FFloatTrack,
	FTransformTrack,
	FEventTrack
>;

// 편집·저장할 데이터. 현재 재생 위치와 프레임 DeltaTime은 Player가 관리한다.
// 파일 저장 시 Binding 포인터는 제외하고 대상 식별 정보를 별도로 직렬화해야 한다.
struct FSequence
{
	TArray<FSequenceTrack> Tracks;
	float PlayRate = 1.0f; // 기본 재생 배율. Player 등록 시 복사하며 런타임 변경은 Player에만 적용한다.
	float FrameRate = 30.0f; // 편집기 초 <-> 프레임 표시·스냅 기준
	double Length = 0.0;
};
