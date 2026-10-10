#pragma once
#include <algorithm>
#include <cmath>
namespace Engine::Renderer {
// Camera contract shared by lighting and post effects. Positive EC brightens.
inline float ManualExposure(float ev100, float compensation=0) noexcept {
    if (!std::isfinite(ev100) || !std::isfinite(compensation)) return 0;
    return std::exp2(std::clamp(compensation-ev100,-126.f,126.f))/1.2f;
}
}
