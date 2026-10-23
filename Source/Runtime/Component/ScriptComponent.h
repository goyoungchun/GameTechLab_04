#pragma once

#include <shellapi.h>
#include <tchar.h>
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
	sol::environment Environment;
	sol::safe_function LuaBeginPlay, LuaTick, LuaEndPlay, LuaOnOverlap, LuaOnHit;


	constexpr static FStringView  TemplatePath = "Assets/Scripts/Template/template.lua";
	constexpr static FStringView  ScriptFolderPath = "Assets/Scripts/";
};