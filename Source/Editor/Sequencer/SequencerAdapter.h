#pragma once

#include "Container/Array.h"
#include "ImSequencer.h"
#include "Sequence/SequenceData.h"

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
	int GetItemTypeCount() const override;
	const char* GetItemTypeName(int TypeIndex) const override;
	void Add(int TypeIndex) override;
	void Del(int Index) override;
	void Duplicate(int Index) override;



	FSequence* GetSequence() const;
	void SetSequence(FSequence* InSequence);
	void SyncCache();
	int TimeToFrame(double Time) const;
	double FrameToTime(int Frame) const;
	bool ConsumeIndex(int& OutTrack, int& OutKey);
	int ConsumeDeletedTrackIndex();
	bool ConsumeDragRequest(int& OutTrack, int& OutKey, int& OutFrame);
	void SetDragKeyIndex(int InKey);
	void SetSelection(int InTrack, int InKey);

	static const char* GetTrackName(const FSequenceTrack& Track);
	static int GetKeyCount(const FSequenceTrack& Track);
	static double GetKeyTime(const FSequenceTrack& Track, int KeyIndex);
	static ESequenceInterpolation GetKeyInterpolation(const FSequenceTrack& Track, int KeyIndex);

private:
	static unsigned int GetTrackColor(const FSequenceTrack& Track);
	FString MakeUniqueTrackName(const FString& BaseName) const;

	static constexpr float KeyHitPadding = 3.0f;
	static constexpr float SelectedKeyScale = 1.2f;
	static constexpr unsigned int KeyColor = 0xFFFFFFFF;
	static constexpr unsigned int SelectedKeyColor = 0xFF00A5FF;

	FSequence* Sequence = nullptr;
	TArray<FTrackFrameRange> FrameRangeCache;
	int ClickedTrackIndex = -1;
	int ClickedKeyIndex = -1;
	int SelectedTrackIndex = -1;
	int SelectedKeyIndex = -1;
	int DeletedTrackIndex = -1;

	int DragTrackIndex = -1;
	int DragKeyIndex = -1;
	int DragStartFrame = 0;
	float DragStartMouseX = 0.0f;
	bool bDragging = false;
	int RequestedDragFrame = -1;

};
