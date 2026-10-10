#pragma once
#include <array>
namespace Engine {
// Engine-owned value math types; copyable across modules and threads.
struct Vec3 { float x=0,y=0,z=0; bool operator==(const Vec3&) const = default; };
struct Mat4 { std::array<float,16> values{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; };
}
