#pragma once

#include <sol/sol.hpp>

// Lua는 카메라에게 "언제, 무엇을 볼지"만 요청한다. 매 프레임 계산은 C++ 카메라 시스템이 맡는다
void RegisterCameraBindings(sol::state& Lua);
