#pragma once

#include <cmath>
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
	Count
};

// X는 정규화된 시간, Y는 보간 비율. 기본 제어점은 선형 곡선이다.
struct Bezier
{
	float X1 = 0.0f;
	float Y1 = 0.0f;
	float X2 = 1.0f;
	float Y2 = 1.0f;

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

struct FCameraShakeParams
{
	float Duration = 0.3f;
	float Strength = 1.0f;
	Bezier Ease; // 카메라 효과가 경과 시간에 따른 강도 감쇠에 사용

	bool IsValid() const
	{
		return std::isfinite(Duration) && Duration >= 0.0f
			&& std::isfinite(Strength) && Strength >= 0.0f
			&& Ease.IsValid();
	}
};

using FSequenceEventParams = std::variant<
	std::monostate,
	FHitStopParams,
	FSlomoParams,
	FCameraShakeParams
>;

// None·Count와 종류가 맞지 않는 Params는 실행할 수 없다.
inline bool IsValidSequenceEventParams(
	ESequenceEventType EventType, const FSequenceEventParams& Params)
{
	switch (EventType)
	{
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
	float PlayRate = 1.0f; // SetPlayRate로 편집한다.
	float FrameRate = 30.0f; // 편집기 초 <-> 프레임 표시·스냅 기준
	double Length = 0.0;
};
