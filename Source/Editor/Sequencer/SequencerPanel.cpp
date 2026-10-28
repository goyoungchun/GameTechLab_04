#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerPanel.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Collision/FPhysSceneChaos.h"
#include "Camera/CameraActor.h"
#include "UObject/UObjectIterator.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Camera/CameraComponent.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Sequence/SequencePlayer.h"
#include "Component/BillboardComponent.h"
#include "Asset/AssetManager.h"
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

	void DrawInterpolationRow(ESequenceInterpolation& Interpolation)
	{
		static const char* InterpolationNames[] = { "Constant", "Linear" };
		BeginPropertyRow("Interpolation");
		int Selected = static_cast<int>(Interpolation);
		if (ImGui::Combo("##Interpolation", &Selected, InterpolationNames, IM_ARRAYSIZE(InterpolationNames)))
		{
			Interpolation = static_cast<ESequenceInterpolation>(Selected);
		}
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

	FSequenceEventParams MakeDefaultEventParams(ESequenceEventType EventType)
	{
		switch (EventType)
		{
		case ESequenceEventType::HitStop:
			return FHitStopParams();
		case ESequenceEventType::Slomo:
			return FSlomoParams();
		case ESequenceEventType::CameraShake:
			return FCameraShakeParams();
		default:
			return std::monostate();
		}
	}

	bool DrawEventTypeCombo(ESequenceEventType& EventType)
	{
		bool bChanged = false;
		if (ImGui::BeginCombo("##EventType", GetEventTypeLabel(EventType)))
		{
			for (uint8 Type = static_cast<uint8>(ESequenceEventType::HitStop); Type < static_cast<uint8>(ESequenceEventType::Count); Type++)
			{
				const ESequenceEventType Candidate = static_cast<ESequenceEventType>(Type);
				if (ImGui::Selectable(GetEventTypeLabel(Candidate), Candidate == EventType) && Candidate != EventType)
				{
					EventType = Candidate;
					bChanged = true;
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	template<typename TKeyFrame>
	int32 FindOrInsertKey(TArray<TKeyFrame>& Keys, int32 Frame, const TKeyFrame& NewKey, bool& bOutExisting, const FSequencerAdapter& Adapter)
	{
		bOutExisting = false;
		for (int32 Index = 0; Index < Keys.Num(); Index++)
		{
			const int32 KeyFrame = Adapter.TimeToFrame(Keys[Index].Time);
			if (KeyFrame == Frame)
			{
				bOutExisting = true;
				return Index;
			}
			if (KeyFrame > Frame)
			{
				Keys.Insert(NewKey, static_cast<uint32>(Index));
				return Index;
			}
		}

		Keys.Add(NewKey);
		return Keys.Num() - 1;
	}

	float GetDefaultFloatValue(const FFloatTrack& Track)
	{
		const UCameraComponent* Camera = Cast<UCameraComponent>(Track.Binding);
		switch (Track.ValueType)
		{
		case ESequenceValueType::FOV:
			return Camera ? Camera->GetFieldOfView() : 90.0f;
		case ESequenceValueType::OrthoWidth:
			return Camera ? Camera->GetOrthoWidth() : 16.0f;
		default:
			return 0.0f;
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

	Player = FObjectFactory::ConstructObject<USequencePlayer>();
	Player->SetSequence(&EditingSequence);
	Player->SetPlaybackWorld(World);
	return true;
}

void FSequencerPanel::SetWorld(UWorld* InWorld)
{
	if (World != InWorld)
	{
		if (Player) Player->Stop();
		EndPlaybackPreview();
		PlaybackElapsedTime = 0.0;
	}
	World = InWorld;
	if (Player)
	{
		Player->SetPlaybackWorld(InWorld);
	}
}

void FSequencerPanel::ValidateBindings()
{
	TArray<UActorComponent*> LiveComponents;
	if (World)
	{
		for (TObjectIterator<ACameraActor> It; It; ++It)
		{
			ACameraActor* Camera = *It;
			if (Camera->GetWorld() == World)
			{
				LiveComponents.Add(Camera->GetCameraComponent());
			}
		}
	}

	for (FSequenceTrack& Track : EditingSequence.Tracks)
	{
		UActorComponent** BindingPtr = nullptr;
		if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track))
		{
			BindingPtr = &TransformTrack->Binding;
		}
		else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
		{
			BindingPtr = &FloatTrack->Binding;
		}

		if (BindingPtr && *BindingPtr && LiveComponents.Find(*BindingPtr) == LiveComponents.Num())
		{
			*BindingPtr = nullptr;
		}
	}
}

void FSequencerPanel::Tick(float DeltaTime)
{
	if (!Player || !World)
	{
		return;
	}

	ValidateBindings();
	const bool bPlaying = Player->GetPlaybackState() == ESequencePlaybackState::Playing;
	if (World->GetWorldType() == EWorldType::Editor)
	{
		if (bPlaying) BeginPlaybackPreview();
		else if (Player->GetPlaybackState() == ESequencePlaybackState::Stopped) EndPlaybackPreview();
	}

	const double UnscaledDeltaTime = std::isfinite(DeltaTime) && DeltaTime > 0.0f ? DeltaTime : 0.0;
	const float GameDeltaTime = static_cast<float>(World->GetTimeController().GetGameDeltaTime(UnscaledDeltaTime));
	// EditorWorld의 ViewportsOnly Tick은 시간 효과를 갱신하지 않는다.
	// 런타임 World는 자신의 Tick에서 갱신하므로 프리뷰 월드만 여기서 처리한다.
	if (World->GetWorldType() == EWorldType::Editor)
		World->GetTimeController().Update(UnscaledDeltaTime);
	if (bPlaying) PlaybackElapsedTime += UnscaledDeltaTime;

	Player->Update(GameDeltaTime, UnscaledDeltaTime);
	// 일반 Play로 Dynamic 물체도 같은 게임 시간만큼 진행한다. Pause/Hit Stop에서는 정지한다.
	// BeginPlay나 스크립트 실행 없이 기존 Solver만 사용하며, 런타임 World는 중복 갱신하지 않는다.
	if (bPlaybackPreviewActive && bPlaying && GameDeltaTime > 0.0f)
	{
		World->GetPhysicsScene().StartFrame(GameDeltaTime);
		World->GetPhysicsScene().EndFrame();
		World->GetScene().UpdateAllTransforms();
	}
	bIsPlaying = Player->GetPlaybackState() == ESequencePlaybackState::Playing;
	CurrentTime = static_cast<float>(Player->GetCurrentTime());
	CurrentFrame = Adapter.TimeToFrame(CurrentTime);

	if (bPreviewEnabled)
	{
		if (!bIsPlaying)
		{
			Player->Seek(CurrentTime, false);
		}
		ApplyPreviewToViewport();
	}
}


void FSequencerPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(800, 300), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	ImGui::Begin(GetPanelName());
	bWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	if (bWindowFocused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		DeleteSelectedKey();
	}

	DrawToolBar();
	DrawPlaybackStatus();
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

	if (ImGui::Button(bIsPlaying ? "Pause###PlayPause" : "Play###PlayPause") && Player)
	{
		if (bIsPlaying)
		{
			Player->Pause();
		}
		else
		{
			Player->Play();
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Stop") && Player)
	{
		Player->Stop();
		EndPlaybackPreview();
		PlaybackElapsedTime = 0.0;
		CurrentTime = 0.0f;
		CurrentFrame = 0;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Camera Preview", &bPreviewEnabled);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("카메라 시점만 뷰포트에 표시합니다. 트랙과 이벤트는 Play로 실행됩니다.");

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	ImGui::BeginDisabled(!CanAddKey());
	if (ImGui::Button("Add Key"))
	{
		FSequenceTrack& Track = EditingSequence.Tracks[SelectedTrackIndex];
		if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track))
		{
			FMinimalViewInfo ViewInfo;
			if (GetCurrentViewPOV(ViewInfo))
			{
				SelectedKeyIndex = AddTransformKey(*TransformTrack, CurrentTime, FTransform(ViewInfo.Rotation, ViewInfo.Location));
			}
		}
		else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
		{
			SelectedKeyIndex = AddFloatKey(*FloatTrack, CurrentTime);
		}
		else if (FEventTrack* EventTrack = std::get_if<FEventTrack>(&Track))
		{
			SelectedKeyIndex = AddEventKey(*EventTrack, CurrentTime);
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!HasSelectedKey());
	if (ImGui::Button("Delete Key"))
	{
		DeleteSelectedKey();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!World || !ViewportsPanel);
	if (ImGui::Button("Create Camera"))
	{
		CreateCameraForTrack();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	DrawSequenceSettings();

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
	const int PreviousSelectedTrackIndex = SelectedTrackIndex;
	ImSequencer::Sequencer(&Adapter, &CurrentFrame,
		&bExpanded, &SelectedTrackIndex, &FirstVisibleFrame,
		ImSequencer::SEQUENCER_CHANGE_FRAME | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL);

	int ClickedTrackIndex = -1;
	int ClickedKeyIndex = -1;
	const bool bClicked = Adapter.ConsumeIndex(ClickedTrackIndex, ClickedKeyIndex);
	const int DeletedTrackIndex = Adapter.ConsumeDeletedTrackIndex();

	if (DeletedTrackIndex != -1)
	{
		if (PreviousSelectedTrackIndex == DeletedTrackIndex)
		{
			SelectedTrackIndex = -1;
			SelectedKeyIndex = -1;
		}
		else if (PreviousSelectedTrackIndex > DeletedTrackIndex)
		{
			SelectedTrackIndex = PreviousSelectedTrackIndex - 1;
		}
		else
		{
			SelectedTrackIndex = PreviousSelectedTrackIndex;
		}
	}
	else if (bClicked)
	{
		SelectedTrackIndex = ClickedTrackIndex;
		SelectedKeyIndex = ClickedKeyIndex;
	}

	int DragTrackIndex = -1;
	int DragKeyIndex = -1;
	int DragFrame = -1;
	if (Adapter.ConsumeDragRequest(DragTrackIndex, DragKeyIndex, DragFrame)
		&& EditingSequence.Tracks.IsValidIndex(DragTrackIndex))
	{
		const int32 NewKeyIndex = MoveKeyToFrame(EditingSequence.Tracks[DragTrackIndex], DragKeyIndex, DragFrame);
		Adapter.SetDragKeyIndex(NewKeyIndex);
		SelectedTrackIndex = DragTrackIndex;
		SelectedKeyIndex = NewKeyIndex;
	}

	if (CurrentFrame != PreviousFrame)
	{
		CurrentTime = static_cast<float>(Adapter.FrameToTime(CurrentFrame));
		if (Player)
		{
			Player->Seek(CurrentTime, false);
		}
	}
}

void FSequencerPanel::DrawTrackBinding()
{
	if (SelectedTrackIndex == -1 || !EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex))
	{
		ImGui::TextDisabled("트랙을 선택해주세요");
		return;
	}

	FSequenceTrack& SelectedTrack = EditingSequence.Tracks[SelectedTrackIndex];
	UActorComponent** BindingPtr = nullptr;
	if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&SelectedTrack))
	{
		BindingPtr = &TransformTrack->Binding;
	}
	else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&SelectedTrack))
	{
		BindingPtr = &FloatTrack->Binding;
	}
	if (!BindingPtr)
	{
		ImGui::TextDisabled("이벤트 트랙은 바인딩이 필요 없습니다");
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

	UActorComponent*& Binding = *BindingPtr;
	ACameraActor* BoundCamera = nullptr;
	if (Binding)
	{
		for (ACameraActor* Camera : Cameras)
		{
			if (Camera->GetCameraComponent() == Binding)
			{
				BoundCamera = Camera;
				break;
			}
		}
		if (!BoundCamera)
		{
			Binding = nullptr;
		}
	}

	const FString PreviewName = BoundCamera ? BoundCamera->GetName() : FString("None");

	ImGui::TextUnformatted(FSequencerAdapter::GetTrackName(SelectedTrack));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##Binding", PreviewName.c_str()))
	{
		if (ImGui::Selectable("None", Binding == nullptr))
		{
			Binding = nullptr;
		}

		for (ACameraActor* Camera : Cameras)
		{
			const bool bSelected = BoundCamera == Camera;
			ImGui::PushID(Camera);
			if (ImGui::Selectable(Camera->GetName().c_str(), bSelected))
			{
				Binding = Camera->GetCameraComponent();
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

		DrawInterpolationRow(TransformTrack->KeyFrames[SelectedKeyIndex].Interpolation);
	}
	else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
	{
		BeginPropertyRow(GetValueTypeLabel(FloatTrack->ValueType));
		ImGui::DragFloat("##FloatValue", &FloatTrack->KeyFrames[SelectedKeyIndex].Value, 0.1f, 0.0f, 0.0f, "%.2f");

		DrawInterpolationRow(FloatTrack->KeyFrames[SelectedKeyIndex].Interpolation);
	}
	else if (FEventTrack* EventTrack = std::get_if<FEventTrack>(&Track))
	{
		BeginPropertyRow("Event");
		FEventKeyFrame& EventKey = EventTrack->KeyFrames[SelectedKeyIndex];
		if (DrawEventTypeCombo(EventKey.EventType))
		{
			EventKey.Params = MakeDefaultEventParams(EventKey.EventType);
			LastEventType = EventKey.EventType;
		}
		if (auto* HitStop = std::get_if<FHitStopParams>(&EventKey.Params))
		{
			BeginPropertyRow("Duration");
			ImGui::DragFloat("##HitStopDuration", &HitStop->Duration, 0.01f, 0.0f, 10.0f, "%.2fs", ImGuiSliderFlags_AlwaysClamp);
		}
		else if (auto* Slomo = std::get_if<FSlomoParams>(&EventKey.Params))
		{
			BeginPropertyRow("Time Scale");
			ImGui::DragFloat("##SlomoScale", &Slomo->TargetScale, 0.01f, 0.0f, 1.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
			BeginPropertyRow("Duration");
			if (ImGui::DragFloat("##SlomoDuration", &Slomo->Duration, 0.01f, 0.0f, 10.0f, "%.2fs", ImGuiSliderFlags_AlwaysClamp))
			{
				Slomo->BlendInTime = std::min(Slomo->BlendInTime, Slomo->Duration);
				Slomo->BlendOutTime = std::min(Slomo->BlendOutTime, Slomo->Duration - Slomo->BlendInTime);
			}
			BeginPropertyRow("Blend In");
			if (ImGui::DragFloat("##SlomoBlendIn", &Slomo->BlendInTime, 0.01f, 0.0f, Slomo->Duration, "%.2fs", ImGuiSliderFlags_AlwaysClamp))
				Slomo->BlendInTime = std::clamp(Slomo->BlendInTime, 0.0f, Slomo->Duration - Slomo->BlendOutTime);
			BeginPropertyRow("Blend Out");
			if (ImGui::DragFloat("##SlomoBlendOut", &Slomo->BlendOutTime, 0.01f, 0.0f, Slomo->Duration, "%.2fs", ImGuiSliderFlags_AlwaysClamp))
				Slomo->BlendOutTime = std::clamp(Slomo->BlendOutTime, 0.0f, Slomo->Duration - Slomo->BlendInTime);
		}
		else if (EventKey.EventType == ESequenceEventType::CameraShake)
		{
			BeginPropertyRow("Status");
			ImGui::TextDisabled("Camera Shake is not implemented");
		}
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
	const bool bConstant = FSequencerAdapter::GetKeyInterpolation(Track, SelectedKeyIndex) == ESequenceInterpolation::Constant;
	if (bConstant)
	{
		ImGui::TextDisabled("Constant 보간에서는 이징이 적용되지 않습니다");
	}
	ImGui::BeginDisabled(bConstant);
	ImGui::Bezier("##Ease", Points);
	ImGui::EndDisabled();

	Ease->X1 = Points[0];
	Ease->Y1 = Points[1];
	Ease->X2 = Points[2];
	Ease->Y2 = Points[3];
	EasePresetIndex = Points[4];
}

void FSequencerPanel::DrawSequenceSettings()
{
	const float InputWidth = ImGui::GetFontSize() * 4.0f;

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Length");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(InputWidth);
	float Length = static_cast<float>(EditingSequence.Length);
	if (ImGui::DragFloat("##Length", &Length, 0.05f, MinSequenceLength, MaxSequenceLength, "%.2fs", ImGuiSliderFlags_AlwaysClamp))
	{
		EditingSequence.Length = Length;
		if (CurrentTime > Length)
		{
			CurrentTime = Length;
			if (Player)
			{
				Player->Seek(CurrentTime, false);
			}
		}
		CurrentFrame = Adapter.TimeToFrame(CurrentTime);
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("FPS");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(InputWidth);
	float FrameRate = EditingSequence.FrameRate;
	if (ImGui::DragFloat("##FrameRate", &FrameRate, 0.5f, MinFrameRate, MaxFrameRate, "%g", ImGuiSliderFlags_AlwaysClamp))
	{
		EditingSequence.FrameRate = FrameRate;
		CurrentFrame = Adapter.TimeToFrame(CurrentTime);
	}
}

bool FSequencerPanel::HasSelectedKey() const
{
	return EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex)
		&& SelectedKeyIndex >= 0
		&& SelectedKeyIndex < FSequencerAdapter::GetKeyCount(EditingSequence.Tracks[SelectedTrackIndex]);
}

void FSequencerPanel::DeleteSelectedKey()
{
	if (!EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex))
	{
		return;
	}

	const int32 KeyIndex = SelectedKeyIndex;
	std::visit([KeyIndex](auto& Typed)
	{
		if constexpr (!std::is_same_v<std::decay_t<decltype(Typed)>, std::monostate>)
		{
			if (Typed.KeyFrames.IsValidIndex(KeyIndex))
			{
				Typed.KeyFrames.RemoveAt(static_cast<uint32>(KeyIndex), 1);
			}
		}
	}, EditingSequence.Tracks[SelectedTrackIndex]);

	SelectedKeyIndex = -1;
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
	if (!EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex))
	{
		return false;
	}

	const FSequenceTrack& Track = EditingSequence.Tracks[SelectedTrackIndex];
	return std::holds_alternative<FTransformTrack>(Track)
		|| std::holds_alternative<FFloatTrack>(Track)
		|| std::holds_alternative<FEventTrack>(Track);
}

int32 FSequencerPanel::AddEventKey(FEventTrack& InTrack, double InTime)
{
	const int32 NewFrame = Adapter.TimeToFrame(InTime);

	FEventKeyFrame NewKey;
	NewKey.Time = Adapter.FrameToTime(NewFrame);
	NewKey.EventType = LastEventType;
	NewKey.Params = MakeDefaultEventParams(LastEventType);

	bool bExisting = false;
	return FindOrInsertKey(InTrack.KeyFrames, NewFrame, NewKey, bExisting, Adapter);
}

bool FSequencerPanel::CanAddTransformKey() const
{
	return (EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex)
		&& std::holds_alternative<FTransformTrack>(EditingSequence.Tracks[SelectedTrackIndex]));
}

int32 FSequencerPanel::AddTransformKey(FTransformTrack& InTrack, double InTime, const FTransform& InValue)
{
	const int32 NewFrame = Adapter.TimeToFrame(InTime);

	FTransformKeyFrame NewKey;
	NewKey.Time = Adapter.FrameToTime(NewFrame);
	NewKey.Value = InValue;

	bool bExisting = false;
	const int32 Index = FindOrInsertKey(InTrack.KeyFrames, NewFrame, NewKey, bExisting, Adapter);
	if (bExisting)
	{
		InTrack.KeyFrames[Index].Value = InValue;
	}
	return Index;
}

int32 FSequencerPanel::AddFloatKey(FFloatTrack& InTrack, double InTime)
{
	const int32 NewFrame = Adapter.TimeToFrame(InTime);

	float Value = GetDefaultFloatValue(InTrack);
	bool bFoundPrevious = false;
	for (const FFloatKeyFrame& Key : InTrack.KeyFrames)
	{
		if (Adapter.TimeToFrame(Key.Time) < NewFrame)
		{
			Value = Key.Value;
			bFoundPrevious = true;
		}
		else
		{
			if (!bFoundPrevious)
			{
				Value = Key.Value;
			}
			break;
		}
	}

	FFloatKeyFrame NewKey;
	NewKey.Time = Adapter.FrameToTime(NewFrame);
	NewKey.Value = Value;

	bool bExisting = false;
	return FindOrInsertKey(InTrack.KeyFrames, NewFrame, NewKey, bExisting, Adapter);
}

bool FSequencerPanel::GetCurrentViewPOV(FMinimalViewInfo& OutView) const
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

void FSequencerPanel::CreateCameraForTrack()
{
	FMinimalViewInfo View;
	if (!World || !GetCurrentViewPOV(View))
	{
		return;
	}

	const FTransform SpawnTransform(View.Rotation, View.Location);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(NAME_None, &SpawnTransform);
	if (!Camera || !Camera->GetCameraComponent())
	{
		return;
	}
	Camera->GetCameraComponent()->SetFieldOfView(View.FOV);

	UBillboardComponent* Icon = Camera->CreateDefaultSubobject<UBillboardComponent>("CameraIcon");
	Icon->SetupAttachment(Camera->GetCameraComponent());
	Icon->SetSprite(UAssetManager::Get().LoadTexture(CameraIconPath));
	Icon->SetHiddenInGame(true);
	Icon->RegisterComponent();

	const bool bSelectedTransformTrack = EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex)
		&& std::holds_alternative<FTransformTrack>(EditingSequence.Tracks[SelectedTrackIndex]);
	if (!bSelectedTransformTrack)
	{
		Adapter.Add(0);
		SelectedTrackIndex = EditingSequence.Tracks.Num() - 1;
		SelectedKeyIndex = -1;
	}

	if (FTransformTrack* Track = std::get_if<FTransformTrack>(&EditingSequence.Tracks[SelectedTrackIndex]))
	{
		Track->Binding = Camera->GetCameraComponent();
	}

	for (FSequenceTrack& Track : EditingSequence.Tracks)
	{
		FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track);
		if (FloatTrack && !FloatTrack->Binding &&
			(FloatTrack->ValueType == ESequenceValueType::FOV || FloatTrack->ValueType == ESequenceValueType::OrthoWidth))
		{
			FloatTrack->Binding = Camera->GetCameraComponent();
		}
	}
}

UCameraComponent* FSequencerPanel::FindPreviewCamera() const
{
	if (!World)
	{
		return nullptr;
	}

	const FTransformTrack* PreviewTrack = nullptr;
	if (EditingSequence.Tracks.IsValidIndex(SelectedTrackIndex))
	{
		const FTransformTrack* Selected = std::get_if<FTransformTrack>(&EditingSequence.Tracks[SelectedTrackIndex]);
		if (Selected && Selected->Binding)
		{
			PreviewTrack = Selected;
		}
	}
	if (!PreviewTrack)
	{
		for (const FSequenceTrack& Track : EditingSequence.Tracks)
		{
			const FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track);
			if (TransformTrack && TransformTrack->Binding)
			{
				PreviewTrack = TransformTrack;
				break;
			}
		}
	}
	if (!PreviewTrack)
	{
		return nullptr;
	}

	for (TObjectIterator<ACameraActor> It; It; ++It)
	{
		ACameraActor* Camera = *It;
		if (Camera->GetWorld() == World && Camera->GetCameraComponent() == PreviewTrack->Binding)
		{
			return Camera->GetCameraComponent();
		}
	}
	return nullptr;
}

void FSequencerPanel::ApplyPreviewToViewport()
{
	if (!ViewportsPanel)
	{
		return;
	}

	UCameraComponent* Camera = FindPreviewCamera();
	FEditorViewportClient* ViewportClient = ViewportsPanel->GetViewportClient(ViewportsPanel->GetActiveViewIndex());
	if (!Camera || !ViewportClient)
	{
		return;
	}

	FMinimalViewInfo View;
	Camera->GetCameraView(View);
	ViewportClient->SetViewLocation(View.Location);
	ViewportClient->SetViewRotation(View.Rotation);
	ViewportClient->SetViewFOV(View.FOV);
}

void FSequencerPanel::BeginPlaybackPreview()
{
	if (!World || World->GetWorldType() != EWorldType::Editor) return;
	if (!bPlaybackPreviewActive)
	{
		SavedPreviewTimeController = World->GetTimeController();
		PhysicsPreviewTransforms.Reset();
		bPlaybackPreviewActive = true;
	}
	if (World->GetPersistentLevel())
		for (AActor* Actor : World->GetPersistentLevel()->GetActors())
			for (UActorComponent* Component : Actor->GetComponents())
				if (auto* Body = Cast<UPrimitiveComponent>(Component))
					if (Body->IsRegistered() && Body->GetMotionType() == EPhysicsMotionType::Dynamic
						&& !PhysicsPreviewTransforms.Contains(Body->GetUUID()))
						PhysicsPreviewTransforms.Add(Body->GetUUID(), Body->GetTransform());
}

void FSequencerPanel::EndPlaybackPreview()
{
	if (!bPlaybackPreviewActive || !World) return;
	// 컴포넌트 포인터를 보관하지 않고 현재 살아 있는 객체의 ID로 복원한다.
	if (World->GetPersistentLevel())
		for (AActor* Actor : World->GetPersistentLevel()->GetActors())
			for (UActorComponent* Component : Actor->GetComponents())
				if (auto* Body = Cast<UPrimitiveComponent>(Component))
					if (const FTransform* Before = PhysicsPreviewTransforms.Find(Body->GetUUID()))
					{
						Body->SetTransform(*Before);
						World->GetPhysicsScene().ResetPrimitiveVelocity(Body);
					}
	World->GetTimeController() = SavedPreviewTimeController;
	PhysicsPreviewTransforms.Reset();
	bPlaybackPreviewActive = false;
	PlaybackElapsedTime = 0.0;
}

void FSequencerPanel::DrawPlaybackStatus()
{
	if (!World || !Player) return;
	const FTimeController& Time = World->GetTimeController();
	const char* State = Time.IsHitStopped() ? "HIT STOP" : Time.IsSlomoActive() ? "SLOMO" : "NORMAL";
	ImGui::Text("%s | Sequence %.2fs | Real %.2fs | Scale %.2fx | Hit Stop %.2fs",
		State, Player->GetCurrentTime(), PlaybackElapsedTime, Time.GetEffectiveTimeDilation(), Time.GetHitStopTimeRemaining());
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
	FEventKeyFrame HitStop;
	HitStop.Time = 1.0;
	HitStop.EventType = ESequenceEventType::HitStop;
	HitStop.Params = FHitStopParams{0.3f};
	EventTrack.KeyFrames.Add(HitStop);
	FEventKeyFrame Slomo;
	Slomo.Time = 2.5;
	Slomo.EventType = ESequenceEventType::Slomo;
	FSlomoParams Params;
	Params.TargetScale = 0.25f;
	Params.Duration = 1.5f;
	Params.BlendInTime = 0.15f;
	Params.BlendOutTime = 0.15f;
	Slomo.Params = Params;
	EventTrack.KeyFrames.Add(Slomo);
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
