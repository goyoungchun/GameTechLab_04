#pragma once

#include "Core/Core.h"
#include "ThirdParty/Json/json.hpp"

class UWorld;
class FCamera;

class FJsonArchive
{
public:
	static json::JSON SerializeWorld(const UWorld& world, const FCamera* perspectivecamera = nullptr);
	static UWorld* DeserializeWorld(const json::JSON& inJson);
	static bool DeserializePerspectiveCamera(const json::JSON& inJson, FCamera& outCamera);
};
