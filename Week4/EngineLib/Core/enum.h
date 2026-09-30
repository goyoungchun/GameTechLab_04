#pragma once

#include "Core.h"
#include <stdexcept>

enum class EAxis : int { X = 0, Y = 1, Z = 2 };

enum class EPrimitive
{
	EP_Sphere,
	EP_Cube,
	EP_Triangle,
	EP_GizmoArrow,
	EP_Circle,
	EP_BillboardQuad,
	EP_StaticMesh,
};

enum EGIZMO_AXIS //어떤축이 선택되었는지
{
	NONE,
	X,
	Y,
	Z
};

enum EGIZMO_TYPE {
	TRANSLATE,
	ROTATE,
	SCALE,
};

enum class EViewModeIndex : uint32
{
	VMI_Lit,
	VMI_Unlit,
	VMI_Wireframe,
};

enum class ERenderFlags : uint32
{
	RF_None = 0,
	RF_Raycastable = 1 << 0,
	RF_Primitive = 1 << 1,
	RF_Texture = 1 << 2,
	RF_Billboard = 1 << 3,
	RF_Text = 1 << 4,
	RF_WorldAxis = 1 << 5,
	RF_Gizmo = 1 << 6,
	RF_BoundingBox = 1 << 7,
	RF_Particle = 1 << 8,
};

constexpr ERenderFlags operator|(ERenderFlags lhs, ERenderFlags rhs)
{
	return static_cast<ERenderFlags>(static_cast<uint32>(lhs) | static_cast<uint32>(rhs));
}

constexpr ERenderFlags operator&(ERenderFlags lhs, ERenderFlags rhs)
{
	return static_cast<ERenderFlags>(static_cast<uint32>(lhs) & static_cast<uint32>(rhs));
}

constexpr ERenderFlags operator~(ERenderFlags flag)
{
	return static_cast<ERenderFlags>(~static_cast<uint32>(flag));
}

[[nodiscard]]
constexpr bool HasAllRenderFlags(ERenderFlags flags, ERenderFlags required)
{
	return (flags & required) == required;
}

[[nodiscard]]
constexpr bool HasAnyRenderFlags(ERenderFlags flags, ERenderFlags required)
{
	return (flags & required) != ERenderFlags::RF_None;
}

enum class EEngineShowFlags : uint32
{
	SF_Primitives = 1 << 0,
	SF_BillboardText = 1 << 1,
	SF_WorldAxis = 1 << 2,
	SF_BoundingBox = 1 << 3,
	SF_Grid = 1 << 4,
};

constexpr EEngineShowFlags operator|(EEngineShowFlags lhs, EEngineShowFlags rhs)
{
	return static_cast<EEngineShowFlags>(static_cast<uint32>(lhs) | static_cast<uint32>(rhs));
}

constexpr EEngineShowFlags operator&(EEngineShowFlags lhs, EEngineShowFlags rhs)
{
	return static_cast<EEngineShowFlags>(static_cast<uint32>(lhs) & static_cast<uint32>(rhs));
}

inline EPrimitive StringToEPrimitive(const char* str)
{
	if (strcmp(str, "Sphere") == 0)
	{
		return EPrimitive::EP_Sphere;
	}
	else if (strcmp(str, "Cube") == 0)
	{
		return EPrimitive::EP_Cube;
	}
	else if (strcmp(str, "Triangle") == 0)
	{
		return EPrimitive::EP_Triangle;
	}
	else if (strcmp(str, "GizmoArrow") == 0)
	{
		return EPrimitive::EP_GizmoArrow;
	}
	else if (strcmp(str, "Circle") == 0)
	{
		return EPrimitive::EP_Circle;
	}
	else if (strcmp(str, "BillboardQuad") == 0)
	{
		return EPrimitive::EP_BillboardQuad;
	}
	else if (strcmp(str, "StaticMesh") == 0)
	{
		return EPrimitive::EP_StaticMesh;
	}
	else
	{
		throw std::runtime_error("Unknown EPrimitive value");
	}
}

inline const char* PrimitiveToString(EPrimitive primitiveType)
{
	switch (primitiveType)
	{
	case EPrimitive::EP_Cube:
		return "Cube";

	case EPrimitive::EP_Sphere:
		return "Sphere";

	case EPrimitive::EP_Triangle:
		return "Triangle";

	case EPrimitive::EP_GizmoArrow:
		return "GizmoArrow";

	case EPrimitive::EP_Circle:
		return "Circle";

	case EPrimitive::EP_BillboardQuad:
		return "BillboardQuad";

	case EPrimitive::EP_StaticMesh:
		return "StaticMesh";

	default:
		return "Unknown";
	}
}

enum ESamplerStateType
{
	SST_Default,
	SST_Clamp,
	SST_Wrap,
	SST_Count,
};

enum EDepthStencilStateType
{
	DSS_Default,
	DSS_NoWrite,

	// For hightlighting selected object
	DSS_StencilMark,
	DSS_StencilOutline,
	DSS_Count,
};

enum ERasterizerStateType
{
	RST_Default,
	RST_Wireframe,
	RST_Count,
};

enum EBlendStateType
{
	BST_Default,
	BST_AlphaBlend,
	BST_Additive,
	BST_NoColorWrite,
	BST_Count,
};

enum EContantBufferType
{
	CBT_Simple,
	CBT_Texture,
	CBT_BillboardTexture,
	CBT_Font,
	CBT_UnicodeFont,
	CBT_Particle,
	CBT_HighlightMask,
	CBT_HighlightOutline,
	CBT_Count,
};

enum EVertexShaderType
{
	VST_Simple,
	VST_Line,
	VST_Texture,
	VST_Instanced,
	VST_Font,
	VST_Billboard,
	VST_Particle,
	VST_StaticMesh,
	VST_LoadingScreen,
	VST_HighlightMask,
	VST_HighlightOutline,
	VST_Count,
};

enum EPixelShaderType
{
	PST_Simple,
	PST_Line,
	PST_Texture,
	PST_Font,
	PST_UnicodeFont,
	PST_Billboard,
	PST_Particle,
	PST_StaticMesh,
	PST_LoadingScreen,
	PST_HighlightMask,
	PST_HighlightOutline,
	PST_Count,
};

enum EInputLayoutType
{
	ILT_Position,
	ILT_PositionColor,
	ILT_PositionTexture,
	ILT_PositionColorMatrixTint,
	ILT_PositionNormalColorTexture,
	ILT_Count,
};
