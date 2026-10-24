#include "EnginePCH.h"
#include "ScriptComponent.h"
#include "GameFramework/Actor.h"

UScriptComponent::UScriptComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UScriptComponent::~UScriptComponent()
{

}

void UScriptComponent::Initialize()
{
	if (ScriptPath.empty())
	{
		return;
	}
	sol::state& Lua = FScriptSystem::Get().GetLua();
	Environment = sol::environment(Lua, sol::create, Lua.globals());
	Environment["obj"] = GetOwner();

	auto Result = Lua.safe_script_file(ScriptPath, Environment, sol::script_pass_on_error);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] Load failed: {} ({})", ScriptPath, Err.what());
		return;
	}

	std::error_code ErrorCode;
	LastFileWriteTime = std::filesystem::last_write_time(ScriptPath, ErrorCode);
	if (ErrorCode)
	{
		HTR_LOG(Error, "[Script] Last Write Time Check failed: {} ({})", ScriptPath, ErrorCode.message());
		return;
	}

	LuaBeginPlay = Environment["BeginPlay"];
	LuaTick = Environment["Tick"];
	LuaEndPlay = Environment["EndPlay"];
	LuaOnOverlap = Environment["OnOverlap"];
	LuaOnHit = Environment["OnHit"];
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();
	Initialize();

	if (!LuaBeginPlay.valid() || GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}
	auto Result = LuaBeginPlay();
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaBeginPlay failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::EndPlay()
{	
	if (LuaEndPlay.valid())
	{
		auto Result = LuaEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			HTR_LOG(Error, "[Script] LuaEndPlay failed: {} ({})", ScriptPath, Err.what());
		}
	}
	LuaBeginPlay = sol::nil;
	LuaTick = sol::nil;
	LuaOnOverlap = sol::nil;
	LuaOnHit = sol::nil;
	LuaEndPlay = sol::nil;
	Environment = sol::nil;
	Super::EndPlay();
}

bool UScriptComponent::CheckChangeScriptFile(float DeltaTime)
{
	std::error_code ErrorCode;

	HotReloadCheckTimer += DeltaTime;
	if (HotReloadCheckTimer < 0.5f)
	{
		return false;
	}

	HotReloadCheckTimer = 0;

	if (!std::filesystem::exists(ScriptPath, ErrorCode) || ErrorCode)
	{
		return false;
	}

	uintmax_t FileSize = std::filesystem::file_size(ScriptPath, ErrorCode);
	if (ErrorCode || FileSize == 0)
	{
		return false;
	}

	std::filesystem::file_time_type CurrentFileWriteTime = 
		std::filesystem::last_write_time(ScriptPath, ErrorCode);

	if (ErrorCode)
	{
		HTR_LOG(Error, "[Script] Last Write Time Check failed: {} ({})", ScriptPath, ErrorCode.message());
		return false;
	}
	
	if (CurrentFileWriteTime != LastFileWriteTime)
	{
		return true;
	}
	return false;
}

bool UScriptComponent::HotReloadFile()
{
	sol::state &Lua = FScriptSystem::Get().GetLua();
	sol::load_result LoadResult; 
	std::error_code ErrorCode;
	sol::environment NewEnvironment;

	LastFileWriteTime = std::filesystem::last_write_time(ScriptPath, ErrorCode);
	if (ErrorCode)
	{
		HTR_LOG(Error, "[Script] Last Write Time Check failed: {} ({})", ScriptPath, ErrorCode.message());
		return false;
	}

	LoadResult = Lua.load_file(ScriptPath);
	if (!LoadResult.valid())
	{
		sol::error Err = LoadResult;
		HTR_LOG(Error, "[Script] HotReload failed: {} ({})", ScriptPath, Err.what());
		return false;
	}
	sol::protected_function Chunk = LoadResult;

	NewEnvironment = sol::environment(Lua, sol::create, Lua.globals());
	NewEnvironment["obj"] = GetOwner();

	NewEnvironment.set_on(Chunk);

	auto RunResult = Chunk();
	if (!RunResult.valid())
	{
		sol::error Err = RunResult;
		HTR_LOG(Error, "[Script] HotReload failed : {} ({})", ScriptPath, Err.what());
		return false;
	}

	Environment = NewEnvironment;

	Lua.collect_garbage();

	LuaBeginPlay = Environment["BeginPlay"];
	LuaTick = Environment["Tick"];
	LuaEndPlay = Environment["EndPlay"];
	LuaOnOverlap = Environment["OnOverlap"];
	LuaOnHit = Environment["OnHit"];

	return true;
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);


	if (GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}

	if (CheckChangeScriptFile(DeltaTime))
	{
		HotReloadFile();
	}

	if (!LuaTick.valid())
	{
		return;
	}
	auto Result = LuaTick(DeltaTime);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaTick failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::OnHit(AActor *InOtherActor)
{
	if (!LuaOnHit.valid() || GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}
	auto Result = LuaOnHit(InOtherActor);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaOnHit failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::OnOverlap(AActor* InOtherActor)
{
	if (!LuaOnOverlap.valid() || GetOwner()->IsActorBeingDestroyed())
	{
		return;
	}
	auto Result = LuaOnOverlap(InOtherActor);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] LuaOnOverlap failed: {} ({})", ScriptPath, Err.what());
		return;
	}
}

void UScriptComponent::SetScriptPath(FString InScriptPath)
{
	ScriptPath = InScriptPath;
}

FString UScriptComponent::GetScriptPath()
{
	return (ScriptPath);
}

const FStringView UScriptComponent::GetTemplateScriptPath()
{
	return (TemplatePath);
}

const FStringView UScriptComponent::GetScriptFolderPath()
{
	return (ScriptFolderPath);
}
