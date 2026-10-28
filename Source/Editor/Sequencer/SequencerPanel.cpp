#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerPanel.h"


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

	if (CurrentFrame != PreviousFrame)
	{
		CurrentTime = Adapter.FrameToTime(CurrentFrame);
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
	FadeTrack.Type = EDummyTrackType::CameraCut;
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