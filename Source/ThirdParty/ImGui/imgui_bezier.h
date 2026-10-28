#pragma once

namespace ImGui
{
    int   Bezier(const char* label, float P[5]);
    float BezierValue(float dt01, float P[4]);
    void  ShowBezierDemo();
}
