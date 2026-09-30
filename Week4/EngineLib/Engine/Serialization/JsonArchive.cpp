#include "JsonArchive.h"

#include <memory>
#include <stdexcept>

#include "Core/Object/ObjectFactory.h"
#include "Engine/EngineStatics.h"
#include "Engine/World.h"

#include "../../Rendering/Camera.h"
#include "../../Engine/Serialization/PropertyJson.h"

namespace
{
	constexpr uint32 SceneFormatVersion = 0;
}

json::JSON FJsonArchive::SerializeWorld(const UWorld& world, const FCamera* perspectiveCamera)
{
	json::JSON worldJson = json::JSON::Make(json::JSON::Class::Object);
	world.SerializeClass(worldJson);

	json::JSON archiveJson = json::JSON::Make(json::JSON::Class::Object);

	archiveJson["Version"] = SceneFormatVersion;
	archiveJson["NextUUID"] = UEngineStatics::GetNextUUID();
	archiveJson["World"] = std::move(worldJson);

	if (perspectiveCamera != nullptr)
	{
		json::JSON cameraJson = json::JSON::Make(json::JSON::Class::Object);

		TPropertyJsonSerializer<FVector>::Serialize(cameraJson,"Location",perspectiveCamera->Location);

		TPropertyJsonSerializer<FRotator>::Serialize(cameraJson,"Rotation",perspectiveCamera->Rotation);

		TPropertyJsonSerializer<float>::Serialize(cameraJson,"FOV",perspectiveCamera->mFovDegree);

		TPropertyJsonSerializer<float>::Serialize(cameraJson, "NearZ", perspectiveCamera->mNearZ);

		TPropertyJsonSerializer<float>::Serialize(cameraJson, "FarZ", perspectiveCamera->mFarZ);

		json::JSON editorStateJson = json::JSON::Make(json::JSON::Class::Object);

		editorStateJson["PerspectiveCamera"] = std::move(cameraJson);

		archiveJson["EditorState"] = std::move(editorStateJson);
	}



	return archiveJson;
}

UWorld* FJsonArchive::DeserializeWorld(const json::JSON& inJson)
{
	if (inJson.JSONType() != json::JSON::Class::Object)
	{
		throw std::runtime_error("Scene archive requires a JSON object");
	}

	if (!inJson.hasKey("Version") ||
		inJson.at("Version").JSONType() != json::JSON::Class::Integral)
	{
		throw std::runtime_error("Scene archive requires an integral Version");
	}

	const long version = inJson.at("Version").ToInt();
	if (version != SceneFormatVersion)
	{
		throw std::runtime_error("Unsupported scene archive version");
	}

	if (!inJson.hasKey("NextUUID") ||
		inJson.at("NextUUID").JSONType() != json::JSON::Class::Integral)
	{
		throw std::runtime_error("Scene archive requires an integral NextUUID");
	}

	const long nextUUID = inJson.at("NextUUID").ToInt();
	if (nextUUID < 0)
	{
		throw std::runtime_error("Scene archive NextUUID cannot be negative");
	}

	if (!inJson.hasKey("World") ||
		inJson.at("World").JSONType() != json::JSON::Class::Object)
	{
		throw std::runtime_error("Scene archive requires a World object");
	}

	const json::JSON& worldJson = inJson.at("World");
	if (!worldJson.hasKey("ClassName")
		|| worldJson.at("ClassName").JSONType() != json::JSON::Class::String
		|| FString(worldJson.at("ClassName").ToString()) != UWorld::GetClass()->Name)
	{
		throw std::runtime_error("Scene archive World must have class UWorld");
	}

	std::unique_ptr<UWorld> newWorld(FObjectFactory::ConstructUnInitializedObject<UWorld>());
	if (!newWorld)
	{
		throw std::runtime_error("Failed to create UWorld");
	}

	newWorld->DeserializeClass(worldJson);
	UEngineStatics::SetNextUUID(static_cast<uint32>(nextUUID));

	return newWorld.release();
}

bool FJsonArchive::DeserializePerspectiveCamera(const json::JSON& inJson, FCamera& outCamera)
{
	if (!inJson.hasKey("EditorState"))
	{
		return false;
	}

	const json::JSON& editorStateJson = inJson.at("EditorState");

	if (editorStateJson.JSONType() !=json::JSON::Class::Object)
	{
		throw std::runtime_error("EditorState requires an object");
	}

	if (!editorStateJson.hasKey("PerspectiveCamera"))
	{
		return false;
	}

	const json::JSON& cameraJson = editorStateJson.at("PerspectiveCamera");

	if (cameraJson.JSONType() != json::JSON::Class::Object)
	{
		throw std::runtime_error("PerspectiveCamera requires an object");
	}

	TPropertyJsonSerializer<FVector>::Deserialize(cameraJson,"Location",outCamera.Location);

	TPropertyJsonSerializer<FRotator>::Deserialize(cameraJson,"Rotation",outCamera.Rotation);

	TPropertyJsonSerializer<float>::Deserialize(cameraJson,"FOV",outCamera.mFovDegree);

	outCamera.mFovDegree = FMath::Clamp(outCamera.mFovDegree,5.0f,175.0f);

	float nearZ = outCamera.mNearZ;
	float farZ = outCamera.mFarZ;

	if (cameraJson.hasKey("NearZ"))
	{
		TPropertyJsonSerializer<float>::Deserialize(cameraJson,"NearZ",nearZ);
	}

	if (cameraJson.hasKey("FarZ"))
	{
		TPropertyJsonSerializer<float>::Deserialize(cameraJson,"FarZ",farZ);
	}

	if (std::isfinite(nearZ) &&std::isfinite(farZ) &&nearZ > 0.0f &&farZ > nearZ)
	{
		outCamera.mNearZ = nearZ;
		outCamera.mFarZ = farZ;
	}

	return true;
}
