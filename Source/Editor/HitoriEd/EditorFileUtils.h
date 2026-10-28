#pragma once

#include "Engine/World.h"
#include "Container/StringView.h"

class FEditorFileUtils
{
public:
	static bool NewScene(UWorld* World);
	static bool SaveScene(UWorld* World);
	static bool SaveSceneAs(UWorld* World);
	static bool LoadScene(UWorld* World);
	static FString GetScenePath(UWorld* World);
	static const FString& GetCurrentScenePath() { return CurrentScenePath; }

	static FString OpenSaveSequenceDialog();
	static FString OpenLoadSequenceDialog();

private:
	static FString OpenSaveSceneDialog();
	static FString OpenLoadSceneDialog();

	static FString CurrentScenePath;
};

