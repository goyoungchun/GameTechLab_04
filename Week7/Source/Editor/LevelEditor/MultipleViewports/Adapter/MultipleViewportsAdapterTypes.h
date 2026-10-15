#pragma once
#include "Editor/LevelEditor/MultipleViewports/Core/MultipleViewports.h"

// Quad View 사이에서 렌더링과 입력이 비워지는 Splitter gutter의 화면 픽셀 폭이다.
inline constexpr float SplitterThickness = 6.0f;

// View별 UI의 투영·축 정렬 프리셋이며 기존 콤보박스 순서를 유지한다.
enum class EMultipleViewportsCameraPreset
{
    Perspective, OrthographicView, Top, Bottom, Front, Back, Left, Right
};

//뷰포트 별로 쉐이딩 상태를 나타낸다
//lit은 빛을 받은, unlit은 빛을 안 받은이라는 뜻을 가진다.
// 렌더러의 ERenderViewMode와 값 순서가 같아야 한다 (EditorEngine.cpp에서 static_assert로 확인)
enum class EMultipleViewPortsViewMode : int
{
    UnLit, Lit_Gouraud, Lit_Lambert, Lit_Phong, World_Normal
};

// GridRenderer가 소유하는 기존 평면 enum을 재정의 없이 참조한다.
enum class EGridPlane : int32;
