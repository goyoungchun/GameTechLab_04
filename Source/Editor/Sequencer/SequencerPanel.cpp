#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerPanel.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "UObject/UObjectIterator.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "imgui_bezier.h"
#include <algorithm>
#include <cfloat>

namespace
{
	float GetLabelColumnWidth() { return ImGui::GetFontSize() * 3.5f; }

	bool DrawAxisControl(const char* Label, float& Value, float Speed, float ResetValue, const ImVec4& Color, float DragWidth)
	{
		bool bChanged = false;
		const float LineHeight = ImGui::GetFrameHeight();

		ImGui::PushStyleColor(ImGuiCol_Button, Color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(Color.x + 0.1f, Color.y + 0.1f, Color.z + 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(Color.x - 0.1f, Color.y - 0.1f, Color.z - 0.1f, 1.0f));
		if (ImGui::Button(Label, ImVec2(LineHeight, LineHeight)))
		{
			Value = ResetValue;
			bChanged = true;
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushID(Label);
		ImGui::SetNextItemWidth(DragWidth);
		bChanged |= ImGui::DragFloat("##Value", &Value, Speed, 0.0f, 0.0f, "%.2f");
		ImGui::PopID();
		return bChanged;
	}

	bool DrawVector3Values(float* Values, float Speed, float ResetValue)
	{
		bool bChanged = false;
		const float Spacing = 2.0f;
		const float ButtonWidth = ImGui::GetFrameHeight();
		const float AxisWidth = (ImGui::GetContentRegionAvail().x - Spacing * 2.0f) / 3.0f;
		const float DragWidth = std::max(1.0f, AxisWidth - ButtonWidth - Spacing);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Spacing, 0.0f));

		bChanged |= DrawAxisControl("X", Values[0], Speed, ResetValue, ImVec4(0.8f, 0.1f, 0.1f, 1.0f), DragWidth);
		ImGui::SameLine();
		bChanged |= DrawAxisControl("Y", Values[1], Speed, ResetValue, ImVec4(0.1f, 0.8f, 0.1f, 1.0f), DragWidth);
		ImGui::SameLine();
		bChanged |= DrawAxisControl("Z", Values[2], Speed, ResetValue, ImVec4(0.1f, 0.1f, 0.8f, 1.0f), DragWidth);

		ImGui::PopStyleVar();
		return bChanged;
	}

	void BeginPropertyRow(const char* Label)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	const char* GetValueTypeLabel(ESequenceValueType ValueType)
	{
		switch (ValueType)
		{
		case ESequenceValueType::FOV:
			return "FOV";
		case ESequenceValueType::OrthoWidth:
			return "Ortho Width";
		case ESequenceValueType::LightIntensity:
			return "Light Intensity";
		default:
			return "Value";
		}
	}

	const char* GetEventTypeLabel(ESequenceEventType EventType)
	{
		switch (EventType)
		{
		case ESequenceEventType::HitStop:
			return "Hit Stop";
		case ESequenceEventType::Slomo:
			return "Slomo";
		case ESequenceEventType::CameraShake:
			return "Camera Shake";
		default:
			return "None";
		}
	}

	template<typename TKeyFrame>
	int32 MoveKeyInArray(TArray<TKeyFrame>& Keys, int32 KeyIndex, int32 NewFrame, double NewTime, const FSequencerAdapter& Adapter)
	{
		for (int32 Index = 0; Index < Keys.Num(); Index++)
		{
			if (Index != KeyIndex && Adapter.TimeToFrame(Keys[Index].Time) == NewFrame)
			{
				return KeyIndex;
			}
		}

		TKeyFrame MovedKey = Keys[KeyIndex];
		MovedKey.Time = NewTime;
		Keys.RemoveAt(static_cast<uint32>(KeyIndex), 1);

		for (int32 Index = 0; Index < Keys.Num(); Index++)
		{
			if (Adapter.TimeToFrame(Keys[Index].Time) > NewFrame)
			{
				Keys.Insert(MovedKey, static_cast<uint32>(Index));
				return Index;
			}
		}

		Keys.Add(MovedKey);
		return Keys.Num() - 1;
	}
}


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

	DrawToolBar();
	ImGui::Separator();
	DrawTrackBinding();
	ImGui::Separator();

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const float MaxDetailPanelWidth = std::max(MinDetailPanelWidth, Available.x - MinTimelineWidth - SplitterWidth);
	DetailPanelWidth = std::clamp(DetailPanelWidth, MinDetailPanelWidth, MaxDetailPanelWidth);
	const float TimelineWidth = Available.x - DetailPanelWidth - SplitterWidth;

	if (ImGui::BeginChild("##TimelineArea", ImVec2(TimelineWidth, Available.y)))
	{
		DrawTimeline();
	}
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::InvisibleButton("##DetailSplitter", ImVec2(SplitterWidth, Available.y));
	const bool bSplitterActive = ImGui::IsItemActive();
	const bool bSplitterHovered = ImGui::IsItemHovered();
	if (bSplitterActive || bSplitterHovered)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (bSplitterActive)
	{
		DetailPanelWidth -= ImGui::GetIO().MouseDelta.x;
	}
	const ImVec2 SplitterMin = ImGui::GetItemRectMin();
	const ImVec2 SplitterMax = ImGui::GetItemRectMax();
	const float SplitterCenterX = (SplitterMin.x + SplitterMax.x) * 0.5f;
	const ImU32 SplitterColor = ImGui::GetColorU32(bSplitterActive ? ImGuiCol_SeparatorActive
		: bSplitterHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
	ImGui::GetWindowDrawList()->AddLine(ImVec2(SplitterCenterX, SplitterMin.y), ImVec2(SplitterCenterX, SplitterMax.y), SplitterColor, 2.0f);

	ImGui::SameLine(0.0f, 0.0f);

	if (ImGui::BeginChild("##KeyDetails", ImVec2(0.0f, Available.y), ImGuiChildFlags_Borders))
	{
		DrawKeyDetails();
	}
	ImGui::EndChild();


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

void FSequencerPanel::DrawKeyDetails()
{
	if (!EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex)
		|| SelectedKeyIndex < 0
		|| SelectedKeyIndex >= FSequencerAdapter::GetKeyCount(EditingSequence.Tracks[SelectedTrackIndex]))
	{
		ImGui::TextDisabled("키를 선택해주세요");
		return;
	}

	FSequenceTrack& Track = EditingSequence.Tracks[SelectedTrackIndex];
	ImGui::Text("%s / Key %d", FSequencerAdapter::GetTrackName(Track), SelectedKeyIndex);

	if (ImGui::CollapsingHeader("Key", ImGuiTreeNodeFlags_DefaultOpen)
		&& ImGui::BeginTable("##KeyProperties", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
	{
		DrawKeyProperties(Track);
		ImGui::EndTable();
	}

	DrawEaseCurve(EditingSequence.Tracks[SelectedTrackIndex]);
}

void FSequencerPanel::DrawKeyProperties(FSequenceTrack& Track)
{
	ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, GetLabelColumnWidth());
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

	BeginPropertyRow("Frame");
	int KeyFrame = Adapter.TimeToFrame(FSequencerAdapter::GetKeyTime(Track, SelectedKeyIndex));
	if (ImGui::DragInt("##Frame", &KeyFrame, 0.2f, Adapter.GetFrameMin(), Adapter.GetFrameMax()))
	{
		SelectedKeyIndex = MoveKeyToFrame(Track, SelectedKeyIndex, KeyFrame);
	}

	BeginPropertyRow("Time");
	ImGui::TextDisabled("%.2fs", FSequencerAdapter::GetKeyTime(Track, SelectedKeyIndex));

	if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track))
	{
		FTransform& Value = TransformTrack->KeyFrames[SelectedKeyIndex].Value;

		BeginPropertyRow("Location");
		ImGui::PushID("Location");
		float Location[3] = { Value.Location.X, Value.Location.Y, Value.Location.Z };
		if (DrawVector3Values(Location, 1.0f, 0.0f))
		{
			Value.Location = FVector(Location[0], Location[1], Location[2]);
		}
		ImGui::PopID();

		BeginPropertyRow("Rotation");
		ImGui::PushID("Rotation");
		float Rotation[3] = { Value.Rotation.Roll, Value.Rotation.Pitch, Value.Rotation.Yaw };
		if (DrawVector3Values(Rotation, 0.5f, 0.0f))
		{
			Value.Rotation.Roll = Rotation[0];
			Value.Rotation.Pitch = Rotation[1];
			Value.Rotation.Yaw = Rotation[2];
		}
		ImGui::PopID();
	}
	else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
	{
		BeginPropertyRow(GetValueTypeLabel(FloatTrack->ValueType));
		ImGui::DragFloat("##FloatValue", &FloatTrack->KeyFrames[SelectedKeyIndex].Value, 0.1f, 0.0f, 0.0f, "%.2f");
	}
	else if (FEventTrack* EventTrack = std::get_if<FEventTrack>(&Track))
	{
		BeginPropertyRow("Event");
		ImGui::TextUnformatted(GetEventTypeLabel(EventTrack->KeyFrames[SelectedKeyIndex].EventType));
	}
}

void FSequencerPanel::DrawEaseCurve(FSequenceTrack& Track)
{
	Bezier* Ease = nullptr;
	if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track))
	{
		Ease = &TransformTrack->KeyFrames[SelectedKeyIndex].Ease;
	}
	else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
	{
		Ease = &FloatTrack->KeyFrames[SelectedKeyIndex].Ease;
	}

	if (!Ease || !ImGui::CollapsingHeader("Ease Curve", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (SelectedKeyIndex == FSequencerAdapter::GetKeyCount(Track) - 1)
	{
		ImGui::TextDisabled("마지막 키는 이징이 적용되지 않습니다");
		return;
	}

	if (EaseTrackIndex != SelectedTrackIndex || EaseKeyIndex != SelectedKeyIndex)
	{
		EaseTrackIndex = SelectedTrackIndex;
		EaseKeyIndex = SelectedKeyIndex;
		EasePresetIndex = 0.0f;
	}

	float Points[5] = { Ease->X1, Ease->Y1, Ease->X2, Ease->Y2, EasePresetIndex };
	ImGui::Bezier("##Ease", Points);

	Ease->X1 = Points[0];
	Ease->Y1 = Points[1];
	Ease->X2 = Points[2];
	Ease->Y2 = Points[3];
	EasePresetIndex = Points[4];
}

int32 FSequencerPanel::MoveKeyToFrame(FSequenceTrack& Track, int32 KeyIndex, int32 NewFrame)
{
	NewFrame = std::clamp(NewFrame, Adapter.GetFrameMin(), Adapter.GetFrameMax());
	const double NewTime = Adapter.FrameToTime(NewFrame);

	return std::visit([&](auto& Typed) -> int32
	{
		if constexpr (std::is_same_v<std::decay_t<decltype(Typed)>, std::monostate>)
		{
			return KeyIndex;
		}
		else
		{
			if (!Typed.KeyFrames.IsValidIndex(KeyIndex))
			{
				return KeyIndex;
			}
			return MoveKeyInArray(Typed.KeyFrames, KeyIndex, NewFrame, NewTime, Adapter);
		}
	}, Track);
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