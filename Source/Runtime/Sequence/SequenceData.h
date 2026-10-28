#pragma once
#include <variant>

#include "Engine/EngineBaseTypes.h"
#include "Math/Transform.h"

class UActorComponent;
class USequencePlayer;

// 보간 종류
enum ESequenceInterpolation
{
	Constant,
	Linear,
	Count
};

// 키프레임에서 사용할 Value 종류, Evaluate를 하나로 통합하기 위함
// 이벤트 결과는 추후에 필요하면 추가, 현재는 필요 없을 것으로 추정
using FKeyFrameValueType = std::variant<
	std::monostate, // 등록되지 않음
	float,
	FTransform,
>;

// Sequence : 트랙 및 이밴트 트랙 판단용 unscaleddelattime 저장 //
struct FSequence
{
	TArray<FSequenceTrack*> Sequence;
	float UnscaledDeltatTime = 0.0f;
};
///////////////////////////////////////////////////////////

// Track : 트랙에 사용할 키프레임 종류별로 트랙 구분 //
using FSequenceTrack = std::variant<
	std::monostate, // 등록되지 않음
	FFloatTrack,
	FTransformTrack,
	FEventTrack
>;

template<typename TTrack>
struct TSequenceTrack
{
	USequencePlayer* Sequence;
	UActorComponent* Binding;
	TArray<TTrack*> Track;
};

using FFloatTrack = TSequenceTrack<FFloatKeyFrame>;
using FTransformTrack = TSequenceTrack<FTransformKeyFrame>;
using FEventTrack = TSequenceTrack<FEventKeyFrame>;
///////////////////////////////////////////////////////////

// KeyFrame : 다양한 종류의 키프레임을 템플릿으로 관리
template<typename TFrame>
struct TTrackKeyFrame
{
	double Time = 0.0f;
	TFrame Value{};

	ESequenceInterpolation Interpolation = ESequenceInterpolation::Linear;
};

using FFloatKeyFrame = TTrackKeyFrame<float>;
using FTransformKeyFrame = TTrackKeyFrame<FTransform>;

// 어떤 Time에 Duration동안 이밴트를 실행할 것인가?
struct FEventKeyFrame
{
	double Time = 0.0f;
	float Duration = 0.1f;
};
///////////////////////////////////////////////////////////