#pragma once

#include "UEditorWindow.h"
#include "../../../Container/FString.h"
#include "../../../Container/TDeque.h"
#include "../../FConsole.h"
#include "../../Core.h"
#include "ImGui/imgui.h"
#include "../../../Matrix.h"

class UConsoleWindow : public UEditorWindow
{
    UCLASS(UConsoleWindow, "ConsoleWindow", UEditorWindow)

private:
	ImGuiTextFilter Filter;
	TDeque<FString> logs;
public:
	void AddDebugText(FString DebugText);
	void AddDebugError(FString ErrorText);
	void Clear();
	void Copy();
	void Option();

	void Render(float DeltaTime) override;
};

