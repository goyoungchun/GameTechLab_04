#pragma once
#include <sol/sol.hpp>

#include "Core/Types.h"
#include "Core/EngineLog.h"
#include "Math/Vector.h"
#include "GameFramework/Actor.h"

class FScriptSystem
{
public:
	static FScriptSystem& Get()
	{
		static FScriptSystem* Instance = new FScriptSystem();
		return *Instance;
	}

	FScriptSystem();
	~FScriptSystem() = default;

	sol::state& GetLua();
	
	void Shutdown();
	
private:
	sol::state Lua;

	void Init();
	void OpenLibraries();
	void OverridePrint();
	void RegisterTypes();



};