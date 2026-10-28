#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerAdapter.h"
#include "imgui_internal.h"


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

	float FramePixelWidth = Rect.GetWidth() / (GetFrameMax() - GetFrameMin() + 1);
	float CenterY = (Rect.Max.y + Rect.Min.y) / 2;
	float KeyRadius = (Rect.Max.y - Rect.Min.y) / 2 * 0.48f;

	DrawList->PushClipRect(ClipRect.Min, ClipRect.Max, true);

	auto& Keys = Sequence->Tracks[Index].Keys;

	for (const auto& Key : Keys)
	{
		int KeyFrame = TimeToFrame(Key.Time);
		float KeyX = Rect.Min.x + (KeyFrame - GetFrameMin()) * FramePixelWidth;
		
		ImVec2 Up = ImVec2(KeyX, CenterY - KeyRadius);
		ImVec2 Right = ImVec2(KeyX + KeyRadius, CenterY);
		ImVec2 Down = ImVec2(KeyX, CenterY + KeyRadius);
		ImVec2 Left = ImVec2(KeyX - KeyRadius, CenterY);
		ImU32 KeyColor = 0xFFFFFFFF;


		DrawList->AddQuadFilled(Up, Right, Down, Left, KeyColor);
	}

	DrawList->PopClipRect();
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
