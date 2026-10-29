#pragma once

enum class EShaderType
{
	Vertex,
	Pixel,
	Compute,
};

enum class EViewportMode : uint8
{
	Unlit,
	Lit,
	Wireframe,
	SceneDepth,
	WorldNormal,
	None
};

enum class EShadingModel : uint8
{
	Lambert,
	Phong,
	BlinnPhong,
};

enum class EShadowFilterMode : uint32
{
	Hard,
	PCF3x3,
	PCF5x5,
	Poisson,
	PCSS,
};