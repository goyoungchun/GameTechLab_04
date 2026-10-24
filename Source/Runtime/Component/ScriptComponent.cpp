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

bool UScriptComponent::LoadScriptFile(sol::environment InEnvironment)
{
	if (ScriptPath.empty())
	{
		return false;
	}
	sol::state& Lua = FScriptSystem::Get().GetLua();
	InEnvironment = sol::environment(Lua, sol::create, Lua.globals());
	InEnvironment["obj"] = GetOwner();

	if (!ResolveScriptPath())
	{
		return false;
	}

	auto Result = Lua.safe_script_file(ScriptPath, InEnvironment, sol::script_pass_on_error);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Script] Load failed: {} ({})", ScriptPath, Err.what());
		return false;
	}

	LuaBeginPlay = InEnvironment["BeginPlay"];
	LuaTick = InEnvironment["Tick"];
	LuaEndPlay = InEnvironment["EndPlay"];
	LuaOnOverlap = InEnvironment["OnOverlap"];
	LuaOnHit = InEnvironment["OnHit"];

	std::error_code ErrorCode;
	LastFileWriteTime = fs::last_write_time(ScriptPath, ErrorCode);
	if (ErrorCode)
	{
		HTR_LOG(Error, "[Script] Last Write Time Check failed: {} ({})", ScriptPath, ErrorCode.message());
	}

	Environment = InEnvironment;

	Lua.collect_garbage();
	return true;
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();

	sol::environment NewEnvironment;
	LoadScriptFile(NewEnvironment);

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

namespace 
{
	bool EqualsIgnoreCase(const FString& A, const FString& B)
	{
		uint64 Index = 0;
		for (; Index < A.size() && Index < B.size(); ++Index)
		{
			const int Left = std::tolower(static_cast<unsigned char>(A[Index]));
			const int Right = std::tolower(static_cast<unsigned char>(B[Index]));
			if (Left != Right)
				return false;
		}
		return Index == A.size() && Index == B.size();
	}
}


bool UScriptComponent::FindScriptByFileName(fs::path FileName)
{
	std::error_code ErrorCode;
	bool FindScript = false;

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(
		ScriptFolderPath,
		fs::directory_options::skip_permission_denied,
		ErrorCode
	))
	{
		if (Entry.is_regular_file())
		{
			fs::path Path = Entry.path().generic_string();
			if (Path.extension() == ".lua" && EqualsIgnoreCase(Path.filename().generic_string(), FileName.generic_string()))
			{
				if (!FindScript)
				{
					FString FolderPath(ScriptFolderPath);
					SetScriptPath(FolderPath + fs::relative(Path, ScriptFolderPath, ErrorCode).generic_string());
					FindScript = true;
				}
				else
				{
					HTR_LOG(Warning, "[Script] Duplicate script name '{}': using '{}', ignored '{}'", 
						FileName.generic_string(), ScriptPath, Path.generic_string());
					return FindScript;
				}
			}
		}
	}
	return FindScript;
}

bool UScriptComponent::ResolveScriptPath()
{ 
	std::error_code ErrorCode;
	fs::path Path = ScriptPath;

	if(Path.empty())
	{
		return false;
	}


	if (!fs::exists(Path, ErrorCode) || ErrorCode)
	{
		if (bScriptNotFound || !FindScriptByFileName(Path.filename()))
		{
			bScriptNotFound = true;
			return false;
		}
	}
	bScriptNotFound = false;
	return true;
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

	if (!ResolveScriptPath())
	{
		return false;
	}

	uintmax_t FileSize = fs::file_size(ScriptPath, ErrorCode);
	if (ErrorCode || FileSize == 0)
	{
		return false;
	}

	fs::file_time_type CurrentFileWriteTime = 
		fs::last_write_time(ScriptPath, ErrorCode);

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
	sol::environment NewEnvironment;

	if (LoadScriptFile(NewEnvironment))
	{
		return true;
	}
	return false;
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
	bScriptNotFound = false;
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
