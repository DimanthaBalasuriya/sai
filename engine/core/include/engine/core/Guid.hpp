#pragma once
#include <cstdint>
namespace Engine {
// Persistent identity; independent of paths and ECS storage slots.
struct Guid {
    std::uint64_t high=0,low=0;
    explicit operator bool() const noexcept { return high || low; }
    bool operator==(const Guid&) const = default;
};
inline constexpr Guid kBuiltinCube{0x454e47494e45,1};
inline constexpr Guid kBuiltinMaterial{0x454e47494e45,2};
}
