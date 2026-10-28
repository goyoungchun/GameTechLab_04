#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerPanel.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "UObject/UObjectIterator.h"


FSequencerPanel::~FSequencerPanel()
{
}

bool FSequencerPanel::Init()
{
	LoadSettings();
	BuildDummySequence();
	Adapter.SetSequence(&DummySequence);
	return true;
}

void FSequencerPanel::Tick(float DeltaTime)
{
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

	ImGui::Button("Play");
	ImGui::SameLine();
	ImGui::Button("Stop");

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	ImGui::Button("Key from View");

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

	const int PreviousFrame = CurrentFrame;
	ImSequencer::Sequencer(&Adapter, &CurrentFrame, 
		&bExpanded, &SelectedTrackIndex, &FirstVisibleFrame,
		ImSequencer::SEQUENCER_CHANGE_FRAME);

	const int ClickedTrackIndex = Adapter.ConsumeClickedTrackIndex();
	if (ClickedTrackIndex != -1)
	{
		SelectedTrackIndex = ClickedTrackIndex;
	}

	if (CurrentFrame != PreviousFrame)
	{
		CurrentTime = Adapter.FrameToTime(CurrentFrame);
	}
}

void FSequencerPanel::DrawTrackBinding()
{
	if (SelectedTrackIndex == -1 || !DummySequence.Tracks.IsValidIndex(SelectedTrackIndex))
	{
		ImGui::TextDisabled("트랙을 선택해주세요");
		return;
	}

	FDummyTrack& Track = DummySequence.Tracks[SelectedTrackIndex];
	if (Track.Type != EDummyTrackType::Transform)
	{
		ImGui::TextDisabled("바인딩은 Transform만 가능합니다");
		return;
	}

	if (!World)
	{
		ImGui::TextDisabled("월드가 없습니다");
		return;
	}

	TArray<ACameraActor*> Cameras;
	for (TObjectIterator<ACameraActor> It; It; ++It)
	{
		ACameraActor* Camera = *It;
		if (Camera->GetWorld() == World)
		{
			Cameras.Add(Camera);
		}
	}

	if (Track.BoundActor)
	{
		bool bBoundActorAlive = false;
		for (ACameraActor* Camera : Cameras)
		{
			if (Camera == Track.BoundActor)
			{
				bBoundActorAlive = true;
				break;
			}
		}
		if (!bBoundActorAlive)
		{
			Track.BoundActor = nullptr;
		}
	}

	const FString PreviewName = Track.BoundActor ? Track.BoundActor->GetName() : FString("None");

	ImGui::TextUnformatted(Track.Name.c_str());
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##Binding", PreviewName.c_str()))
	{
		if (ImGui::Selectable("None", Track.BoundActor == nullptr))
		{
			Track.BoundActor = nullptr;
		}

		for (ACameraActor* Camera : Cameras)
		{
			const bool bSelected = Track.BoundActor == Camera;
			ImGui::PushID(Camera);
			if (ImGui::Selectable(Camera->GetName().c_str(), bSelected))
			{
				Track.BoundActor = Camera;
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

void FSequencerPanel::BuildDummySequence()
{
	DummySequence = FDummySequence();
	DummySequence.Length = 5.0f;
	DummySequence.FrameRate = 30.0f;

	FDummyTrack CameraTrack;
	CameraTrack.Name = "Camera Transform";
	CameraTrack.Type = EDummyTrackType::Transform;
	for (float Time : { 0.0f, 2.0f, 4.0f })
	{
		FDummyKey Key;
		Key.Time = Time;
		Key.Location = FVector(Time * 100.0f, 0.0f, 100.0f);
		CameraTrack.Keys.Add(Key);
	}
	DummySequence.Tracks.Add(CameraTrack);

	FDummyTrack FovTrack;
	FovTrack.Name = "FOV";
	FovTrack.Type = EDummyTrackType::Float;
	for (float Time : { 1.0f, 3.0f })
	{
		FDummyKey Key;
		Key.Time = Time;
		Key.FloatValue = Time < 2.0f ? 90.0f : 60.0f;
		FovTrack.Keys.Add(Key);
	}
	DummySequence.Tracks.Add(FovTrack);

	FDummyTrack FadeTrack;
	FadeTrack.Name = "Fade";
	FadeTrack.Type = EDummyTrackType::Float;
	for (float Time : { 4.0f, 5.0f })
	{
		FDummyKey Key;
		Key.Time = Time;
		Key.FloatValue = Time < 4.5f ? 0.0f : 1.0f;
		FadeTrack.Keys.Add(Key);
	}
	DummySequence.Tracks.Add(FadeTrack);
}

bool FSequencerPanel::SaveSettings() const
{

	return true;
}

bool FSequencerPanel::LoadSettings()
{
	
	return true;
}