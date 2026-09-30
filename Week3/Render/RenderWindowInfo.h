#pragma once 
#include <d3d11.h>

struct RenderWindowInfo {
	UINT ScreenWidth{ 0 };
	UINT ScreenHeight{ 0 };
	D3D11_VIEWPORT Viewport{};
};