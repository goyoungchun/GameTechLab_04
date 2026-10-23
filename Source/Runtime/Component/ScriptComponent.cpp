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

	if (!LuaBeginPlay.valid())
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
	Environment = sol::lua_nil;
	Super::EndPlay();
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

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
	if (!LuaOnHit.valid())
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
	if (!LuaOnOverlap.valid())
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
