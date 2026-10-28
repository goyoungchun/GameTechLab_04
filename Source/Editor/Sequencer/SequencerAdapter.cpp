#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerAdapter.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cfloat>

namespace
{
	enum class EKeyShape : uint8
	{
		Diamond,
		Triangle,
		Square
	};

	EKeyShape GetKeyShape(const FSequenceTrack& Track, int KeyIndex)
	{
		if (std::holds_alternative<FEventTrack>(Track))
		{
			return EKeyShape::Diamond;
		}
		return FSequencerAdapter::GetKeyInterpolation(Track, KeyIndex) == ESequenceInterpolation::Constant
			? EKeyShape::Square
			: EKeyShape::Triangle;
	}

	float GetKeyHitDistance(EKeyShape Shape, float DeltaX, float DeltaY)
	{
		if (Shape == EKeyShape::Diamond)
		{
			return fabsf(DeltaX) + fabsf(DeltaY);
		}
		return std::max(fabsf(DeltaX), fabsf(DeltaY));
	}

	void DrawKeyShape(ImDrawList* DrawList, EKeyShape Shape, const ImVec2& Center, float Radius, ImU32 Color)
	{
		switch (Shape)
		{
		case EKeyShape::Square:
			DrawList->AddRectFilled(ImVec2(Center.x - Radius, Center.y - Radius), ImVec2(Center.x + Radius, Center.y + Radius), Color);
			break;
		case EKeyShape::Triangle:
			DrawList->AddTriangleFilled(ImVec2(Center.x, Center.y - Radius), ImVec2(Center.x + Radius, Center.y + Radius), ImVec2(Center.x - Radius, Center.y + Radius), Color);
			break;
		default:
			DrawList->AddQuadFilled(ImVec2(Center.x, Center.y - Radius), ImVec2(Center.x + Radius, Center.y), ImVec2(Center.x, Center.y + Radius), ImVec2(Center.x - Radius, Center.y), Color);
			break;
		}
	}
}


FSequencerAdapter::~FSequencerAdapter()
{
}

//ImSequencer가 요구하는 재정의 함수
int FSequencerAdapter::GetFrameMin() const
{
	return 0;
}

int FSequencerAdapter::GetFrameMax() const
{
	if (!Sequence)
	{
		return 0;
	}

	return TimeToFrame(Sequence->Length);
}

int FSequencerAdapter::GetItemCount() const
{
	if (!Sequence)
	{
		return 0;
	}

	return Sequence->Tracks.Num();
}

const char* FSequencerAdapter::GetItemLabel(int Index) const
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index))
	{
		return "";
	}

	return GetTrackName(Sequence->Tracks[Index]);
}

void FSequencerAdapter::Get(int Index, int** Start, int** End, int* Type, unsigned int* Color)
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index) || !FrameRangeCache.IsValidIndex(Index))
	{
		return;
	}

	const FSequenceTrack& Track = Sequence->Tracks[Index];
	FTrackFrameRange& Range = FrameRangeCache[Index];
	Range.Start = GetFrameMin();
	Range.End = GetFrameMax();

	if (Start != nullptr)
	{
		*Start = &Range.Start;
	}
	if (End != nullptr)
	{
		*End = &Range.End;
	}
	if (Type != nullptr)
	{
		*Type = static_cast<int>(Track.index());
	}
	if (Color != nullptr)
	{
		*Color = GetTrackColor(Track);
	}
}

void FSequencerAdapter::CustomDrawCompact(int Index, ImDrawList* DrawList, const ImRect& Rect, const ImRect& ClipRect)
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index))
	{
		return;
	}

	const FSequenceTrack& Track = Sequence->Tracks[Index];
	const int KeyCount = GetKeyCount(Track);
	const float FramePixelWidth = Rect.GetWidth() / (GetFrameMax() - GetFrameMin() + 1);
	const float CenterY = (Rect.Max.y + Rect.Min.y) / 2;
	const float KeyRadius = (Rect.Max.y - Rect.Min.y) / 2 * 0.8f;
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	const ImRect RowRect(ImVec2(ClipRect.Min.x - LegendWidth, ClipRect.Min.y), ClipRect.Max);
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)
		&& RowRect.Contains(MousePos))
	{
		ClickedTrackIndex = Index;
		ClickedKeyIndex = -1;

		if (ClipRect.Contains(MousePos))
		{
			const float HitRadius = KeyRadius + KeyHitPadding;
			float ClosestDistance = FLT_MAX;
			for (int KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++)
			{
				const float KeyX = Rect.Min.x + (TimeToFrame(GetKeyTime(Track, KeyIndex)) - GetFrameMin()) * FramePixelWidth;
				const float Distance = GetKeyHitDistance(GetKeyShape(Track, KeyIndex), MousePos.x - KeyX, MousePos.y - CenterY);
				if (Distance <= HitRadius && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					ClickedKeyIndex = KeyIndex;
				}
			}
		}
	}

	DrawList->PushClipRect(ClipRect.Min, ClipRect.Max, true);

	for (int KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++)
	{
		const float KeyX = Rect.Min.x + (TimeToFrame(GetKeyTime(Track, KeyIndex)) - GetFrameMin()) * FramePixelWidth;
		const bool bSelected = Index == SelectedTrackIndex && KeyIndex == SelectedKeyIndex;
		const float DrawRadius = bSelected ? KeyRadius * SelectedKeyScale : KeyRadius;
		const ImU32 Color = bSelected ? SelectedKeyColor : KeyColor;

		DrawKeyShape(DrawList, GetKeyShape(Track, KeyIndex), ImVec2(KeyX, CenterY), DrawRadius, Color);
	}

	DrawList->PopClipRect();
}

bool FSequencerAdapter::ConsumeIndex(int& OutTrack, int& OutKey)
{
	if (ClickedTrackIndex == -1)
	{
		return false;
	}

	OutTrack = ClickedTrackIndex;
	OutKey = ClickedKeyIndex;
	ClickedTrackIndex = -1;
	ClickedKeyIndex = -1;
	return true;
}

void FSequencerAdapter::SetSelection(int InTrack, int InKey)
{
	SelectedTrackIndex = InTrack;
	SelectedKeyIndex = InKey;
}

FSequence* FSequencerAdapter::GetSequence() const
{
	return Sequence;
}

void FSequencerAdapter::SetSequence(FSequence* InSequence)
{
	Sequence = InSequence;
	SyncCache();
}

void FSequencerAdapter::SyncCache()
{
	if (!Sequence)
	{
		FrameRangeCache.Reset();
		return;
	}

	FrameRangeCache.SetNum(Sequence->Tracks.Num());
}

int FSequencerAdapter::TimeToFrame(double Time) const
{
	if (!Sequence)
	{
		return 0;
	}

	return static_cast<int>(std::round(Time * Sequence->FrameRate));
}

double FSequencerAdapter::FrameToTime(int Frame) const
{
	if (!Sequence || Sequence->FrameRate <= 0.0f)
	{
		return 0.0;
	}

	return static_cast<double>(Frame) / Sequence->FrameRate;
}

const char* FSequencerAdapter::GetTrackName(const FSequenceTrack& Track)
{
	return std::visit([](const auto& Typed) -> const char*
	{
		if constexpr (std::is_same_v<std::decay_t<decltype(Typed)>, std::monostate>)
		{
			return "";
		}
		else
		{
			return Typed.Name.c_str();
		}
	}, Track);
}

int FSequencerAdapter::GetKeyCount(const FSequenceTrack& Track)
{
	return std::visit([](const auto& Typed) -> int
	{
		if constexpr (std::is_same_v<std::decay_t<decltype(Typed)>, std::monostate>)
		{
			return 0;
		}
		else
		{
			return Typed.KeyFrames.Num();
		}
	}, Track);
}

double FSequencerAdapter::GetKeyTime(const FSequenceTrack& Track, int KeyIndex)
{
	return std::visit([KeyIndex](const auto& Typed) -> double
	{
		if constexpr (std::is_same_v<std::decay_t<decltype(Typed)>, std::monostate>)
		{
			return 0.0;
		}
		else
		{
			if (!Typed.KeyFrames.IsValidIndex(KeyIndex))
			{
				return 0.0;
			}
			return Typed.KeyFrames[KeyIndex].Time;
		}
	}, Track);
}

ESequenceInterpolation FSequencerAdapter::GetKeyInterpolation(const FSequenceTrack& Track, int KeyIndex)
{
	return std::visit([KeyIndex](const auto& Typed) -> ESequenceInterpolation
	{
		using TTrack = std::decay_t<decltype(Typed)>;
		if constexpr (std::is_same_v<TTrack, FFloatTrack> || std::is_same_v<TTrack, FTransformTrack>)
		{
			if (Typed.KeyFrames.IsValidIndex(KeyIndex))
			{
				return Typed.KeyFrames[KeyIndex].Interpolation;
			}
		}
		return ESequenceInterpolation::Linear;
	}, Track);
}

unsigned int FSequencerAdapter::GetTrackColor(const FSequenceTrack& Track)
{
	if (std::holds_alternative<FTransformTrack>(Track))
	{
		return 0xFFAA8080;
	}
	if (std::holds_alternative<FFloatTrack>(Track))
	{
		return 0xFF80AA80;
	}
	if (std::holds_alternative<FEventTrack>(Track))
	{
		return 0xFF80AAAA;
	}
	return 0xFF808080;
}
