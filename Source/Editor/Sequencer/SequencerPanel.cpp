#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerPanel.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "UObject/UObjectIterator.h"
#include "Editor/Viewports/ViewportsPanel.h"


FSequencerPanel::~FSequencerPanel()
{
}

bool FSequencerPanel::Init()
{
	LoadSettings();
	BuildSampleSequence();
	Adapter.SetSequence(&EditingSequence);
	return true;
}

void FSequencerPanel::Tick(float DeltaTime)
{
	if (!bIsPlaying)
	{
		return;
	}

	CurrentTime += DeltaTime;
	if (CurrentTime >= EditingSequence.Length)
	{
		CurrentTime = static_cast<float>(EditingSequence.Length);
		bIsPlaying = false;
	}

	CurrentFrame = Adapter.TimeToFrame(CurrentTime);
}


void FSequencerPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(800, 300), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	ImGui::Begin(GetPanelName());
	//const float FontSize = ImGui::GetFontSize();
	//const ImGuiStyle& Style = ImGui::GetStyle();
	//const float Scale = Style.FontScaleDpi * Style.FontScaleMain;

	DrawToolBar();
	ImGui::Separator();
	DrawTrackBinding();
	ImGui::Separator();
	DrawTimeline();


	ImGui::End();
}

void FSequencerPanel::DrawToolBar()
{
	ImGui::Button("New");
	ImGui::SameLine();
	ImGui::Button("Save");
	ImGui::SameLine();
	ImGui::Button("Load");

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	if (!bIsPlaying)
	{
		if (ImGui::Button("Play"))
		{
			if (CurrentTime >= EditingSequence.Length)
			{
				CurrentTime = 0.0f;
				CurrentFrame = 0;
			}
			bIsPlaying = true;
		}
	}
	else
	{
		if (ImGui::Button("Stop"))
		{
			bIsPlaying = false;
			CurrentTime = 0.0f;
			CurrentFrame = 0;
		}
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	ImGui::BeginDisabled(!CanAddKey());
	if (ImGui::Button("Add Key"))
	{
		FSequencerViewInfo ViewInfo;
		if (GetCurrentViewPOV(ViewInfo))
		{
			if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&EditingSequence.Tracks[SelectedTrackIndex]))
			{
				SelectedKeyIndex = AddTransformKey(*TransformTrack, CurrentTime, FTransform(ViewInfo.Rotation, ViewInfo.Location));
			}
		}
	}
	ImGui::EndDisabled();

	const int32 DisplayFrame = Adapter.TimeToFrame(CurrentTime);
	char TimeText[64];
	snprintf(TimeText, sizeof(TimeText), "%.2fs  %df", CurrentTime, DisplayFrame);

	ImGui::SameLine();
	const float TextWidth = ImGui::CalcTextSize(TimeText).x;
	const float AvailWidth = ImGui::GetContentRegionAvail().x;
	if (AvailWidth > TextWidth)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + AvailWidth - TextWidth);
	}
	ImGui::TextUnformatted(TimeText);
}

void FSequencerPanel::DrawTimeline()
{
	Adapter.SyncCache();
	ValidateKeySelection();
	Adapter.SetSelection(SelectedTrackIndex, SelectedKeyIndex);

	const int PreviousFrame = CurrentFrame;
	ImSequencer::Sequencer(&Adapter, &CurrentFrame, 
		&bExpanded, &SelectedTrackIndex, &FirstVisibleFrame,
		ImSequencer::SEQUENCER_CHANGE_FRAME);

	int ClickedTrackIndex = -1;
	int ClickedKeyIndex = -1;

	if (Adapter.ConsumeIndex(ClickedTrackIndex, ClickedKeyIndex))
	{
		SelectedTrackIndex = ClickedTrackIndex;
		SelectedKeyIndex = ClickedKeyIndex;
	}

	if (CurrentFrame != PreviousFrame)
	{
		CurrentTime = static_cast<float>(Adapter.FrameToTime(CurrentFrame));
	}
}

void FSequencerPanel::DrawTrackBinding()
{
	if (SelectedTrackIndex == -1 || !EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex))
	{
		ImGui::TextDisabled("트랙을 선택해주세요");
		return;
	}

	FTransformTrack* TrackPtr = std::get_if<FTransformTrack>(&EditingSequence.Tracks[SelectedTrackIndex]);
	if (!TrackPtr)
	{
		ImGui::TextDisabled("바인딩은 Transform만 가능합니다");
		return;
	}

	if (!World)
	{
		ImGui::TextDisabled("월드가 없습니다");
		return;
	}

	ACameraActor* MainCamera = World->GetMainCamera();
	TArray<ACameraActor*> Cameras;
	for (TObjectIterator<ACameraActor> It; It; ++It)
	{
		ACameraActor* Camera = *It;
		if (Camera->GetWorld() == World && Camera != MainCamera)
		{
			Cameras.Add(Camera);
		}
	}

	FTransformTrack& Track = *TrackPtr;
	ACameraActor* BoundCamera = nullptr;
	if (Track.Binding)
	{
		for (ACameraActor* Camera : Cameras)
		{
			if (Camera->GetCameraComponent() == Track.Binding)
			{
				BoundCamera = Camera;
				break;
			}
		}
		if (!BoundCamera)
		{
			Track.Binding = nullptr;
		}
	}

	const FString PreviewName = BoundCamera ? BoundCamera->GetName() : FString("None");

	ImGui::TextUnformatted(Track.Name.c_str());
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##Binding", PreviewName.c_str()))
	{
		if (ImGui::Selectable("None", Track.Binding == nullptr))
		{
			Track.Binding = nullptr;
		}

		for (ACameraActor* Camera : Cameras)
		{
			const bool bSelected = BoundCamera == Camera;
			ImGui::PushID(Camera);
			if (ImGui::Selectable(Camera->GetName().c_str(), bSelected))
			{
				Track.Binding = Camera->GetCameraComponent();
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}
}

void FSequencerPanel::ValidateKeySelection()
{
	if (!EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex)
		|| SelectedKeyIndex < 0
		|| SelectedKeyIndex >= FSequencerAdapter::GetKeyCount(EditingSequence.Tracks[SelectedTrackIndex]))
	{
		SelectedKeyIndex = -1;
	}
}

bool FSequencerPanel::CanAddKey() const
{
	return (CanAddTransformKey());
}

bool FSequencerPanel::CanAddTransformKey() const
{
	return (EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex)
		&& std::holds_alternative<FTransformTrack>(EditingSequence.Tracks[SelectedTrackIndex]));
}

int32 FSequencerPanel::AddTransformKey(FTransformTrack& InTrack, double InTime, const FTransform& InValue)
{
	const int32 NewFrame = Adapter.TimeToFrame(InTime);

	for (int32 Index = 0; Index < InTrack.KeyFrames.Num(); Index++)
	{
		FTransformKeyFrame& Key = InTrack.KeyFrames[Index];
		const int32 KeyFrame = Adapter.TimeToFrame(Key.Time);
		if (KeyFrame == NewFrame)
		{
			Key.Value = InValue;
			return Index;
		}
		if (KeyFrame > NewFrame)
		{
			FTransformKeyFrame NewKey;
			NewKey.Time = Adapter.FrameToTime(NewFrame);
			NewKey.Value = InValue;
			InTrack.KeyFrames.Insert(NewKey, Index);
			return Index;
		}
	}

	FTransformKeyFrame NewKey;
	NewKey.Time = Adapter.FrameToTime(NewFrame);
	NewKey.Value = InValue;
	InTrack.KeyFrames.Add(NewKey);
	return (InTrack.KeyFrames.Num() - 1);
}

bool FSequencerPanel::GetCurrentViewPOV(FSequencerViewInfo& OutView) const
{
	if (!ViewportsPanel)
	{
		return (false);
	}
	FEditorViewportClient* ActiveViewportClient = ViewportsPanel->GetViewportClient(ViewportsPanel->GetActiveViewIndex());
	if (!ActiveViewportClient)
	{
		return false;
	}
	OutView.FOV = ActiveViewportClient->GetViewFOV();
	OutView.Location = ActiveViewportClient->GetViewLocation();
	OutView.Rotation = ActiveViewportClient->GetViewRotation();
	return (true);
}

void FSequencerPanel::BuildSampleSequence()
{
	EditingSequence = FSequence();
	EditingSequence.Length = 5.0;
	EditingSequence.FrameRate = 30.0f;

	FTransformTrack CameraTrack;
	CameraTrack.Name = "Camera Transform";
	CameraTrack.ValueType = ESequenceValueType::Transform;
	for (double Time : { 0.0, 2.0, 4.0 })
	{
		FTransformKeyFrame Key;
		Key.Time = Time;
		Key.Value.Location = FVector(static_cast<float>(Time) * 100.0f, 0.0f, 100.0f);
		CameraTrack.KeyFrames.Add(Key);
	}
	EditingSequence.Tracks.Add(CameraTrack);

	FFloatTrack FovTrack;
	FovTrack.Name = "FOV";
	FovTrack.ValueType = ESequenceValueType::FOV;
	for (double Time : { 1.0, 3.0 })
	{
		FFloatKeyFrame Key;
		Key.Time = Time;
		Key.Value = Time < 2.0 ? 90.0f : 60.0f;
		FovTrack.KeyFrames.Add(Key);
	}
	EditingSequence.Tracks.Add(FovTrack);

	FEventTrack EventTrack;
	EventTrack.Name = "Event";
	FEventKeyFrame ShakeKey;
	ShakeKey.Time = 4.0;
	ShakeKey.EventType = ESequenceEventType::CameraShake;
	ShakeKey.Params = FCameraShakeParams();
	EventTrack.KeyFrames.Add(ShakeKey);
	EditingSequence.Tracks.Add(EventTrack);
}

bool FSequencerPanel::SaveSettings() const
{

	return true;
}

bool FSequencerPanel::LoadSettings()
{
	
	return true;
}