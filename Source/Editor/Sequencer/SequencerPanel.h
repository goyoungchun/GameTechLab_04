#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Sequencer/SequencerAdapter.h"
#include "Camera/CameraTypes.h"
#include "Engine/TimeController.h"
#include "Sequence/SequencePlayer.h"
#include "Container/Map.h"
#include <functional>

class UWorld;
class FViewportsPanel;
class USequencePlayer;
class UCameraComponent;
class ACameraActor;

class FSequencerPanel : public IEditorPanel
{
public:
	FSequencerPanel() = default;
	~FSequencerPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Sequencer"; }

	void SetWorld(UWorld* InWorld);
	void SetViewportsPanel(FViewportsPanel* InPanel) { ViewportsPanel = InPanel; }
	bool IsFocused() const { return bWindowFocused; }
	void SetSelectionResetCallback(std::function<void()> InCallback) { OnResetSelection = std::move(InCallback); }

private:
	bool LoadSettings();
	bool SaveSettings() const;

	void DrawToolBar();
	void DrawSequenceSettings();
	void DrawTimeline();
	void DrawTrackBinding();
	void DrawKeyDetails();
	void DrawKeyProperties(FSequenceTrack& Track);
	void DrawEaseCurve(FSequenceTrack& Track);
	int32 MoveKeyToFrame(FSequenceTrack& Track, int32 KeyIndex, int32 NewFrame);
	void DeleteSelectedKey();
	bool HasSelectedKey() const;
	void BuildSampleSequence();
	void BeginPlaybackPreview();
	void EndPlaybackPreview();
	void DrawPlaybackStatus();
	bool GetCurrentViewPOV(FMinimalViewInfo& OutView) const;
	UCameraComponent* FindPreviewCamera() const;
	void CreateCameraForTrack();
	void ApplyPreviewToViewport();
	int32 AddTransformKey(FTransformTrack& InTrack, double InTime, const FTransform& InValue);
	int32 AddFloatKey(FFloatTrack& InTrack, double InTime);
	int32 AddEventKey(FEventTrack& InTrack, double InTime);
	bool CanAddTransformKey() const;
	bool CanAddKey() const;
	void ValidateKeySelection();
	void ValidateBindings();
	void NewSequence();
	void SaveSequence(bool bSaveAs);
	void LoadSequence();
	void DestroySequenceCameras(const TArray<ACameraActor*>& KeepCameras);
	void ResetEditingState();

	UWorld* World = nullptr;

	// 시간은 초 단위, 프레임은 표시할 때 변환
	float CurrentTime = 0.0f;

	FSequence EditingSequence;
	FSequencerAdapter Adapter;
	FViewportsPanel* ViewportsPanel = nullptr;

	int CurrentFrame = 0;
	bool bExpanded = true;
	bool bIsPlaying = false;
	bool bPreviewEnabled = false;
	USequencePlayer* Player = nullptr;
	bool bWindowFocused = false;
	int SelectedTrackIndex = -1;
	int SelectedKeyIndex = -1;
	int FirstVisibleFrame = 0;
	float DetailPanelWidth = 500.0f;
	float LegendWidth = 0.0f;
	int EaseTrackIndex = -1;
	int EaseKeyIndex = -1;
	float EasePresetIndex = 0.0f;
	ESequenceEventType LastEventType = ESequenceEventType::HitStop;
	bool bPlaybackPreviewActive = false;
	TMap<uint32, FTransform> PhysicsPreviewTransforms;
	FTimeController SavedPreviewTimeController;
	double PlaybackElapsedTime = 0.0;
	FString CurrentSequencePath;
	FString SequenceScenePath;
	std::function<void()> OnResetSelection;

	static constexpr float SplitterWidth = 6.0f;
	static constexpr float MinDetailPanelWidth = 220.0f;
	static constexpr float MinTimelineWidth = 300.0f;
	static constexpr float MinSequenceLength = 0.1f;
	static constexpr float MaxSequenceLength = 600.0f;
	static constexpr float MinFrameRate = 1.0f;
	static constexpr float MinLegendWidth = 120.0f;
	static constexpr float MaxLegendWidth = 600.0f;
	static constexpr float MaxFrameRate = 120.0f;
};
