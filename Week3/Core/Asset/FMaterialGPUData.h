#pragma once

#include <array>
#include <cstddef>

constexpr uint32 MATERIAL_GPU_STRIDE = 128;

struct FMaterialGPUSlot {
    std::array<std::byte, MATERIAL_GPU_STRIDE> Data{};
};

static_assert(sizeof(FMaterialGPUSlot) == MATERIAL_GPU_STRIDE);