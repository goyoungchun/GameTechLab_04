#include "EnginePCH.h"
#include "Sequence/SequenceSerializer.h"

#include "Asset/AssetManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Component/BillboardComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Serialization/TypeSerializer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace
{
	constexpr int32 SequenceFileVersion = 1;

	const char* ValueTypeToString(ESequenceValueType ValueType)
	{
		switch (ValueType)
		{
		case ESequenceValueType::FOV: return "FOV";
		case ESequenceValueType::Transform: return "Transform";
		case ESequenceValueType::OrthoWidth: return "OrthoWidth";
		case ESequenceValueType::LightIntensity: return "LightIntensity";
		default: return "None";
		}
	}

	ESequenceValueType ValueTypeFromString(const FString& Name)
	{
		for (uint8 Type = 0; Type < static_cast<uint8>(ESequenceValueType::Count); Type++)
		{
			const ESequenceValueType Candidate = static_cast<ESequenceValueType>(Type);
			if (Name == ValueTypeToString(Candidate))
				return Candidate;
		}
		return ESequenceValueType::None;
	}

	const char* EventTypeToString(ESequenceEventType EventType)
	{
		switch (EventType)
		{
		case ESequenceEventType::HitStop: return "HitStop";
		case ESequenceEventType::Slomo: return "Slomo";
		case ESequenceEventType::CameraShake: return "CameraShake";
		case ESequenceEventType::CameraFade: return "CameraFade";
		case ESequenceEventType::StopCameraShake: return "StopCameraShake";
		case ESequenceEventType::StopCameraFade: return "StopCameraFade";
		case ESequenceEventType::Letterbox: return "Letterbox";
		default: return "None";
		}
	}

	ESequenceEventType EventTypeFromString(const FString& Name)
	{
		for (uint8 Type = 0; Type < static_cast<uint8>(ESequenceEventType::Count); Type++)
		{
			const ESequenceEventType Candidate = static_cast<ESequenceEventType>(Type);
			if (Name == EventTypeToString(Candidate))
				return Candidate;
		}
		return ESequenceEventType::None;
	}

	const char* InterpolationToString(ESequenceInterpolation Interpolation)
	{
		if (Interpolation == ESequenceInterpolation::Constant) return "Constant";
		return Interpolation == ESequenceInterpolation::Bezier ? "Bezier" : "Linear";
	}

	ESequenceInterpolation InterpolationFromString(const FString& Name)
	{
		if (Name == "Constant") return ESequenceInterpolation::Constant;
		return Name == "Bezier" ? ESequenceInterpolation::Bezier : ESequenceInterpolation::Linear;
	}

	json BezierToJson(const Bezier& Ease)
	{
		return json::array({ Ease.X1, Ease.Y1, Ease.X2, Ease.Y2 });
	}

	Bezier BezierFromJson(const json& Value)
	{
		Bezier Ease;
		if (Value.is_array() && Value.size() == 4)
		{
			Ease.X1 = Value[0].get<float>();
			Ease.Y1 = Value[1].get<float>();
			Ease.X2 = Value[2].get<float>();
			Ease.Y2 = Value[3].get<float>();
		}
		return Ease;
	}

	json EventParamsToJson(const FSequenceEventParams& Params)
	{
		json Result = json::object();
		if (const FHitStopParams* HitStop = std::get_if<FHitStopParams>(&Params))
		{
			Result["Duration"] = HitStop->Duration;
		}
		else if (const FSlomoParams* Slomo = std::get_if<FSlomoParams>(&Params))
		{
			Result["TargetScale"] = Slomo->TargetScale;
			Result["Duration"] = Slomo->Duration;
			Result["BlendInTime"] = Slomo->BlendInTime;
			Result["BlendOutTime"] = Slomo->BlendOutTime;
			Result["Ease"] = BezierToJson(Slomo->Ease);
		}
		else if (const FCameraShakeParams* Shake = std::get_if<FCameraShakeParams>(&Params))
		{
			Result["Duration"] = Shake->Duration;
			Result["Strength"] = Shake->Strength;
			Result["Type"] = Shake->Type == ECameraShakeType::Sine ? "Sine" : "Perlin";
			Result["Frequency"] = Shake->Frequency;
			Result["Ease"] = BezierToJson(Shake->Ease);
		}
		else if (const auto* Fade = std::get_if<FCameraFadeParams>(&Params))
		{
			Result = {{"FromAlpha", Fade->FromAlpha}, {"ToAlpha", Fade->ToAlpha}, {"Duration", Fade->Duration},
				{"Color", Fade->Color}, {"Hold", Fade->bHoldWhenFinished}, {"Ease", BezierToJson(Fade->Ease)}};
		}
		else if (const auto* Bars = std::get_if<FLetterboxParams>(&Params))
			Result = {{"Enable", Bars->bEnable}, {"Ratio", Bars->Ratio}};
		return Result;
	}

	FSequenceEventParams EventParamsFromJson(ESequenceEventType EventType, const json& Value)
	{
		switch (EventType)
		{
		case ESequenceEventType::CameraFade:
		{
			FCameraFadeParams Params;
			Params.FromAlpha = Value.value("FromAlpha", Params.FromAlpha);
			Params.ToAlpha = Value.value("ToAlpha", Params.ToAlpha);
			Params.Duration = Value.value("Duration", Params.Duration);
			Params.Color = Value.value("Color", Params.Color);
			Params.bHoldWhenFinished = Value.value("Hold", Params.bHoldWhenFinished);
			Params.Ease = BezierFromJson(Value.value("Ease", json()));
			return Params;
		}
		case ESequenceEventType::Letterbox:
		{
			FLetterboxParams Params;
			Params.bEnable = Value.value("Enable", Params.bEnable);
			Params.Ratio = Value.value("Ratio", Params.Ratio);
			return Params;
		}
		case ESequenceEventType::HitStop:
		{
			FHitStopParams Params;
			Params.Duration = Value.value("Duration", Params.Duration);
			return Params;
		}
		case ESequenceEventType::Slomo:
		{
			FSlomoParams Params;
			Params.TargetScale = Value.value("TargetScale", Params.TargetScale);
			Params.Duration = Value.value("Duration", Params.Duration);
			Params.BlendInTime = Value.value("BlendInTime", Params.BlendInTime);
			Params.BlendOutTime = Value.value("BlendOutTime", Params.BlendOutTime);
			if (Value.contains("Ease"))
				Params.Ease = BezierFromJson(Value["Ease"]);
			return Params;
		}
		case ESequenceEventType::CameraShake:
		{
			FCameraShakeParams Params;
			Params.Type = Value.value("Type", FString("Perlin")) == "Sine" ? ECameraShakeType::Sine : ECameraShakeType::Perlin;
			Params.Frequency = Value.value("Frequency", Params.Frequency);
			Params.Duration = Value.value("Duration", Params.Duration);
			Params.Strength = Value.value("Strength", Params.Strength);
			if (Value.contains("Ease"))
				Params.Ease = BezierFromJson(Value["Ease"]);
			return Params;
		}
		default:
			return std::monostate();
		}
	}

	template<typename TKeyFrame>
	void SortKeys(TArray<TKeyFrame>& Keys)
	{
		std::stable_sort(Keys.begin(), Keys.end(),
			[](const TKeyFrame& A, const TKeyFrame& B) { return A.Time < B.Time; });
	}

	struct FCameraData
	{
		FTransform Transform;
		float FOV = 90.0f;
		float OrthoWidth = 16.0f;
		bool bOrthographic = false;
	};

	json CameraToJson(const UCameraComponent* Camera)
	{
		const FTransform Transform(Camera->GetWorldRotation(), Camera->GetWorldLocation(), Camera->GetWorldScale3D());
		json Result;
		Result["Transform"] = Transform;
		Result["FOV"] = Camera->GetFieldOfView();
		Result["OrthoWidth"] = Camera->GetOrthoWidth();
		Result["Orthographic"] = Camera->GetIsOrthogonal();
		return Result;
	}

	FCameraData CameraFromJson(const json& Value)
	{
		FCameraData Data;
		Value.at("Transform").get_to(Data.Transform);
		Data.FOV = Value.value("FOV", Data.FOV);
		Data.OrthoWidth = Value.value("OrthoWidth", Data.OrthoWidth);
		Data.bOrthographic = Value.value("Orthographic", Data.bOrthographic);
		return Data;
	}

	struct FBindingRef
	{
		int32 CameraIndex = -1;
		FString ActorName;
		FString ComponentName;
	};

	FBindingRef BindingRefFromJson(const json& TrackJson, int32 CameraCount)
	{
		FBindingRef Ref;
		const int32 CameraIndex = TrackJson.value("Binding", -1);
		if (CameraIndex >= 0 && CameraIndex < CameraCount)
			Ref.CameraIndex = CameraIndex;
		Ref.ActorName = TrackJson.value("Actor", FString());
		Ref.ComponentName = TrackJson.value("Component", FString());
		return Ref;
	}

	USceneComponent* FindSceneComponent(UWorld* World, const FString& ActorName, const FString& ComponentName)
	{
		if (!World || !World->GetPersistentLevel() || ActorName.empty())
			return nullptr;

		for (AActor* Actor : World->GetPersistentLevel()->GetActors())
		{
			if (!Actor || Actor->GetName() != ActorName)
				continue;

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component && Component->GetName() == ComponentName)
					return Cast<USceneComponent>(Component);
			}
			return nullptr;
		}
		return nullptr;
	}

	void SetTrackBinding(FSequenceTrack& Track, UActorComponent* Binding)
	{
		if (FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track))
			TransformTrack->Binding = Binding;
		else if (FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
			FloatTrack->Binding = Binding;
	}
}

bool FSequenceSerializer::Save(const FSequence& Sequence, const FString& Path, const FString& ScenePath)
{
	json Json;
	Json["Version"] = SequenceFileVersion;
	if (!ScenePath.empty())
		Json["Scene"] = ScenePath;
	Json["Length"] = Sequence.Length;
	Json["FrameRate"] = Sequence.FrameRate;
	Json["PlayRate"] = Sequence.PlayRate;
	Json["Cameras"] = json::array();
	Json["Tracks"] = json::array();

	TArray<UCameraComponent*> Cameras;
	auto WriteBinding = [&Cameras, &Json](json& TrackJson, UActorComponent* Binding)
	{
		TrackJson["Binding"] = -1;
		USceneComponent* Component = Cast<USceneComponent>(Binding);
		AActor* Owner = Component ? Component->GetOwner() : nullptr;
		if (!Owner)
			return;

		UCameraComponent* Camera = Cast<UCameraComponent>(Component);
		if (Camera && IsSequenceCamera(Cast<ACameraActor>(Owner)))
		{
			auto Found = std::find(Cameras.begin(), Cameras.end(), Camera);
			if (Found != Cameras.end())
			{
				TrackJson["Binding"] = static_cast<int32>(Found - Cameras.begin());
				return;
			}

			Cameras.Add(Camera);
			Json["Cameras"].push_back(CameraToJson(Camera));
			TrackJson["Binding"] = Cameras.Num() - 1;
			return;
		}

		TrackJson["Actor"] = Owner->GetName();
		TrackJson["Component"] = Component->GetName();
	};

	for (const FSequenceTrack& Track : Sequence.Tracks)
	{
		json TrackJson;
		if (const FTransformTrack* TransformTrack = std::get_if<FTransformTrack>(&Track))
		{
			TrackJson["Type"] = "Transform";
			TrackJson["Name"] = TransformTrack->Name;
			TrackJson["ValueType"] = ValueTypeToString(TransformTrack->ValueType);
			TrackJson["Relative"] = TransformTrack->bRelative;
			TrackJson["AnimateLocation"] = TransformTrack->bAnimateLocation;
			TrackJson["AnimateRotation"] = TransformTrack->bAnimateRotation;
			TrackJson["AnimateScale"] = TransformTrack->bAnimateScale;
			WriteBinding(TrackJson, TransformTrack->Binding);
			TrackJson["Keys"] = json::array();
			for (const FTransformKeyFrame& Key : TransformTrack->KeyFrames)
			{
				TrackJson["Keys"].push_back({
					{ "Time", Key.Time },
					{ "Value", Key.Value },
					{ "Ease", BezierToJson(Key.Ease) },
					{ "Interpolation", InterpolationToString(Key.Interpolation) } });
			}
		}
		else if (const FFloatTrack* FloatTrack = std::get_if<FFloatTrack>(&Track))
		{
			TrackJson["Type"] = "Float";
			TrackJson["Name"] = FloatTrack->Name;
			TrackJson["ValueType"] = ValueTypeToString(FloatTrack->ValueType);
			WriteBinding(TrackJson, FloatTrack->Binding);
			TrackJson["Keys"] = json::array();
			for (const FFloatKeyFrame& Key : FloatTrack->KeyFrames)
			{
				TrackJson["Keys"].push_back({
					{ "Time", Key.Time },
					{ "Value", Key.Value },
					{ "Ease", BezierToJson(Key.Ease) },
					{ "Interpolation", InterpolationToString(Key.Interpolation) } });
			}
		}
		else if (const FEventTrack* EventTrack = std::get_if<FEventTrack>(&Track))
		{
			TrackJson["Type"] = "Event";
			TrackJson["Name"] = EventTrack->Name;
			TrackJson["Keys"] = json::array();
			for (const FEventKeyFrame& Key : EventTrack->KeyFrames)
			{
				if (!Key.IsValid())
				{
					HTR_LOG(Warning, "Sequence: skipped invalid event key at {}s", Key.Time);
					continue;
				}
				TrackJson["Keys"].push_back({
					{ "Time", Key.Time },
					{ "Event", EventTypeToString(Key.EventType) },
					{ "Params", EventParamsToJson(Key.Params) } });
			}
		}
		else
		{
			continue;
		}
		Json["Tracks"].push_back(TrackJson);
	}

	std::error_code Error;
	const std::filesystem::path FilePath(Path);
	if (FilePath.has_parent_path())
		std::filesystem::create_directories(FilePath.parent_path(), Error);

	std::ofstream File(FilePath);
	if (!File.is_open())
	{
		HTR_LOG(Error, "Sequence: failed to open {}", Path);
		return false;
	}

	File << Json.dump(4);
	File.close();
	return !File.fail();
}

bool FSequenceSerializer::Load(const FString& Path, UWorld* World, FSequence& OutSequence, TArray<ACameraActor*>& OutCameras, FString* OutScenePath)
{
	if (!World)
		return false;

	std::ifstream File(Path);
	if (!File.is_open())
	{
		HTR_LOG(Warning, "Sequence: {} not found", Path);
		return false;
	}

	FSequence Loaded;
	FString ScenePath;
	TArray<FCameraData> CameraData;
	TArray<FBindingRef> TrackBindings;

	try
	{
		json Json;
		File >> Json;

		if (Json.value("Version", 0) != SequenceFileVersion)
		{
			HTR_LOG(Warning, "Sequence: unsupported version in {}", Path);
			return false;
		}

		ScenePath = Json.value("Scene", FString());
		Loaded.Length = Json.value("Length", Loaded.Length);
		Loaded.FrameRate = Json.value("FrameRate", Loaded.FrameRate);
		Loaded.PlayRate = Json.value("PlayRate", Loaded.PlayRate);

		for (const json& CameraJson : Json.at("Cameras"))
		{
			CameraData.Add(CameraFromJson(CameraJson));
		}

		for (const json& TrackJson : Json.at("Tracks"))
		{
			const FString Type = TrackJson.at("Type").get<FString>();
			const FString Name = TrackJson.value("Name", FString());
			const FBindingRef Binding = BindingRefFromJson(TrackJson, CameraData.Num());

			if (Type == "Transform")
			{
				FTransformTrack Track;
				Track.Name = Name;
				Track.ValueType = ValueTypeFromString(TrackJson.at("ValueType").get<FString>());
				Track.bRelative = TrackJson.value("Relative", false);
				Track.bAnimateLocation = TrackJson.value("AnimateLocation", true);
				Track.bAnimateRotation = TrackJson.value("AnimateRotation", true);
				Track.bAnimateScale = TrackJson.value("AnimateScale", true);
				for (const json& KeyJson : TrackJson.at("Keys"))
				{
					FTransformKeyFrame Key;
					Key.Time = KeyJson.at("Time").get<double>();
					KeyJson.at("Value").get_to(Key.Value);
					Key.Ease = BezierFromJson(KeyJson.value("Ease", json()));
					Key.Interpolation = InterpolationFromString(KeyJson.value("Interpolation", FString("Linear")));
					Track.KeyFrames.Add(Key);
				}
				if (!Track.HasCompatibleValueType())
				{
					HTR_LOG(Warning, "Sequence: track {} has invalid value type", Name);
					return false;
				}
				SortKeys(Track.KeyFrames);
				Loaded.Tracks.Add(Track);
			}
			else if (Type == "Float")
			{
				FFloatTrack Track;
				Track.Name = Name;
				Track.ValueType = ValueTypeFromString(TrackJson.at("ValueType").get<FString>());
				for (const json& KeyJson : TrackJson.at("Keys"))
				{
					FFloatKeyFrame Key;
					Key.Time = KeyJson.at("Time").get<double>();
					Key.Value = KeyJson.at("Value").get<float>();
					Key.Ease = BezierFromJson(KeyJson.value("Ease", json()));
					Key.Interpolation = InterpolationFromString(KeyJson.value("Interpolation", FString("Linear")));
					Track.KeyFrames.Add(Key);
				}
				if (!Track.HasCompatibleValueType())
				{
					HTR_LOG(Warning, "Sequence: track {} has invalid value type", Name);
					return false;
				}
				SortKeys(Track.KeyFrames);
				Loaded.Tracks.Add(Track);
			}
			else if (Type == "Event")
			{
				FEventTrack Track;
				Track.Name = Name;
				for (const json& KeyJson : TrackJson.at("Keys"))
				{
					FEventKeyFrame Key;
					Key.Time = KeyJson.at("Time").get<double>();
					Key.EventType = EventTypeFromString(KeyJson.at("Event").get<FString>());
					Key.Params = EventParamsFromJson(Key.EventType, KeyJson.value("Params", json::object()));
					if (!Key.IsValid())
					{
						HTR_LOG(Warning, "Sequence: skipped invalid event key at {}s", Key.Time);
						continue;
					}
					Track.KeyFrames.Add(Key);
				}
				SortKeys(Track.KeyFrames);
				Loaded.Tracks.Add(Track);
			}
			else
			{
				HTR_LOG(Warning, "Sequence: unknown track type {}", Type);
				continue;
			}

			TrackBindings.Add(Binding);
		}
	}
	catch (const json::exception& Exception)
	{
		HTR_LOG(Warning, "Sequence: failed to parse {} ({})", Path, Exception.what());
		return false;
	}

	TArray<ACameraActor*> Cameras;
	for (const FCameraData& Data : CameraData)
	{
		ACameraActor* Camera = SpawnCamera(World, Data.Transform);
		UCameraComponent* Component = Camera ? Camera->GetCameraComponent() : nullptr;
		if (Component)
		{
			Component->SetFieldOfView(Data.FOV);
			Component->SetOrthoWidth(Data.OrthoWidth);
			Component->SetIsOrthogonal(Data.bOrthographic);
		}
		Cameras.Add(Camera);
	}

	for (int32 Index = 0; Index < Loaded.Tracks.Num(); Index++)
	{
		const FBindingRef& Ref = TrackBindings[Index];
		UActorComponent* Binding = nullptr;
		if (Ref.CameraIndex >= 0)
		{
			ACameraActor* Camera = Cameras[Ref.CameraIndex];
			Binding = Camera ? Camera->GetCameraComponent() : nullptr;
		}
		else if (!Ref.ActorName.empty())
		{
			Binding = FindSceneComponent(World, Ref.ActorName, Ref.ComponentName);
			if (!Binding)
				HTR_LOG(Warning, "Sequence: binding {}.{} not found", Ref.ActorName, Ref.ComponentName);
		}
		SetTrackBinding(Loaded.Tracks[Index], Binding);
	}

	OutSequence = Loaded;
	OutCameras = Cameras;
	if (OutScenePath)
		*OutScenePath = ScenePath;
	return true;
}

ACameraActor* FSequenceSerializer::SpawnCamera(UWorld* World, const FTransform& Transform)
{
	if (!World)
		return nullptr;

	ACameraActor* Camera = World->SpawnActor<ACameraActor>(NAME_None, &Transform);
	if (!Camera || !Camera->GetCameraComponent())
		return nullptr;

	Camera->SetFlags(EObjectFlags::RF_Transient);

	if (World->GetWorldType() == EWorldType::Editor)
	{
		UBillboardComponent* Icon = Camera->CreateDefaultSubobject<UBillboardComponent>("CameraIcon");
		Icon->SetupAttachment(Camera->GetCameraComponent());
		Icon->SetSprite(UAssetManager::Get().LoadTexture(CameraIconPath));
		Icon->SetHiddenInGame(true);
		Icon->RegisterComponent();
	}
	return Camera;
}

bool FSequenceSerializer::IsSequenceCamera(const ACameraActor* Camera)
{
	return Camera && Camera->HasAnyFlags(EObjectFlags::RF_Transient);
}

FString FSequenceSerializer::GetSequencePath(const FString& Name)
{
	return FString(SequenceDirectory) + "/" + Name + SequenceExtension;
}
