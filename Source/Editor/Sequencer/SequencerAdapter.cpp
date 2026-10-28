#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerAdapter.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cfloat>
#include <iterator>
#include <string>

namespace
{
	const char* TrackTypeNames[] = { "Camera Transform", "FOV", "Event" };

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
	const float KeyRadius = (Rect.Max.y - Rect.Min.y) / 2 * 0.6f;
	const ImVec2 MousePos = ImGui::GetIO().MousePos;

	const ImRect RowRect(ImVec2(ClipRect.Min.x - ImSequencer::GetLegendWidth(), ClipRect.Min.y), ClipRect.Max);
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

	if (ClickedTrackIndex == Index && ClickedKeyIndex != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		DragTrackIndex = Index;
		DragKeyIndex = ClickedKeyIndex;
		DragStartFrame = TimeToFrame(GetKeyTime(Track, ClickedKeyIndex));
		DragStartMouseX = MousePos.x;
		bDragging = false;
	}

	if (DragTrackIndex == Index)
	{
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || DragKeyIndex < 0 || DragKeyIndex >= KeyCount)
		{
			DragTrackIndex = -1;
			DragKeyIndex = -1;
			bDragging = false;
		}
		else
		{
			if (!bDragging && fabsf(MousePos.x - DragStartMouseX) >= ImGui::GetIO().MouseDragThreshold)
			{
				bDragging = true;
			}

			if (bDragging)
			{
				const int MouseFrame = static_cast<int>(std::round((MousePos.x - Rect.Min.x) / FramePixelWidth)) + GetFrameMin();
				const int TargetFrame = std::clamp(MouseFrame, GetFrameMin(), GetFrameMax());
				if (TargetFrame != TimeToFrame(GetKeyTime(Track, DragKeyIndex)))
				{
					RequestedDragFrame = TargetFrame;
				}
				ImGui::SetTooltip("%df -> %df", DragStartFrame, TargetFrame);
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

int FSequencerAdapter::GetItemTypeCount() const
{
	return static_cast<int>(std::size(TrackTypeNames));
}

const char* FSequencerAdapter::GetItemTypeName(int TypeIndex) const
{
	if (TypeIndex < 0 || TypeIndex >= GetItemTypeCount())
	{
		return "";
	}
	return TrackTypeNames[TypeIndex];
}

void FSequencerAdapter::Add(int TypeIndex)
{
	if (!Sequence || TypeIndex < 0 || TypeIndex >= GetItemTypeCount())
	{
		return;
	}

	const FString Name = MakeUniqueTrackName(TrackTypeNames[TypeIndex]);
	switch (TypeIndex)
	{
	case 0:
	{
		FTransformTrack Track;
		Track.Name = Name;
		Track.ValueType = ESequenceValueType::Transform;
		Sequence->Tracks.Add(Track);
		break;
	}
	case 1:
	{
		FFloatTrack Track;
		Track.Name = Name;
		Track.ValueType = ESequenceValueType::FOV;
		Sequence->Tracks.Add(Track);
		break;
	}
	default:
	{
		FEventTrack Track;
		Track.Name = Name;
		Sequence->Tracks.Add(Track);
		break;
	}
	}
}

void FSequencerAdapter::Del(int Index)
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index))
	{
		return;
	}

	Sequence->Tracks.RemoveAt(static_cast<uint32>(Index), 1);
	DeletedTrackIndex = Index;
	DragTrackIndex = -1;
	DragKeyIndex = -1;
	bDragging = false;
}

void FSequencerAdapter::Duplicate(int Index)
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index))
	{
		return;
	}

	FSequenceTrack Copy = Sequence->Tracks[Index];
	const FString Name = MakeUniqueTrackName(FString(GetTrackName(Copy)) + " Copy");
	std::visit([&Name](auto& Typed)
	{
		if constexpr (!std::is_same_v<std::decay_t<decltype(Typed)>, std::monostate>)
		{
			Typed.Name = Name;
			Typed.Binding = nullptr;
		}
	}, Copy);
	Sequence->Tracks.Add(Copy);
}

bool FSequencerAdapter::ConsumeDragRequest(int& OutTrack, int& OutKey, int& OutFrame)
{
	if (RequestedDragFrame == -1 || DragTrackIndex == -1)
	{
		RequestedDragFrame = -1;
		return false;
	}

	OutTrack = DragTrackIndex;
	OutKey = DragKeyIndex;
	OutFrame = RequestedDragFrame;
	RequestedDragFrame = -1;
	return true;
}

void FSequencerAdapter::SetDragKeyIndex(int InKey)
{
	DragKeyIndex = InKey;
}

int FSequencerAdapter::ConsumeDeletedTrackIndex()
{
	const int Result = DeletedTrackIndex;
	DeletedTrackIndex = -1;
	return Result;
}

FString FSequencerAdapter::MakeUniqueTrackName(const FString& BaseName) const
{
	auto IsNameUsed = [this](const FString& Candidate)
	{
		for (const FSequenceTrack& Track : Sequence->Tracks)
		{
			if (Candidate == GetTrackName(Track))
			{
				return true;
			}
		}
		return false;
	};

	if (!IsNameUsed(BaseName))
	{
		return BaseName;
	}

	for (int Number = 2;; Number++)
	{
		const FString Candidate = BaseName + " " + std::to_string(Number);
		if (!IsNameUsed(Candidate))
		{
			return Candidate;
		}
	}
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
