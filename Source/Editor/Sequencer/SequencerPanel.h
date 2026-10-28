#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Sequencer/SequencerAdapter.h"

class UWorld;
class FViewportsPanel;

// 임시용
struct FSequencerViewInfo
{
	FVector Location;
	FRotator Rotation;
	float FOV = 0.0f;
};

class FSequencerPanel : public IEditorPanel
{
public:
	FSequencerPanel() = default;
	~FSequencerPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Sequencer"; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetViewportsPanel(FViewportsPanel* InPanel) { ViewportsPanel = InPanel; }

private:
	bool LoadSettings();
	bool SaveSettings() const;

	void DrawToolBar();
	void DrawTimeline();
	void DrawTrackBinding();
	void BuildSampleSequence();
	bool GetCurrentViewPOV(FSequencerViewInfo& OutView) const;
	int32 AddTransformKey(FTransformTrack& InTrack, double InTime, const FTransform& InValue);
	bool CanAddTransformKey() const;
	bool CanAddKey() const;
	void ValidateKeySelection();

	UWorld* World = nullptr;

	// 시간은 초 단위, 프레임은 표시할 때 변환
	float CurrentTime = 0.0f;

	FSequence EditingSequence;
	FSequencerAdapter Adapter;
	FViewportsPanel* ViewportsPanel = nullptr;

	int CurrentFrame = 0;
	bool bExpanded = true;
	bool bIsPlaying = false;
	int SelectedTrackIndex = -1;
	int SelectedKeyIndex = -1;
	int FirstVisibleFrame = 0;
};
