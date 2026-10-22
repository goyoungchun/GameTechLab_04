#pragma once

#include <imgui.h>
#include <Windows.h>

#include <d3d11.h>

class FImGuiRenderer
{
public:
	FImGuiRenderer() = default;
	~FImGuiRenderer();

	bool Init(HWND WindowHandle, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
	void Begin();
	void End();
	void Shutdown();
	void UpdateScale(float Scale);
	void UpdateDPI(float DPI);
private:
	void ApplyStyleScale();

private:
	HWND MainWindow = nullptr;
	float DPI = 1.0f;
	float Scale = 1.0f;
	ImGuiStyle OriginalStyle;
};