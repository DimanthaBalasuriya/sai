#pragma once
#include "engine/core/Math.hpp"
#include <cstddef>
#include <cstdint>
namespace Engine::Renderer {
// Value ABI shared by raster lighting and future ray tracing. No World pointers.
struct alignas(16) GpuLight {
    Vec3 position; float invRangeSq=0;
    Vec3 direction{0,-1,0}; std::uint32_t type=0;
    Vec3 color; std::uint32_t shadowIndex=UINT32_MAX;
    float spotScale=0,spotOffset=0;
    std::uint32_t flags=0;float padding=0;
};
inline constexpr std::uint32_t kLightEnabled=1,kLightCastShadows=2,kLightLegacyLocal=4;
static_assert(sizeof(GpuLight)==64 && alignof(GpuLight)==16);
static_assert(offsetof(GpuLight,direction)==16 && offsetof(GpuLight,color)==32 && offsetof(GpuLight,flags)==56);
}
