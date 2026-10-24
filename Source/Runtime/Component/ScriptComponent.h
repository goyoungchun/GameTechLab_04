#pragma once

#include <shellapi.h>
#include <tchar.h>
#include <chrono>
#include "ActorComponent.h"
#include "SceneComponent.h"
#include "Script/ScriptSystem.h"
#include "Container/StringView.h"
#include "Script/CoroutineTypes.h"

class UScriptComponent : public UActorComponent
{
	DECLARE_CLASS(UScriptComponent, UActorComponent)
	REFLECT_START(ClassName)
	PROPERTY_READONLY(ScriptPath)
	REFLECT_END()
public:
	UScriptComponent();
	virtual ~UScriptComponent() override;

	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	void OnHit(AActor* InOtherActor);
	void OnOverlap(AActor* InOtherActor);
		
	bool LoadScriptFile(sol::environment InEnvironment);
	bool ResolveScriptPath();

	void SetScriptPath(FString InScriptPath);
	FString GetScriptPath();
	const FStringView GetTemplateScriptPath();
	const FStringView GetScriptFolderPath();

private:
	void BindScriptAPI(sol::environment& InEnvironment);
	void BindCoroutineAPI(sol::environment& InEnvironment);
	void UnbindScriptAPI(sol::environment& InEnvironment);
	void UnbindCoroutineAPI(sol::environment& InEnvironment);
	void CacheScriptCallbacks(sol::environment& Env);
	void ClearScriptCallbacks();
	void ClearCoroutines();

	template<typename... Args>
	void ResumeRecord(FCoroutineRecord& Record, Args&&... InArgs);

	sol::optional<uint32> StartCoroutine(sol::protected_function Fn, sol::variadic_args Args);
	void HandleYieldedResult(sol::protected_function_result& Result, FCoroutineRecord& Record);
	void UpdateCoroutines(float DeltaTime);
	void StopRecord(FCoroutineRecord& Record);
	void StopAllRecords();

	FCoroutineRecord* FindRecord(uint32 Id);
	bool IsRecordReady(FCoroutineRecord& Record, float DeltaTime);

private:
	FString	ScriptPath;
	float	HotReloadCheckTimer = 0.f;
	bool	bScriptNotFound = false;
	fs::file_time_type LastFileWriteTime;
	sol::environment Environment;
	sol::safe_function LuaBeginPlay, LuaTick, LuaEndPlay, LuaOnOverlap, LuaOnHit;

	TArray<TUniquePtr<FCoroutineRecord>> CoroutineRecords;
	uint32 NextCoroutineId = 1;
	int32 FrameCounter = 0;
	bool bAcceptCoroutines = false;

	bool CheckChangeScriptFile(float DeltaTime);
	bool HotReloadFile();
	bool FindScriptByFileName(std::filesystem::path FileName);
	
	constexpr static FStringView  TemplatePath = "Assets/Scripts/Template/template.lua";
	constexpr static FStringView  ScriptFolderPath = "Assets/Scripts/";
};