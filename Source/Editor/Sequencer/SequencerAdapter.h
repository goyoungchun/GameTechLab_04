#pragma once

#include "Container/Array.h"
#include "ImSequencer.h"
#include "Editor/Sequencer/SequencerDummyData.h"

struct FTrackFrameRange
{
	int Start = 0;
	int End = 0;
};

class FSequencerAdapter : public ImSequencer::SequenceInterface
{
public:
	FSequencerAdapter() = default;
	~FSequencerAdapter();

	//ImSequencer가 요구하는 재정의 함수
	int GetFrameMin() const override;
	int GetFrameMax() const override;
	int GetItemCount() const override;
	const char* GetItemLabel(int Index) const override;
	void Get(int Index, int** Start, int** End, int* Type, unsigned int* Color) override;
	void CustomDrawCompact(int Index, ImDrawList* DrawList, const ImRect& Rect, const ImRect& ClipRect) override;



	FDummySequence* GetSequence() const;
	void SetSequence(FDummySequence* InSequence);
	void SyncCache();
	int TimeToFrame(float Time) const;
	float FrameToTime(int Frame) const;
	bool ConsumeIndex(int& OutTrack, int& OutKey);
	void SetSelection(int InTrack, int InKey);


private:
	static unsigned int GetTrackColor(EDummyTrackType Type);

	static constexpr float LegendWidth = 200.0f;
	static constexpr float KeyHitPadding = 3.0f;
	static constexpr float SelectedKeyScale = 1.8f;
	static constexpr unsigned int KeyColor = 0xFFFFFFFF;
	static constexpr unsigned int SelectedKeyColor = 0xFF00A5FF;

	FDummySequence* Sequence = nullptr;
	TArray<FTrackFrameRange> FrameRangeCache;
	int ClickedTrackIndex = -1;
	int ClickedKeyIndex = -1;
	int SelectedTrackIndex = -1;
	int SelectedKeyIndex = -1;

};
