#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerAdapter.h"
#include "imgui_internal.h"
#include <cfloat>


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

	return Sequence->Tracks[Index].Name.c_str();
}

void FSequencerAdapter::Get(int Index, int** Start, int** End, int* Type, unsigned int* Color)
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index) || !FrameRangeCache.IsValidIndex(Index))
	{
		return;
	}

	const FDummyTrack& Track = Sequence->Tracks[Index];
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
		*Type = static_cast<int>(Track.Type);
	}
	if (Color != nullptr)
	{
		*Color = GetTrackColor(Track.Type);
	}
}

void FSequencerAdapter::CustomDrawCompact(int Index, ImDrawList* DrawList, const ImRect& Rect, const ImRect& ClipRect)
{
	if (!Sequence || !Sequence->Tracks.IsValidIndex(Index))
	{
		return;
	}

	const TArray<FDummyKey>& Keys = Sequence->Tracks[Index].Keys;
	const float FramePixelWidth = Rect.GetWidth() / (GetFrameMax() - GetFrameMin() + 1);
	const float CenterY = (Rect.Max.y + Rect.Min.y) / 2;
	const float KeyRadius = (Rect.Max.y - Rect.Min.y) / 2 * 0.48f;
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
			for (int KeyIndex = 0; KeyIndex < Keys.Num(); KeyIndex++)
			{
				const float KeyX = Rect.Min.x + (TimeToFrame(Keys[KeyIndex].Time) - GetFrameMin()) * FramePixelWidth;
				const float Distance = fabsf(MousePos.x - KeyX) + fabsf(MousePos.y - CenterY);
				if (Distance <= HitRadius && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					ClickedKeyIndex = KeyIndex;
				}
			}
		}
	}

	DrawList->PushClipRect(ClipRect.Min, ClipRect.Max, true);

	for (int KeyIndex = 0; KeyIndex < Keys.Num(); KeyIndex++)
	{
		const float KeyX = Rect.Min.x + (TimeToFrame(Keys[KeyIndex].Time) - GetFrameMin()) * FramePixelWidth;
		const bool bSelected = Index == SelectedTrackIndex && KeyIndex == SelectedKeyIndex;
		const float DrawRadius = bSelected ? KeyRadius * SelectedKeyScale : KeyRadius;
		const ImU32 Color = bSelected ? SelectedKeyColor : KeyColor;

		const ImVec2 Up(KeyX, CenterY - DrawRadius);
		const ImVec2 Right(KeyX + DrawRadius, CenterY);
		const ImVec2 Down(KeyX, CenterY + DrawRadius);
		const ImVec2 Left(KeyX - DrawRadius, CenterY);
		DrawList->AddQuadFilled(Up, Right, Down, Left, Color);
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

FDummySequence* FSequencerAdapter::GetSequence() const
{
	return Sequence;
}

void FSequencerAdapter::SetSequence(FDummySequence* InSequence)
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

int FSequencerAdapter::TimeToFrame(float Time) const
{
	if (!Sequence)
	{
		return 0;
	}

	return static_cast<int>(roundf(Time * Sequence->FrameRate));
}

float FSequencerAdapter::FrameToTime(int Frame) const
{
	if (!Sequence || Sequence->FrameRate <= 0.0f)
	{
		return 0.0f;
	}

	return static_cast<float>(Frame) / Sequence->FrameRate;
}

unsigned int FSequencerAdapter::GetTrackColor(EDummyTrackType Type)
{
	switch (Type)
	{
	case EDummyTrackType::Transform:
		return 0xFFAA8080;
	case EDummyTrackType::Float:
		return 0xFF80AA80;
	case EDummyTrackType::CameraCut:
		return 0xFF8080AA;
	case EDummyTrackType::Event:
		return 0xFF80AAAA;
	}
	return 0xFF808080;
}
