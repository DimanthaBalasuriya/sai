#pragma once
#include "engine/scene/World.hpp"
namespace Engine::Scene {
inline constexpr ComponentType kEntityFlags=static_cast<ComponentType>(8192);
inline constexpr ComponentType kPropertyInheritance=static_cast<ComponentType>(8193);
// Flags: visible, isStatic, inheritVisible, inheritStatic (unsigned booleans).
// Inheritance: meshColor, lightColor, lightIntensity (unsigned booleans).
Result RegisterEntityFeatures();
}
