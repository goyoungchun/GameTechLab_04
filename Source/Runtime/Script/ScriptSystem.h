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
	void Init();
	sol::state& GetLua();
	
	void Shutdown();
	
private:
	bool bIsInitialized = false;
	TUniquePtr<sol::state> Lua;
	void OpenLibraries();
	void OverridePrint();
	void RegisterTypes();
	void RegisterInput();
	void RegisterFunction();


};