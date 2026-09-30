// EngineLib/Core/BuiltinAssets.h

#pragma once

#include "Core/Name.h"

namespace BuiltinAssets
{
	/* Mesh */
	/* Static Mesh*/
	inline const FName CubeMesh{ "Engine/Primitives/CubeMesh" };
	inline const FName SphereMesh{ "Engine/Primitives/SphereMesh" };

	/* Simple */
	inline const FName CubeSimple{ "Engine/Primitives/CubeSimple" };
	inline const FName GizmoArrow{ "Engine/Primitives/GizmoArrow" };
	inline const FName Circle{ "Engine/Primitives/Circle" };
	inline const FName Triangle{ "Engine/Primitives/Triangle" };
	inline const FName BillboardQuad{ "Engine/Primitives/BillboardQuad" };
	inline const FName BillboardQuadTextured{ "Engine/Primitives/BillboardQuadTextured" };
	inline const FName LoadingScreenQuad{ "Engine/Primitives/LoadingScreenQuad" };

	/* Material */
	inline const FName DefaultMaterial{ "Engine/Materials/DefaultMaterial" };
	inline const FName CubeMaterial{ "Engine/Materials/CubeMaterial" };
	inline const FName SphereMaterial{ "Engine/Materials/SphereMaterial" };

	/* Texture */
	inline const FName EnglishFontAtlas{ "Assets/Fonts/EnglishBigFontAtlas.dds" };
	inline const FName KoreanFontAtlas{ "Assets/Fonts/KoreanFullAtlas.png" };

	inline const FName LoadingScreenTexture{ "Assets/Textures/LoadingScreen.dds" };
	inline const FName DefaultWhiteTexture{ "Assets/Textures/DefaultWhiteTexture" };
	//inline const FName ExplosionTexture{ "Assets/Textures/Explosion_Alpha.png" };
	//inline const FName CubeTexture{ "Assets/Textures/CubeTexture.png" };
	//inline const FName EarthTexture{ "Assets/Textures/EarthTexture.png" };
}
