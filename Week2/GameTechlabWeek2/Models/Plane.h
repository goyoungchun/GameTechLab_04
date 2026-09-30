#pragma once

#include <cstdint>
#include "Engine/Renderer/FVertexSimple.h"

inline FVertexSimple plane_vertices[] = {
    { -1.000000f, -1.000000f, 0.000000f, 1.000000f, 1.000000f, 0.000000f, 0.000000f },
    { 1.000000f, -1.000000f, 0.000000f, 1.000000f, 0.000000f, 1.000000f, 0.000000f },
    { -1.000000f, 1.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 1.000000f },
    { 1.000000f, 1.000000f, 0.000000f, 1.000000f, 1.000000f, 0.000000f, 0.000000f },
};

inline uint32_t plane_indices[] = {
    0, 1, 3,
    0, 3, 2,
};