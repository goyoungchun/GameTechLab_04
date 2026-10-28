#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "SequencerDummyData.h"
#include "SequencerAdapter.h"

class UWorld;

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

private:
	bool LoadSettings();
	bool SaveSettings() const;

	void DrawToolBar();
	void DrawTimeline();
	void DrawTrackBinding();
	void BuildDummySequence();

	UWorld* World = nullptr;

	// 시간은 초 단위, 프레임은 표시할 때 변환
	float CurrentTime = 0.0f;

	FDummySequence DummySequence;
	FSequencerAdapter Adapter;

	int CurrentFrame = 0;
	bool bExpanded = true;
	int SelectedTrackIndex = -1;
	int FirstVisibleFrame = 0;
};
