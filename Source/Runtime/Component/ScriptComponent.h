#pragma once

#include <shellapi.h>
#include <tchar.h>
#include <chrono>
#include "ActorComponent.h"
#include "SceneComponent.h"
#include "Script/ScriptSystem.h"
#include "Container/StringView.h"

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

	void Initialize();

	void SetScriptPath(FString InScriptPath);
	FString GetScriptPath();
	const FStringView GetTemplateScriptPath();
	const FStringView GetScriptFolderPath();

private:
	FString ScriptPath;
	float HotReloadCheckTimer = 0.f;
	std::filesystem::file_time_type LastFileWriteTime;
	sol::environment Environment;
	sol::safe_function LuaBeginPlay, LuaTick, LuaEndPlay, LuaOnOverlap, LuaOnHit;


	bool CheckChangeScriptFile(float DeltaTime);
	bool HotReloadFile();

	constexpr static FStringView  TemplatePath = "Assets/Scripts/Template/template.lua";
	constexpr static FStringView  ScriptFolderPath = "Assets/Scripts/";
};