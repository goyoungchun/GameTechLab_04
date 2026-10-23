#include "EnginePCH.h"
#include "ScriptComponent.h"
#include "GameFramework/Actor.h"

UScriptComponent::UScriptComponent()
{
	
}

UScriptComponent::~UScriptComponent()
{

}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
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
